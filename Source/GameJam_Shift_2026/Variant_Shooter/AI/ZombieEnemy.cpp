// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Variant_Shooter/AI/ZombieEnemy.h"
#include "AIController.h"
#include "DimensionShiftSubsystem.h"
#include "ShooterGameMode.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

AZombieEnemy::AZombieEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	WeaponClass = nullptr;
	CurrentHP = 100.0f;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCapsuleSize(34.0f, 90.0f);
	}

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
	Super::BeginPlay();

	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;

	// Snap zombie cleanly onto solid floor so it never spawns floating
	if (UWorld* World = GetWorld())
	{
		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieGroundSnap), false);
		Params.AddIgnoredActor(this);
		const FVector Loc = GetActorLocation();
		if (World->LineTraceSingleByChannel(GroundHit, Loc + FVector(0.0f, 0.0f, 200.0f), Loc - FVector(0.0f, 0.0f, 600.0f), ECC_WorldStatic, Params))
		{
			SetActorLocation(FVector(Loc.X, Loc.Y, GroundHit.ImpactPoint.Z + HalfHeight));
		}
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = ZombieMoveSpeed;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	// Ground feet at the exact bottom of the capsule and align mesh forward
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight));
		SkelMesh->SetRelativeRotation(FRotator(0.0f, MeshYawOffset, 0.0f));
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

		if (ZombieSkinMaterial)
		{
			const int32 NumMats = SkelMesh->GetNumMaterials();
			for (int32 i = 0; i < NumMats; ++i)
			{
				SkelMesh->SetMaterial(i, ZombieSkinMaterial);
			}
		}
	}

	// Lazily fall back to the imported Zombie animations if not already set on the Blueprint
	if (!IdleAnim)   { IdleAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Dev_Blueprint/Characters/Zombie/Animations/Anim_Zombie_idle1.Anim_Zombie_idle1")); }
	if (!WalkAnim)   { WalkAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Dev_Blueprint/Characters/Zombie/Animations/Anim_Zombie_walk.Anim_Zombie_walk")); }
	if (!RunAnim)    { RunAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Dev_Blueprint/Characters/Zombie/Animations/Anim_Zombie_run.Anim_Zombie_run")); }
	if (!AttackAnim) { AttackAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Dev_Blueprint/Characters/Zombie/Animations/Anim_Zombie_atack1.Anim_Zombie_atack1")); }
	if (!DeathAnim)  { DeathAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Dev_Blueprint/Characters/Zombie/Animations/Anim_Zombie_death1.Anim_Zombie_death1")); }

	if (IdleAnim && GetMesh())
	{
		GetMesh()->PlayAnimation(IdleAnim, true);
		CurrentLoopAnim = IdleAnim;
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
		UpdateLocomotionAnimation();
		return;
	}

	// Pause cleanly if player shifted down into the Farm world
	if (PlayerPawn->GetActorLocation().Z < -10000.0f)
	{
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			AICon->StopMovement();
		}
		UpdateLocomotionAnimation();
		return;
	}

	const FVector MyLoc = GetActorLocation();
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const float Dist = FVector::Dist(MyLoc, PlayerLoc);
	if (Dist > DetectionRange)
	{
		UpdateLocomotionAnimation();
		return;
	}

	// Always turn smoothly to face the player directly
	const FVector ToPlayer2D = (PlayerLoc - MyLoc).GetSafeNormal2D();
	if (!ToPlayer2D.IsNearlyZero())
	{
		const FRotator DesiredRot(0.0f, ToPlayer2D.Rotation().Yaw, 0.0f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, 12.0f));
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
		AddMovementInput(ToPlayer2D, 1.0f);
	}

	UpdateLocomotionAnimation();
}

void AZombieEnemy::UpdateLocomotionAnimation()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	UWorld* World = GetWorld();
	if (!SkelMesh || !World)
	{
		return;
	}

	// Let one-shot melee attack animation finish before resuming idle/walk/run
	if (World->GetTimeSeconds() < ActionAnimEndTime)
	{
		return;
	}

	const float Speed2D = GetVelocity().Size2D();
	UAnimSequence* TargetAnim = IdleAnim;
	float PlayRate = 1.0f;

	if (Speed2D > 15.0f)
	{
		TargetAnim = (bUseRunAnim && RunAnim) ? RunAnim : WalkAnim;
		if (ReferenceAnimSpeed > 1.0f)
		{
			PlayRate = FMath::Clamp(Speed2D / ReferenceAnimSpeed, 0.55f, 1.85f);
		}
	}

	if (TargetAnim && TargetAnim != CurrentLoopAnim)
	{
		SkelMesh->PlayAnimation(TargetAnim, true);
		CurrentLoopAnim = TargetAnim;
	}
	SkelMesh->SetPlayRate(PlayRate);
}

void AZombieEnemy::PerformMeleeAttack(AActor* TargetActor)
{
	if (!TargetActor || bIsDead)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	if (AttackAnim && GetMesh())
	{
		GetMesh()->PlayAnimation(AttackAnim, false);
		GetMesh()->SetPlayRate(1.4f);
		CurrentLoopAnim = nullptr;
		ActionAnimEndTime = GetWorld()->GetTimeSeconds() + FMath::Min(1.05f, AttackAnim->GetPlayLength() / 1.4f);
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

	if (DeathAnim && GetMesh())
	{
		bIsDead = true;
		Tags.Add(DeathTag);
		OnPawnDeath.Broadcast();

		if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
		{
			GM->IncrementTeamScore(TeamByte);
		}

		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->StopActiveMovement();

		GetMesh()->PlayAnimation(DeathAnim, false);
		GetMesh()->SetPlayRate(1.25f);
		CurrentLoopAnim = nullptr;

		GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &AZombieEnemy::DeferredDestruction, DeferredDestructionTime, false);
		return;
	}

	Super::Die();
}
