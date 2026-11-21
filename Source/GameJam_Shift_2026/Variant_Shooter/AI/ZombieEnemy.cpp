// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Variant_Shooter/AI/ZombieEnemy.h"
#include "AIController.h"
#include "DimensionShiftSubsystem.h"
#include "ShooterGameMode.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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

		ZombieWoundMIDs.Reset();
		const int32 NumMats = SkelMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			UMaterialInterface* BaseMat = ZombieSkinMaterial ? ZombieSkinMaterial.Get() : SkelMesh->GetMaterial(i);
			if (BaseMat)
			{
				if (UMaterialInstanceDynamic* WoundMID = SkelMesh->CreateDynamicMaterialInstance(i, BaseMat))
				{
					ZombieWoundMIDs.Add(WoundMID);
				}
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

	// Smoothly pitch the zombie belly-down onto the ground while crawling (after losing a leg)
	if (bIsCrawling && GetMesh())
	{
		CrawlBlendAlpha = FMath::Min(1.0f, CrawlBlendAlpha + DeltaTime * 3.8f);
		const float Smooth = FMath::InterpEaseOut(0.0f, 1.0f, CrawlBlendAlpha, 2.0f);
		const float GroundSpeed = GetVelocity().Size2D();
		if (GroundSpeed > 8.0f)
		{
			CrawlRockTime += DeltaTime * 6.5f;
		}
		const float SideRock = FMath::Sin(CrawlRockTime) * 7.5f * Smooth;
		const float TargetPitchRoll = 78.0f * Smooth;
		const float TargetMeshZ = FMath::Lerp(DefaultMeshRelativeZ, -18.0f, Smooth);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, TargetMeshZ));
		GetMesh()->SetRelativeRotation(FRotator(SideRock, MeshYawOffset, TargetPitchRoll));
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

	// Pause cleanly if player shifted down into the Farm world or while the Start Menu is active (zombies roam instead of attacking)
	bool bStartMenuOpen = false;
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			bStartMenuOpen = Sub->bIsStartMenuActive;
		}
	}

	if (bStartMenuOpen)
	{
		bAwarePlayer = false;
		bCanSeePlayer = false;
		RoamAround(World->GetTimeSeconds());
		FaceVelocity(DeltaTime);
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = ZombieMoveSpeed * RoamSpeedScale;
		}
		UpdateLocomotionAnimation();
		return;
	}

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

	if (USoundBase* BiteHitSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_ZombieBiteHit.SFX_ZombieBiteHit")))
	{
		UGameplayStatics::PlaySound2D(this, BiteHitSnd, 1.0f, FMath::FRandRange(0.95f, 1.06f));
	}
	else if (AttackGroanSound)
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

void AZombieEnemy::ApplyBulletWound(const FVector& WorldHitLocation, const FVector& ShotDirection)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1. Tom Looman 3D Character Wound Shader Mask:
	// Store the hit offset relative to ActorLocation (matches WorldPosition - ActorPositionWS in M_ZombieMaster)
	const FVector LocalHitOffset = WorldHitLocation - GetActorLocation();
	const float WoundRadius = FMath::FRandRange(22.0f, 30.0f);
	const int32 SlotIdx = NextWoundSlotIndex % 4;
	NextWoundSlotIndex = (NextWoundSlotIndex + 1) % 4;

	const FName PosParamName(*FString::Printf(TEXT("HitPosition%d"), SlotIdx));
	const FName ActiveParamName(*FString::Printf(TEXT("HitActive%d"), SlotIdx));
	const FLinearColor WoundVec(LocalHitOffset.X, LocalHitOffset.Y, LocalHitOffset.Z, WoundRadius);

	for (UMaterialInstanceDynamic* MID : ZombieWoundMIDs)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(PosParamName, WoundVec);
			MID->SetScalarParameterValue(ActiveParamName, 1.0f);
		}
	}

	// 2. Spawn Realistic Arterial Blood Droplet Burst & Ground Blood Splatter Pool
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));

	if (SphereMesh && BaseMat)
	{
		const FVector OutwardSprayDir = (-ShotDirection).GetSafeNormal();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Spawn 7 flying blood droplets with gravity arc
		for (int32 i = 0; i < 7; ++i)
		{
			AActor* DropActor = World->SpawnActor<AActor>(AActor::StaticClass(), WorldHitLocation, FRotator::ZeroRotator, Params);
			if (!DropActor)
			{
				continue;
			}

			UStaticMeshComponent* DropMesh = NewObject<UStaticMeshComponent>(DropActor, TEXT("BloodDropletMesh"));
			DropMesh->SetStaticMesh(SphereMesh);
			DropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DropMesh->SetCastShadow(false);
			const float DropScale = FMath::FRandRange(0.024f, 0.048f);
			DropMesh->SetWorldScale3D(FVector(DropScale, DropScale, DropScale * 1.35f));

			if (UMaterialInstanceDynamic* BloodMID = UMaterialInstanceDynamic::Create(BaseMat, DropActor))
			{
				BloodMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.26f, 0.006f, 0.006f, 1.0f));
				BloodMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.08f, 0.002f, 0.002f, 1.0f));
				DropMesh->SetMaterial(0, BloodMID);
			}

			DropMesh->RegisterComponent();
			DropActor->SetRootComponent(DropMesh);

			UProjectileMovementComponent* DropMove = NewObject<UProjectileMovementComponent>(DropActor, TEXT("BloodDropMove"));
			const FVector RandSpread = FVector(
				FMath::FRandRange(-0.55f, 0.55f),
				FMath::FRandRange(-0.55f, 0.55f),
				FMath::FRandRange(0.20f, 0.75f)
			);
			const FVector LaunchDir = (OutwardSprayDir * 0.65f + RandSpread).GetSafeNormal();
			const float Speed = FMath::FRandRange(180.0f, 420.0f);
			DropMove->Velocity = LaunchDir * Speed;
			DropMove->InitialSpeed = Speed;
			DropMove->MaxSpeed = 800.0f;
			DropMove->ProjectileGravityScale = 1.15f;
			DropMove->RegisterComponent();

			DropActor->SetLifeSpan(FMath::FRandRange(0.38f, 0.65f));
		}
	}

	// 3. Trace straight down to place a glossy crimson blood splatter pool on the floor under the bleeding zombie
	if (CylinderMesh && BaseMat)
	{
		FHitResult FloorHit;
		FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(BloodFloorTrace), false);
		FloorParams.AddIgnoredActor(this);
		const FVector TraceStart = WorldHitLocation + FVector(FMath::FRandRange(-25.0f, 25.0f), FMath::FRandRange(-25.0f, 25.0f), 10.0f);
		if (World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceStart - FVector(0.0f, 0.0f, 320.0f), ECC_WorldStatic, FloorParams))
		{
			FActorSpawnParameters PoolParams;
			PoolParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* PoolActor = World->SpawnActor<AActor>(AActor::StaticClass(), FloorHit.ImpactPoint + FVector(0.0f, 0.0f, 0.6f), FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), PoolParams);
			if (PoolActor)
			{
				UStaticMeshComponent* PoolMesh = NewObject<UStaticMeshComponent>(PoolActor, TEXT("BloodPoolMesh"));
				PoolMesh->SetStaticMesh(CylinderMesh);
				PoolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PoolMesh->SetCastShadow(false);
				const float PoolRadiusX = FMath::FRandRange(0.22f, 0.44f);
				const float PoolRadiusY = FMath::FRandRange(0.22f, 0.44f);
				PoolMesh->SetWorldScale3D(FVector(PoolRadiusX, PoolRadiusY, 0.008f));

				if (UMaterialInstanceDynamic* PoolMID = UMaterialInstanceDynamic::Create(BaseMat, PoolActor))
				{
					PoolMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.20f, 0.005f, 0.005f, 1.0f));
					PoolMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.04f, 0.001f, 0.001f, 1.0f));
					PoolMesh->SetMaterial(0, PoolMID);
				}

				PoolMesh->RegisterComponent();
				PoolActor->SetRootComponent(PoolMesh);
				PoolActor->SetLifeSpan(14.0f);
			}
		}
	}

	// 4. Evaluate CodeLikeMe-style limb dismemberment (Head -> instant kill, Arms -> fly off & keep chasing, Legs -> fly off & crawl!)
	TryDismemberAtHit(WorldHitLocation, ShotDirection);
}

