// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Traps/LandmineActor.h"
#include "Variant_Shooter/AI/ZombieEnemy.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ALandmineActor::ALandmineActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* MineRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MineRoot"));
	RootComponent = MineRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MineMaterialFinder(TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMaterialFinder(TEXT("/Game/Dev_Blueprint/VFX/Materials/M_BulletTracerCore.M_BulletTracerCore"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ExplosionSoundFinder(TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"));

	MineBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MineBody"));
	MineBody->SetupAttachment(MineRoot);
	MineBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MineBody->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	MineBody->SetCanEverAffectNavigation(false);
	MineBody->SetCastShadow(true);
	MineBody->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	MineBody->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.16f));
	if (CylinderFinder.Succeeded())
	{
		MineBody->SetStaticMesh(CylinderFinder.Object);
	}

	PressurePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PressurePlate"));
	PressurePlate->SetupAttachment(MineRoot);
	PressurePlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PressurePlate->SetCastShadow(false);
	PressurePlate->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	PressurePlate->SetRelativeScale3D(FVector(0.38f, 0.38f, 0.055f));
	if (CylinderFinder.Succeeded())
	{
		PressurePlate->SetStaticMesh(CylinderFinder.Object);
	}

	WarningLightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningLightMesh"));
	WarningLightMesh->SetupAttachment(MineRoot);
	WarningLightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WarningLightMesh->SetCastShadow(false);
	WarningLightMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 27.0f));
	WarningLightMesh->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.07f));
	if (SphereFinder.Succeeded())
	{
		WarningLightMesh->SetStaticMesh(SphereFinder.Object);
	}

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(MineRoot);
	TriggerSphere->SetSphereRadius(TriggerRadius);
	TriggerSphere->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TriggerSphere->SetCollisionObjectType(ECC_Pawn);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerSphere->SetGenerateOverlapEvents(true);
	TriggerSphere->SetCanEverAffectNavigation(false);
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ALandmineActor::OnTriggerBeginOverlap);

	ExplosionFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionFlash"));
	ExplosionFlash->SetupAttachment(MineRoot);
	ExplosionFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExplosionFlash->SetCastShadow(false);
	ExplosionFlash->SetReceivesDecals(false);
	ExplosionFlash->SetTranslucentSortPriority(10);
	ExplosionFlash->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
	ExplosionFlash->SetRelativeScale3D(FVector(0.12f));
	ExplosionFlash->SetVisibility(false);
	if (SphereFinder.Succeeded())
	{
		ExplosionFlash->SetStaticMesh(SphereFinder.Object);
	}

	ExplosionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExplosionLight"));
	ExplosionLight->SetupAttachment(MineRoot);
	ExplosionLight->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	ExplosionLight->SetIntensityUnits(ELightUnits::Candelas);
	ExplosionLight->SetIntensity(900.0f);
	ExplosionLight->SetAttenuationRadius(900.0f);
	ExplosionLight->SetLightColor(FColor(255, 150, 55));
	ExplosionLight->SetCastShadows(false);
	ExplosionLight->SetVisibility(false);

	if (MineMaterialFinder.Succeeded())
	{
		BodyMID = UMaterialInstanceDynamic::Create(MineMaterialFinder.Object, this);
		PlateMID = UMaterialInstanceDynamic::Create(MineMaterialFinder.Object, this);
		WarningMID = UMaterialInstanceDynamic::Create(MineMaterialFinder.Object, this);
		if (BodyMID)
		{
			BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.11f, 0.13f, 0.14f, 1.0f));
			BodyMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.01f, 0.012f, 0.014f, 1.0f));
			MineBody->SetMaterial(0, BodyMID);
		}
		if (PlateMID)
		{
			PlateMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.34f, 0.20f, 0.10f, 1.0f));
			PlateMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.025f, 0.008f, 0.002f, 1.0f));
			PressurePlate->SetMaterial(0, PlateMID);
		}
		if (WarningMID)
		{
			WarningMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.035f, 0.01f, 1.0f));
			WarningMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(7.0f, 0.18f, 0.02f, 1.0f));
			WarningLightMesh->SetMaterial(0, WarningMID);
		}
	}

	if (FlashMaterialFinder.Succeeded())
	{
		FlashMID = UMaterialInstanceDynamic::Create(FlashMaterialFinder.Object, this);
		if (FlashMID)
		{
			FlashMID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(1.0f, 0.38f, 0.06f, 1.0f));
			FlashMID->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
			ExplosionFlash->SetMaterial(0, FlashMID);
		}
	}

	if (ExplosionSoundFinder.Succeeded())
	{
		ExplosionSound = ExplosionSoundFinder.Object;
	}
}

