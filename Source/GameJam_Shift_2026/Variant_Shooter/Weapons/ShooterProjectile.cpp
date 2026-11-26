// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterProjectile.h"
#include "BulletTracer.h"
#include "Variant_Shooter/AI/ZombieEnemy.h"
#include "Components/SphereComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

AShooterProjectile::AShooterProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the collision component and assign it as the root
	RootComponent = CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Component"));

	CollisionComponent->SetSphereRadius(6.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	// create the projectile movement component. No need to attach it because it's not a Scene Component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));

	ProjectileMovement->InitialSpeed = 12000.0f;
	ProjectileMovement->MaxSpeed = 12000.0f;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	DeferredDestructionTime = 0.0f;

	// set the default damage type
	HitDamageType = UDamageType::StaticClass();
}

void AShooterProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Keep a reliable fallback start point; the firing weapon immediately replaces
	// this with the exact barrel location before the round can collide.
	TracerStartLocation = GetActorLocation();
	bHasTracerStartLocation = true;
	
	// ignore the pawn that shot this projectile
	CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);

	// Hide any test rubber/foam bullet mesh components added by BP_ShooterProjectile_Bullet
	// and disable bounce/gravity so rounds behave like clean high-velocity bullets
	if (!bExplodeOnHit)
	{
		if (ProjectileMovement)
		{
			ProjectileMovement->bShouldBounce = false;
			ProjectileMovement->ProjectileGravityScale = 0.0f;
			ProjectileMovement->InitialSpeed = 12000.0f;
			ProjectileMovement->MaxSpeed = 12000.0f;
			ProjectileMovement->Velocity = GetActorForwardVector() * 12000.0f;
		}
		DeferredDestructionTime = 0.0f;

		TArray<UMeshComponent*> MeshComps;
		GetComponents<UMeshComponent>(MeshComps);
		for (UMeshComponent* MC : MeshComps)
		{
			if (MC)
			{
				MC->SetVisibility(false, true);
				MC->SetHiddenInGame(true, true);
				MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}

void AShooterProjectile::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the destruction timer
	GetWorld()->GetTimerManager().ClearTimer(DestructionTimer);
}

void AShooterProjectile::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	// ignore if we've already hit something else
	if (bHit)
	{
		return;
	}

	bHit = true;

	// disable collision on the projectile
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// make AI perception noise
	MakeNoise(NoiseLoudness, GetInstigator(), GetActorLocation(), NoiseRange, NoiseTag);

	if (bExplodeOnHit)
	{
		
		// apply explosion damage centered on the projectile
		ExplosionCheck(GetActorLocation());

	} else {

		// single hit projectile. Process the collided actor
		ProcessHit(Other, OtherComp, Hit.ImpactPoint, -Hit.ImpactNormal);

		// The visual trail always terminates on the projectile's real collision point,
		// including aim spread, instead of the camera's original aim trace.
		if (!bExplodeOnHit)
		{
			const FVector TrailStart = bHasTracerStartLocation ? TracerStartLocation : GetActorLocation();
			ABulletTracer::SpawnTracer(GetWorld(), TrailStart, HitLocation, Hit.ImpactNormal);
		}

	}

	// pass control to BP for any extra effects
	BP_OnProjectileHit(Hit);

	// check if we should schedule deferred destruction of the projectile
	if (DeferredDestructionTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(DestructionTimer, this, &AShooterProjectile::OnDeferredDestruction, DeferredDestructionTime, false);

	} else {

		// destroy the projectile right away
		Destroy();
	}
}

void AShooterProjectile::ExplosionCheck(const FVector& ExplosionCenter)
{
	// do a sphere overlap check look for nearby actors to damage
	TArray<FOverlapResult> Overlaps;

	FCollisionShape OverlapShape;
	OverlapShape.SetSphere(ExplosionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (!bDamageOwner)
	{
		QueryParams.AddIgnoredActor(GetInstigator());
	}

	GetWorld()->OverlapMultiByObjectType(Overlaps, ExplosionCenter, FQuat::Identity, ObjectParams, OverlapShape, QueryParams);

	TArray<AActor*> DamagedActors;

	// process the overlap results
	for (const FOverlapResult& CurrentOverlap : Overlaps)
	{
		// overlaps may return the same actor multiple times per each component overlapped
		// ensure we only damage each actor once by adding it to a damaged list
		if (DamagedActors.Find(CurrentOverlap.GetActor()) == INDEX_NONE)
		{
			DamagedActors.Add(CurrentOverlap.GetActor());

			// apply physics force away from the explosion
			const FVector& ExplosionDir = CurrentOverlap.GetActor()->GetActorLocation() - GetActorLocation();

			// push and/or damage the overlapped actor
			ProcessHit(CurrentOverlap.GetActor(), CurrentOverlap.GetComponent(), GetActorLocation(), ExplosionDir.GetSafeNormal());
		}
			
	}
}

void AShooterProjectile::ProcessHit(AActor* HitActor, UPrimitiveComponent* HitComp, const FVector& HitLocation, const FVector& HitDirection)
{
	// have we hit a character?
	if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
	{
		// ignore the owner of this projectile
		if (HitCharacter != GetOwner() || bDamageOwner)
		{
			// If we hit a Zombie, render Tom Looman 3D sphere-mask character wound & arterial blood splatter at the exact impact point
			if (AZombieEnemy* HitZombie = Cast<AZombieEnemy>(HitCharacter))
			{
				HitZombie->ApplyBulletWound(HitLocation, HitDirection);
			}

			// apply damage to the character
			AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;
			UGameplayStatics::ApplyDamage(HitCharacter, HitDamage, InstigatorController, this, HitDamageType);
		}
	}

	// have we hit a physics object?
	if (HitComp && HitComp->IsSimulatingPhysics())
	{
		// give some physics impulse to the object
		HitComp->AddImpulseAtLocation(HitDirection * PhysicsForce, HitLocation);
	}
}

void AShooterProjectile::SetTracerStartLocation(const FVector& MuzzleLocation)
{
	TracerStartLocation = MuzzleLocation;
	bHasTracerStartLocation = true;
}

void AShooterProjectile::OnDeferredDestruction()
{
	// destroy this actor
	Destroy();
}