void AZombieEnemy::TryDismemberAtHit(const FVector& WorldHitLocation, const FVector& ShotDirection)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh || bIsDead)
	{
		return;
	}

	const FVector Dir = ShotDirection.GetSafeNormal();
	const FVector RayStart = WorldHitLocation - Dir * 25.0f;
	const FVector RayEnd = WorldHitLocation + Dir * 130.0f;

	struct FLimbCandidate
	{
		FName QueryBone;
		FName SeverBone;
		FName StumpBone;
		float MaxDist;
		bool bIsHead;
		bool bIsLeg;
		bool* bAlreadySevered;
	};

	const FLimbCandidate Candidates[] = {
		// Headshot (instant decapitation + kill)
		{ FName("Zombie_Head"),          FName("Zombie_Head"),          FName("Zombie_Neck"),          28.0f, true,  false, &bHeadSevered },
		// Left Arm
		{ FName("Zombie_LeftForeArm"),   FName("Zombie_LeftForeArm"),   FName("Zombie_LeftArm"),       28.0f, false, false, &bLeftArmSevered },
		{ FName("Zombie_LeftHand"),      FName("Zombie_LeftForeArm"),   FName("Zombie_LeftArm"),       28.0f, false, false, &bLeftArmSevered },
		{ FName("Zombie_LeftArm"),       FName("Zombie_LeftArm"),       FName("Zombie_LeftShoulder"),  28.0f, false, false, &bLeftArmSevered },
		// Right Arm
		{ FName("Zombie_RightForeArm"),  FName("Zombie_RightForeArm"),  FName("Zombie_RightArm"),      28.0f, false, false, &bRightArmSevered },
		{ FName("Zombie_RightHand"),     FName("Zombie_RightForeArm"),  FName("Zombie_RightArm"),      28.0f, false, false, &bRightArmSevered },
		{ FName("Zombie_RightArm"),      FName("Zombie_RightArm"),      FName("Zombie_RightShoulder"), 28.0f, false, false, &bRightArmSevered },
		// Left Leg (-> crawling!)
		{ FName("Zombie_LeftLeg"),       FName("Zombie_LeftLeg"),       FName("Zombie_LeftUpLeg"),     32.0f, false, true,  &bLeftLegSevered },
		{ FName("Zombie_LeftFoot"),      FName("Zombie_LeftLeg"),       FName("Zombie_LeftUpLeg"),     32.0f, false, true,  &bLeftLegSevered },
		{ FName("Zombie_LeftUpLeg"),     FName("Zombie_LeftUpLeg"),     FName("Zombie_Spine"),         30.0f, false, true,  &bLeftLegSevered },
		// Right Leg (-> crawling!)
		{ FName("Zombie_RightLeg"),      FName("Zombie_RightLeg"),      FName("Zombie_RightUpLeg"),    32.0f, false, true,  &bRightLegSevered },
		{ FName("Zombie_RightFoot"),     FName("Zombie_RightLeg"),      FName("Zombie_RightUpLeg"),    32.0f, false, true,  &bRightLegSevered },
		{ FName("Zombie_RightUpLeg"),    FName("Zombie_RightUpLeg"),    FName("Zombie_Spine"),         30.0f, false, true,  &bRightLegSevered },
	};

	const FLimbCandidate* BestMatch = nullptr;
	float BestDist = FLT_MAX;

	for (const FLimbCandidate& C : Candidates)
	{
		if (*C.bAlreadySevered)
		{
			continue;
		}
		if (SkelMesh->GetBoneIndex(C.QueryBone) == INDEX_NONE)
		{
			continue;
		}
		const FVector BoneLoc = SkelMesh->GetBoneLocation(C.QueryBone);
		const float Dist = FMath::PointDistToSegment(BoneLoc, RayStart, RayEnd);
		if (Dist <= C.MaxDist && Dist < BestDist)
		{
			BestDist = Dist;
			BestMatch = &C;
		}
	}

	if (!BestMatch)
	{
		return;
	}

	*BestMatch->bAlreadySevered = true;
	DismemberLimb(BestMatch->SeverBone, BestMatch->StumpBone, BestMatch->bIsHead, BestMatch->bIsLeg, Dir);
}

