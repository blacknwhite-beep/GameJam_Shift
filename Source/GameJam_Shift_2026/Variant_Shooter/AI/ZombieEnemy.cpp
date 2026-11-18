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
#include "NavigationSystem.h"
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
		StopAIMovement();
		bAwarePlayer = false;
		bCanSeePlayer = false;
		UpdateLocomotionAnimation();
		return;
	}

	const float Now = World->GetTimeSeconds();
	const FVector MyLoc = GetActorLocation();
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();

	bCanSeePlayer = CanSeePlayer(PlayerPawn);
	if (bCanSeePlayer)
	{
		bAwarePlayer = true;
		bMemoryMoveIssued = false;
		LastSeenTime = Now;
		LastKnownPlayerLocation = PlayerLoc;
	}
	else if (bAwarePlayer && Now - LastSeenTime > LoseSightDelay)
	{
		bAwarePlayer = false;
		bHasRoamTarget = false;
		NextRoamTime = Now + RoamPauseSeconds;
		StopAIMovement();
	}

	if (bCanSeePlayer)
	{
		// Turn smoothly to face the player, then chase or attack while in line of sight
		const FVector ToPlayer2D = (PlayerLoc - MyLoc).GetSafeNormal2D();
		if (!ToPlayer2D.IsNearlyZero())
		{
			const FRotator DesiredRot(0.0f, ToPlayer2D.Rotation().Yaw, 0.0f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, 12.0f));
		}

		StopAIMovement();
		const float Dist = FVector::Dist(MyLoc, PlayerLoc);
		if (Dist <= MeleeRange)
		{
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
	}
	else if (bAwarePlayer)
	{
		// Lost sight: investigate the last place the player was seen
		if (!bMemoryMoveIssued)
		{
			if (AAIController* AICon = Cast<AAIController>(GetController()))
			{
				AICon->MoveToLocation(LastKnownPlayerLocation, 80.0f);
			}
			bMemoryMoveIssued = true;
		}
		FaceVelocity(DeltaTime);
	}
	else
	{
		RoamAround(Now);
		FaceVelocity(DeltaTime);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bAwarePlayer ? ZombieMoveSpeed : ZombieMoveSpeed * RoamSpeedScale;
	}

	UpdateLocomotionAnimation();
}

bool AZombieEnemy::CanSeePlayer(const APawn* PlayerPawn) const
{
	UWorld* World = GetWorld();
	if (!World || !PlayerPawn)
	{
		return false;
	}

	const FVector EyeLoc = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	const FVector TargetLoc = PlayerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const FVector ToTarget = TargetLoc - EyeLoc;

	if (ToTarget.Size() > DetectionRange)
	{
		return false;
	}

	const FVector Forward2D = GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTarget2D = ToTarget.GetSafeNormal2D();
	if (FVector::DotProduct(Forward2D, ToTarget2D) < FMath::Cos(FMath::DegreesToRadians(SightHalfAngle)))
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSightTrace), false);
	Params.AddIgnoredActor(this);
	if (World->LineTraceSingleByChannel(Hit, EyeLoc, TargetLoc, ECC_Visibility, Params))
	{
		return Hit.GetActor() == PlayerPawn;
	}
	return true;
}

void AZombieEnemy::RoamAround(float Now)
{
	AAIController* AICon = Cast<AAIController>(GetController());
	UWorld* World = GetWorld();
	if (!AICon || !World)
	{
		return;
	}

	if (bHasRoamTarget)
	{
		const bool bArrived = FVector::Dist2D(GetActorLocation(), RoamTarget) < 90.0f;
		const bool bTimedOut = Now - RoamTargetSetTime > 12.0f;
		if (bArrived || bTimedOut)
		{
			bHasRoamTarget = false;
			NextRoamTime = Now + RoamPauseSeconds;
			AICon->StopMovement();
		}
		return;
	}

	if (Now < NextRoamTime)
	{
		return;
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation NavLoc;
		if (NavSys->GetRandomReachablePointInRadius(GetActorLocation(), RoamRadius, NavLoc))
		{
			RoamTarget = NavLoc.Location;
			RoamTargetSetTime = Now;
			bHasRoamTarget = true;
			AICon->MoveToLocation(RoamTarget, 60.0f);
			return;
		}
	}

	NextRoamTime = Now + 0.5f;
}

void AZombieEnemy::StopAIMovement()
{
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		AICon->StopMovement();
	}
}

void AZombieEnemy::FaceVelocity(float DeltaTime)
{
	const FVector Vel2D = GetVelocity().GetSafeNormal2D();
	if (Vel2D.IsNearlyZero())
	{
		return;
	}

	const FRotator DesiredRot(0.0f, Vel2D.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, 6.0f));
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
	float TargetPlayRate = 1.0f;

	if (Speed2D > 15.0f)
	{
		TargetAnim = (bUseRunAnim && RunAnim) ? RunAnim : WalkAnim;
		if (ReferenceAnimSpeed > 1.0f)
		{
			TargetPlayRate = FMath::Clamp(Speed2D / ReferenceAnimSpeed, 0.55f, 1.85f);
		}
	}

	if (TargetAnim && TargetAnim != CurrentLoopAnim)
	{
		SkelMesh->PlayAnimation(TargetAnim, true);
		CurrentLoopAnim = TargetAnim;
	}

	CurrentLocoPlayRate = FMath::FInterpTo(CurrentLocoPlayRate, TargetPlayRate, World->GetDeltaSeconds(), 8.0f);
	SkelMesh->SetPlayRate(CurrentLocoPlayRate);
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
		StopAIMovement();
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
		GetMesh()->SetPlayRate(DeathPlayRate);
		CurrentLoopAnim = nullptr;

		GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &AZombieEnemy::DeferredDestruction, DeferredDestructionTime, false);
		return;
	}

	Super::Die();
}