void ALandmineActor::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerSphere)
	{
		TriggerSphere->SetSphereRadius(TriggerRadius);
	}

	if (ArmDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ArmTimer, this, &ALandmineActor::ArmMine, ArmDelay, false);
	}
	else
	{
		ArmMine();
	}
}

void ALandmineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bExploded)
	{
		ExplosionElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(ExplosionElapsed / ExplosionVisualDuration, 0.0f, 1.0f);
		if (ExplosionFlash)
		{
			ExplosionFlash->SetRelativeScale3D(FVector(0.12f + Alpha * 1.65f));
			if (FlashMID)
			{
				FlashMID->SetScalarParameterValue(TEXT("Opacity"), 1.0f - Alpha);
			}
		}
		if (ExplosionLight)
		{
			ExplosionLight->SetIntensity(900.0f * (1.0f - Alpha) * (1.0f - Alpha));
		}
		if (Alpha >= 1.0f)
		{
			Destroy();
		}
		return;
	}

	BlinkTime += DeltaTime;
	const float BlinkRate = bArmed ? 7.0f : 2.5f;
	const bool bLightOn = FMath::Sin(BlinkTime * BlinkRate) > -0.15f;
	if (WarningLightMesh)
	{
		WarningLightMesh->SetVisibility(bLightOn);
	}
}

void ALandmineActor::ArmMine()
{
	if (bExploded || bArmed)
	{
		return;
	}

	bArmed = true;
	if (TriggerSphere)
	{
		TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// Catch a zombie that was already standing on the mine when the arm delay ended.
	TArray<AActor*> OverlappingActors;
	if (TriggerSphere)
	{
		TriggerSphere->GetOverlappingActors(OverlappingActors, AZombieEnemy::StaticClass());
	}
	for (AActor* Actor : OverlappingActors)
	{
		if (IsValid(Actor))
		{
			Explode();
			return;
		}
	}
}

void ALandmineActor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bArmed && !bExploded && Cast<AZombieEnemy>(OtherActor))
	{
		Explode();
	}
}

void ALandmineActor::Explode()
{
	if (bExploded)
	{
		return;
	}

	bExploded = true;
	bArmed = false;
	GetWorld()->GetTimerManager().ClearTimer(ArmTimer);
	if (TriggerSphere)
	{
		TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TriggerSphere->SetGenerateOverlapEvents(false);
	}
	if (MineBody)
	{
		MineBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MineBody->SetVisibility(false);
	}
	if (PressurePlate)
	{
		PressurePlate->SetVisibility(false);
	}
	if (WarningLightMesh)
	{
		WarningLightMesh->SetVisibility(false);
	}
	if (ExplosionFlash)
	{
		ExplosionFlash->SetVisibility(true);
	}
	if (ExplosionLight)
	{
		ExplosionLight->SetVisibility(true);
	}

	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation(), 1.35f, 0.48f);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LandmineExplosion), false);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(ExplosionRadius), QueryParams);

	TSet<AActor*> HandledActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!IsValid(HitActor) || HandledActors.Contains(HitActor))
		{
			continue;
		}
		HandledActors.Add(HitActor);

		const FVector ToTarget = HitActor->GetActorLocation() - GetActorLocation();
		const float Distance = ToTarget.Size();
		const float Strength = 1.0f - 0.35f * FMath::Clamp(Distance / FMath::Max(ExplosionRadius, 1.0f), 0.0f, 1.0f);

		if (AZombieEnemy* Zombie = Cast<AZombieEnemy>(HitActor))
		{
			FVector RadialDirection = ToTarget.GetSafeNormal2D();
			if (RadialDirection.IsNearlyZero())
			{
				RadialDirection = FMath::VRand().GetSafeNormal2D();
			}
			const FVector LaunchVelocity = RadialDirection * (RadialLaunchSpeed * Strength)
				+ FVector(0.0f, 0.0f, UpwardLaunchSpeed * Strength)
				+ FMath::VRand() * 55.0f;
			const FVector SpinVelocity(
				FMath::FRandRange(-900.0f, 900.0f),
				FMath::FRandRange(-900.0f, 900.0f),
				FMath::FRandRange(-900.0f, 900.0f)
			);
			Zombie->KillByExplosion(LaunchVelocity, SpinVelocity);
			continue;
		}

		if (UPrimitiveComponent* PhysicsComp = Overlap.GetComponent())
		{
			if (PhysicsComp->IsSimulatingPhysics())
			{
				const FVector ContactLocation = PhysicsComp->GetComponentLocation();
				const FVector ImpulseDirection = (ContactLocation - GetActorLocation()).GetSafeNormal();
				PhysicsComp->AddImpulseAtLocation(
					(ImpulseDirection + FVector(0.0f, 0.0f, 0.45f)) * PropImpulseForce * Strength,
					ContactLocation
				);
			}
		}
	}
}