void AZombieEnemy::DismemberLimb(FName SeverBone, FName StumpSocketBone, bool bIsHead, bool bIsLeg, const FVector& ShotDirection)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		return;
	}

	const FVector SeverWorldLoc = SkelMesh->GetBoneLocation(SeverBone);
	const FRotator SeverWorldRot = SkelMesh->GetSocketRotation(SeverBone);

	// 1. Cleanly hide the severed bone & terminate its child physics bodies (zero mesh stretching!)
	SkelMesh->HideBoneByName(SeverBone, PBO_Term);

	// 2. Cap the severed joint with a gory crimson flesh + pale bone stump
	AttachStumpCap(StumpSocketBone, bIsHead);

	// 3. Spawn a flying physics-simulating severed limb piece that tumbles across the ground
	SpawnSeveredLimbGib(SeverWorldLoc, SeverWorldRot, bIsHead, bIsLeg, ShotDirection);

	// 4. Play a wet crunch sound & alert the zombie to the player
	if (AttackGroanSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackGroanSound, SeverWorldLoc, 0.9f, bIsHead ? 1.35f : 0.85f);
	}
	bAwarePlayer = true;

	// 5. Apply CodeLikeMe-style behavioral consequence:
	//    - Head blown off -> instant fatal headshot!
	//    - Leg blown off  -> drop to the ground and crawl relentlessly toward the player!
	//    - Both arms off  -> keep chasing with weaker bite damage!
	if (bIsHead)
	{
		CurrentHP = 0.0f;
		Die();
	}
	else if (bIsLeg && !bIsCrawling)
	{
		EnterCrawlState();
	}
	else if (bLeftArmSevered && bRightArmSevered)
	{
		MeleeDamage = FMath::Max(6.0f, MeleeDamage * 0.5f);
	}
}

