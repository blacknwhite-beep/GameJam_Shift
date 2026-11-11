// Copyright GameJam Shift 2026. All Rights Reserved.

#include "DimensionShiftSubsystem.h"
#include "GameJam_Shift_2026.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"

UDimensionShiftSubsystem::UDimensionShiftSubsystem()
	: FarmWorldOffset(0.0f, 0.0f, -20000.0f)
	, ShiftCooldown(0.5f)
	, bRequireGroundCheck(true)
	, CurrentDimension(EGameDimension::Combat)
	, LastShiftTime(-100.0f)
{
}

void UDimensionShiftSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentDimension = EGameDimension::Combat;
	LastShiftTime = -100.0f;
	PrototypeFarmPlatform.Reset();
	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("UDimensionShiftSubsystem initialized. Starting in Combat dimension."));
}

void UDimensionShiftSubsystem::Deinitialize()
{
	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("UDimensionShiftSubsystem deinitialized."));
	PrototypeFarmPlatform.Reset();
	Super::Deinitialize();
}

void UDimensionShiftSubsystem::EnsurePrototypeFarmPlatform(UWorld* World) const
{
	if (!World)
	{
		return;
	}

	// Ensure world KillZ is deep enough to allow characters and objects to exist at Z = -20000
	if (AWorldSettings* WS = World->GetWorldSettings())
	{
		if (WS->KillZ > -30000.0f)
		{
			WS->KillZ = -100000.0f;
			UE_LOG(LogGameJam_Shift_2026, Log, TEXT("UDimensionShiftSubsystem: Adjusted WorldSettings->KillZ to -100000.0 to support Farming world."));
		}
	}

	if (PrototypeFarmPlatform.IsValid())
	{
		return;
	}

	// Check if any floor already exists around Z = -20000
	FHitResult FloorHit;
	FCollisionQueryParams CheckParams(SCENE_QUERY_STAT(FarmFloorPreCheck), false);
	const bool bHasFloor = World->LineTraceSingleByChannel(
		FloorHit,
		FVector(0.0f, 0.0f, -19000.0f),
		FVector(0.0f, 0.0f, -21000.0f),
		ECC_WorldStatic,
		CheckParams
	);

	if (bHasFloor)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Center at Z = -20050.0f. With Cube thickness 100, the top surface is exactly Z = -20000.0f.
	const FVector PlatformLoc(0.0f, 0.0f, -20050.0f);
	AStaticMeshActor* Platform = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), PlatformLoc, FRotator::ZeroRotator, SpawnParams);
	if (Platform)
	{
		if (UStaticMeshComponent* MeshComp = Platform->GetStaticMeshComponent())
		{
			MeshComp->SetMobility(EComponentMobility::Movable);
			if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
			{
				MeshComp->SetStaticMesh(CubeMesh);
			}
			MeshComp->SetWorldScale3D(FVector(1000.0f, 1000.0f, 1.0f));
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MeshComp->SetCollisionObjectType(ECC_WorldStatic);
			MeshComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
			MeshComp->RecreatePhysicsState();
		}
#if WITH_EDITOR
		Platform->SetActorLabel(TEXT("Prototype_Farm_Platform"));
#endif
		PrototypeFarmPlatform = Platform;
		UE_LOG(LogGameJam_Shift_2026, Log, TEXT("UDimensionShiftSubsystem: Spawned 1km x 1km prototype farm platform at Z = -20000 with collision."));
	}
}

