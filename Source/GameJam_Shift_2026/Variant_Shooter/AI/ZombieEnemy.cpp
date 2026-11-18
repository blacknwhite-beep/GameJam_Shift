// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Variant_Shooter/AI/ZombieEnemy.h"
#include "AIController.h"
#include "DimensionShiftSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

AZombieEnemy::AZombieEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	WeaponClass = nullptr;
	CurrentHP = 100.0f;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	// Use plain AAIController instead of AShooterAIController so ST_Shooter StateTree never overrides zombie melee AI
	AIControllerClass = AAIController::StaticClass();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = ZombieMoveSpeed;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	}
	bUseControllerRotationYaw = false;
}

void AZombieEnemy::BeginPlay()
{
	// Skip spawning a firearm by keeping WeaponClass null; AShooterNPC::BeginPlay handles null WeaponClass safely
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = ZombieMoveSpeed;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	// Ensure Manny/Quinn skeletal mesh faces actor forward (+X)
	if (GetMesh())
	{
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	if (ZombieSkinMaterial && GetMesh())
	{
		const int32 NumMats = GetMesh()->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			GetMesh()->SetMaterial(i, ZombieSkinMaterial);
		}
	}
}

void AZombieEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDead)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		return;
	}

	// Only chase players currently in the Combat dimension (Z > -10,000); pause cleanly if player shifted to the Farm
	if (PlayerPawn->GetActorLocation().Z < -10000.0f)
	{
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			AICon->StopMovement();
		}
		return;
	}

	const FVector MyLoc = GetActorLocation();
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const float Dist = FVector::Dist(MyLoc, PlayerLoc);
	if (Dist > DetectionRange)
	{
		return;
	}

	// Always turn smoothly to face the player directly
	const FVector ToPlayer2D = (PlayerLoc - MyLoc).GetSafeNormal2D();
	if (!ToPlayer2D.IsNearlyZero())
	{
		const FRotator DesiredRot(0.0f, ToPlayer2D.Rotation().Yaw, 0.0f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, 10.0f));
	}

	if (Dist <= MeleeRange)
	{
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			AICon->StopMovement();
		}

		const float Now = World->GetTimeSeconds();
		if (Now - LastAttackTime >= AttackCooldown)
		{
			LastAttackTime = Now;
			PerformMeleeAttack(PlayerPawn);
		}
	}
	else
	{
		// Steer directly toward the player; also nudge AIController if NavMesh is built
		AddMovementInput(ToPlayer2D, 1.0f);
	}
}

void AZombieEnemy::PerformMeleeAttack(AActor* TargetActor)
{
	if (!TargetActor || bIsDead)
	{
		return;
	}

	// Stop horizontal velocity before striking so the zombie never shoves or pops the player's capsule
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	if (AttackGroanSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackGroanSound, GetActorLocation());
	}

	UGameplayStatics::ApplyDamage(TargetActor, MeleeDamage, GetController(), this, nullptr);
}

void AZombieEnemy::Die()
{
	if (bIsDead)
	{
		return;
	}

	Super::Die();

	if (KillGoldReward > 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Subsystem->AddGold(KillGoldReward);
			}
		}
	}
}