void AZombieEnemy::EnterCrawlState()
{
	if (bIsCrawling || bIsDead)
	{
		return;
	}

	bIsCrawling = true;
	CrawlBlendAlpha = 0.0f;
	CrawlRockTime = 0.0f;

	if (GetMesh())
	{
		DefaultMeshRelativeZ = GetMesh()->GetRelativeLocation().Z;
	}

	// Lower collision capsule so the player must aim down at the crawling zombie
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCapsuleSize(34.0f, 36.0f, true);
	}

	// Slow down to a relentless ground crawl
	ZombieMoveSpeed = FMath::Max(55.0f, ZombieMoveSpeed * 0.38f);
	bUseRunAnim = false;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = ZombieMoveSpeed;
	}
}

void AZombieEnemy::AttachStumpCap(FName SocketBone, bool bIsHead)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!SkelMesh || !SphereMesh || !BaseMat || SkelMesh->GetBoneIndex(SocketBone) == INDEX_NONE)
	{
		return;
	}

	UStaticMeshComponent* FleshCap = NewObject<UStaticMeshComponent>(this);
	FleshCap->SetStaticMesh(SphereMesh);
	FleshCap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FleshCap->SetCastShadow(false);
	const float CapScale = bIsHead ? 0.14f : 0.11f;
	FleshCap->SetRelativeScale3D(FVector(CapScale, CapScale, CapScale * 0.7f));
	if (UMaterialInstanceDynamic* FleshMID = UMaterialInstanceDynamic::Create(BaseMat, this))
	{
		FleshMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.008f, 0.008f, 1.0f));
		FleshMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.06f, 0.002f, 0.002f, 1.0f));
		FleshCap->SetMaterial(0, FleshMID);
	}
	FleshCap->SetupAttachment(SkelMesh, SocketBone);
	FleshCap->RegisterComponent();

	// Exposed pale bone tip poking slightly out of the crimson stump
	if (CylinderMesh)
	{
		UStaticMeshComponent* BoneTip = NewObject<UStaticMeshComponent>(this);
		BoneTip->SetStaticMesh(CylinderMesh);
		BoneTip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoneTip->SetCastShadow(false);
		BoneTip->SetRelativeScale3D(FVector(0.032f, 0.032f, 0.09f));
		if (UMaterialInstanceDynamic* BoneMID = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			BoneMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.84f, 0.78f, 0.68f, 1.0f));
			BoneTip->SetMaterial(0, BoneMID);
		}
		BoneTip->SetupAttachment(FleshCap);
		BoneTip->RegisterComponent();
	}
}