bool UDimensionShiftSubsystem::CanShiftSafely(const FVector& SourceLocation, FVector& OutTargetLocation, const UWorld* World) const
{
	if (!World)
	{
		return false;
	}

	// Strictly altitude-based dimension determination:
	// Combat world is around Z = 0 .. 2,000.
	// Farming world is around Z = -20,000.
	// Threshold at Z = -10,000 ensures deterministic shifting regardless of previous state.
	const bool bPhysicallyInCombat = (SourceLocation.Z > -10000.0f);
	const FVector DimensionDelta = bPhysicallyInCombat ? FarmWorldOffset : -FarmWorldOffset;
	FVector CandidateTarget = SourceLocation + DimensionDelta;

	// When shifting into Farming, guarantee the prototype platform exists so raycast detects solid ground
	if (bPhysicallyInCombat)
	{
		EnsurePrototypeFarmPlatform(const_cast<UWorld*>(World));
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DimensionShiftSafetySweep), false);

	// 1. Ground raycast to verify solid footing and snap feet to floor
	const FVector RayStart = CandidateTarget + FVector(0.0f, 0.0f, 600.0f);
	const FVector RayEnd = CandidateTarget - FVector(0.0f, 0.0f, 2000.0f);

	FHitResult GroundHit;
	bool bHitGround = World->LineTraceSingleByChannel(GroundHit, RayStart, RayEnd, ECC_WorldStatic, QueryParams);
	if (!bHitGround)
	{
		bHitGround = World->LineTraceSingleByChannel(GroundHit, RayStart, RayEnd, ECC_WorldDynamic, QueryParams);
	}

	const bool bWalkableGround = bHitGround && (GroundHit.Normal.Z >= 0.5f);

	// If ground check is required (default true), strictly deny teleport if no valid walkable floor is below destination
	if (bRequireGroundCheck && !bWalkableGround)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Orange, TEXT(">>> [DIMENSION] Shift Denied: No solid floor at destination!"));
		}
		UE_LOG(LogGameJam_Shift_2026, Warning, TEXT("CanShiftSafely failed: No solid walkable floor below candidate target %s (Hit: %d, NormalZ: %.2f)"), 
			*CandidateTarget.ToString(), bHitGround ? 1 : 0, bHitGround ? GroundHit.Normal.Z : 0.0f);
		return false;
	}

	if (bWalkableGround)
	{
		// Align target Z with ground surface + standard character half-height (96) + 2cm cushion
		CandidateTarget.Z = GroundHit.ImpactPoint.Z + 98.0f;
		if (GroundHit.GetActor())
		{
			QueryParams.AddIgnoredActor(GroundHit.GetActor());
		}
	}
	else
	{
		// Fallback to direct delta height only if bRequireGroundCheck was explicitly disabled
		CandidateTarget.Z = SourceLocation.Z + DimensionDelta.Z;
	}

	// 2. Capsule sweep to ensure destination body is not obstructed by solid walls or actors
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(38.0f, 90.0f);
	const FVector SweepStart = CandidateTarget + FVector(0.0f, 0.0f, 5.0f);
	const FVector SweepEnd = CandidateTarget + FVector(0.0f, 0.0f, 6.0f);

	FHitResult ObstructionHit;
	const bool bObstructed = World->SweepSingleByChannel(
		ObstructionHit,
		SweepStart,
		SweepEnd,
		FQuat::Identity,
		ECC_Pawn,
		CapsuleShape,
		QueryParams
	);

	if (bObstructed && ObstructionHit.GetActor() && ObstructionHit.GetActor() != GroundHit.GetActor())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, FString::Printf(TEXT(">>> [DIMENSION] Shift Blocked: Obstructed by %s!"), *GetNameSafe(ObstructionHit.GetActor())));
		}
		UE_LOG(LogGameJam_Shift_2026, Warning, TEXT("CanShiftSafely failed: Target location obstructed by %s"), *GetNameSafe(ObstructionHit.GetActor()));
		return false;
	}

	OutTargetLocation = CandidateTarget;
	return true;
}

bool UDimensionShiftSubsystem::RequestShift(ACharacter* PlayerCharacter)
{
	if (!PlayerCharacter)
	{
		UE_LOG(LogGameJam_Shift_2026, Warning, TEXT("RequestShift failed: PlayerCharacter is null."));
		return false;
	}

	UWorld* World = PlayerCharacter->GetWorld();
	if (!World)
	{
		return false;
	}

	// Cooldown validation
	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastShiftTime < ShiftCooldown)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT(">>> [DIMENSION] Shift is on cooldown..."));
		}
		UE_LOG(LogGameJam_Shift_2026, Verbose, TEXT("RequestShift ignored: Shift is on cooldown."));
		return false;
	}

	// Ensure world KillZ is deep enough so player is not destroyed upon teleporting to Z = -20000
	if (AWorldSettings* WS = World->GetWorldSettings())
	{
		if (WS->KillZ > -30000.0f)
		{
			WS->KillZ = -100000.0f;
		}
	}

	// Determine physical dimension strictly from altitude
	const bool bPhysicallyInCombat = (PlayerCharacter->GetActorLocation().Z > -10000.0f);
	const EGameDimension TargetDimension = bPhysicallyInCombat ? EGameDimension::Farming : EGameDimension::Combat;

	// Destination safety and clearance check
	FVector TargetLocation = FVector::ZeroVector;
	if (!CanShiftSafely(PlayerCharacter->GetActorLocation(), TargetLocation, World))
	{
		UE_LOG(LogGameJam_Shift_2026, Log, TEXT("RequestShift denied: Target destination is unsafe or obstructed."));
		return false;
	}

	// Zero out vertical velocity so the character lands cleanly without falling momentum
	if (UCharacterMovementComponent* MovementComp = PlayerCharacter->GetCharacterMovement())
	{
		MovementComp->Velocity = FVector(0.0f, 0.0f, 0.0f);
	}

	// Teleport character
	const bool bTeleported = PlayerCharacter->TeleportTo(TargetLocation, PlayerCharacter->GetActorRotation(), false, true);
	if (!bTeleported)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT(">>> [DIMENSION] TeleportTo returned false!"));
		}
		UE_LOG(LogGameJam_Shift_2026, Warning, TEXT("RequestShift failed: TeleportTo returned false."));
		return false;
	}

	// Update state
	LastShiftTime = CurrentTime;
	CurrentDimension = TargetDimension;

	if (GEngine)
	{
		const FColor WorldColor = (CurrentDimension == EGameDimension::Farming) ? FColor::Green : FColor::Yellow;
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, WorldColor, FString::Printf(TEXT(">>> [DIMENSION SHIFT SUCCESS] Swapped to: %s! Location: (Z=%.0f)"), 
			*UEnum::GetValueAsString(CurrentDimension), TargetLocation.Z));
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("Dimension shift successful! New dimension: %s, Location: %s"),
		*UEnum::GetValueAsString(CurrentDimension), *TargetLocation.ToString());

	// Broadcast event to notify audio, camera shake, visual effects, and UI
	OnDimensionShifted.Broadcast(CurrentDimension, TargetLocation);

	return true;
}

void UDimensionShiftSubsystem::SetCurrentDimension(EGameDimension NewDimension)
{
	CurrentDimension = NewDimension;
	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("Dimension state manually set to %s"), *UEnum::GetValueAsString(CurrentDimension));
}