void AZombieEnemy::SpawnSeveredLimbGib(const FVector& SpawnLoc, const FRotator& SpawnRot, bool bIsHead, bool bIsLeg, const FVector& ShotDirection)
{
	UWorld* World = GetWorld();
	UStaticMesh* MeshAsset = bIsHead
		? LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))
		: LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!World || !MeshAsset || !BaseMat)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* GibActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnLoc, SpawnRot, Params);
	if (!GibActor)
	{
		return;
	}

	UStaticMeshComponent* GibMesh = NewObject<UStaticMeshComponent>(GibActor, TEXT("SeveredLimbGib"));
	GibMesh->SetStaticMesh(MeshAsset);
	const FVector GibScale = bIsHead
		? FVector(0.22f, 0.20f, 0.24f)
		: (bIsLeg ? FVector(0.14f, 0.14f, 0.48f) : FVector(0.11f, 0.11f, 0.40f));
	GibMesh->SetWorldScale3D(GibScale);

	if (UMaterialInstanceDynamic* SkinMID = UMaterialInstanceDynamic::Create(BaseMat, GibActor))
	{
		// Rotting zombie flesh-green tint stained with dark crimson
		SkinMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.24f, 0.26f, 0.17f, 1.0f));
		SkinMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.03f, 0.005f, 0.004f, 1.0f));
		GibMesh->SetMaterial(0, SkinMID);
	}

	GibMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GibMesh->SetCollisionObjectType(ECC_PhysicsBody);
	GibMesh->SetCollisionResponseToAllChannels(ECR_Block);
	GibMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GibMesh->SetSimulatePhysics(true);
	GibMesh->SetEnableGravity(true);
	GibMesh->RegisterComponent();
	GibActor->SetRootComponent(GibMesh);

	// Launch the severed limb outward along the bullet trajectory with realistic tumbling spin
	const FVector ImpulseDir = (ShotDirection + FVector(FMath::FRandRange(-0.25f, 0.25f), FMath::FRandRange(-0.25f, 0.25f), 0.55f)).GetSafeNormal();
	GibMesh->AddImpulse(ImpulseDir * FMath::FRandRange(340.0f, 520.0f), NAME_None, true);
	GibMesh->AddAngularImpulseInDegrees(
		FVector(FMath::FRandRange(-520.0f, 520.0f), FMath::FRandRange(-520.0f, 520.0f), FMath::FRandRange(-520.0f, 520.0f)),
		NAME_None,
		true
	);

	GibActor->SetLifeSpan(18.0f);
}
