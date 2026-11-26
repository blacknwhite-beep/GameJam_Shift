// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterNPCSpawner.h"
#include "Dimension/DimensionShiftSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "ShooterNPC.h"
#include "Variant_Shooter/AI/ZombieEnemy.h"
#include "EngineUtils.h"

// Sets default values
AShooterNPCSpawner::AShooterNPCSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the reference spawn capsule
	SpawnCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Spawn Capsule"));
	SpawnCapsule->SetupAttachment(RootComponent);

	SpawnCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	SpawnCapsule->SetCapsuleSize(35.0f, 90.0f);
	SpawnCapsule->SetCollisionProfileName(FName("NoCollision"));

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("Spawn Direction"));
	SpawnDirection->SetupAttachment(RootComponent);
}

int32 AShooterNPCSpawner::GetEffectiveUnlockOrder() const
{
	if (UnlockOrder >= 0)
	{
		return UnlockOrder;
	}

	const FString ActorName = GetName();
	const int32 LastUnderscore = ActorName.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (LastUnderscore != INDEX_NONE)
	{
		const FString NumericSuffix = ActorName.RightChop(LastUnderscore + 1);
		if (FCString::IsNumeric(*NumericSuffix))
		{
			return FCString::Atoi(*NumericSuffix);
		}
	}
	return -1;
}

void AShooterNPCSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Backward-compatible rank for the current native map actors (_0.._9).
	if (UnlockOrder < 0)
	{
		UnlockOrder = GetEffectiveUnlockOrder();
	}

	CacheTypeUnlockOrder();

	// Polling keeps normal ticking and developer time-skip transitions in sync
	// without changing the subsystem or destroying living zombies at dawn.
	GetWorld()->GetTimerManager().SetTimer(StateTimer, this, &AShooterNPCSpawner::EvaluateSpawnState, 0.25f, true);
	EvaluateSpawnState();
}

void AShooterNPCSpawner::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the spawn and state timers
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	GetWorld()->GetTimerManager().ClearTimer(StateTimer);

	if (AShooterNPC* NPC = CurrentNPC.Get())
	{
		NPC->OnPawnDeath.RemoveDynamic(this, &AShooterNPCSpawner::OnNPCDied);
	}
}

bool AShooterNPCSpawner::IsFastSpawner() const
{
	if (!NPCClass)
	{
		return false;
	}

	if (const AZombieEnemy* ZombieCDO = NPCClass->GetDefaultObject<AZombieEnemy>())
	{
		return ZombieCDO->bUseRunAnim;
	}

	// Fallback for any non-zombie or renamed class that still follows the naming convention.
	return NPCClass->GetName().Contains(TEXT("Fast"), ESearchCase::IgnoreCase);
}

void AShooterNPCSpawner::CacheTypeUnlockOrder()
{
	bFastSpawner = IsFastSpawner();
	TypeUnlockOrder = 0;

	if (!GetWorld() || UnlockOrder < 0)
	{
		TypeUnlockOrder = -1;
		return;
	}

	// Count only earlier spawners of the same zombie type. This turns the existing
	// global 0..9 map order into independent slow/fast unlock sequences.
	for (TActorIterator<AShooterNPCSpawner> It(GetWorld()); It; ++It)
	{
		AShooterNPCSpawner* Other = *It;
		if (Other == this || !IsValid(Other))
		{
			continue;
		}
		const int32 OtherOrder = Other->GetEffectiveUnlockOrder();
		if (OtherOrder < 0)
		{
			continue;
		}
		if (OtherOrder < UnlockOrder && Other->IsFastSpawner() == bFastSpawner)
		{
			++TypeUnlockOrder;
		}
	}
}

bool AShooterNPCSpawner::IsSpawnEligible() const
{
	const UDimensionShiftSubsystem* Sub = (GetGameInstance())
		? GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>()
		: nullptr;

	if (!Sub || SpawnCount <= 0 || bWaitingForDeath || TypeUnlockOrder < 0 || !Sub->IsNightWindow())
	{
		return false;
	}

	if (bFastSpawner)
	{
		// Night 1: zero fast spawners. Night 2: one. Then +1 fast spawner each night.
		const int32 FastUnlockCount = FMath::Max(0, Sub->CurrentDay - 1);
		return TypeUnlockOrder < FastUnlockCount;
	}

	// Every slow-zombie spawner is available from the first night onward.
	return true;
}

void AShooterNPCSpawner::EvaluateSpawnState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!IsSpawnEligible())
	{
		// Suspend new spawns at dawn or while locked/waiting, but keep living NPCs alive.
		World->GetTimerManager().ClearTimer(SpawnTimer);
		return;
	}

	if (World->GetTimerManager().IsTimerActive(SpawnTimer))
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float RemainingDelay = bHasSpawned ? FMath::Max(0.0f, NextSpawnTime - Now) : InitialSpawnDelay;
	if (RemainingDelay <= 0.001f)
	{
		SpawnNPC();
	}
	else
	{
		World->GetTimerManager().SetTimer(SpawnTimer, this, &AShooterNPCSpawner::SpawnNPC, RemainingDelay, false);
	}
}

void AShooterNPCSpawner::SpawnNPC()
{
	// A timer can fire just as dawn begins, so recheck eligibility at the last moment.
	if (!IsSpawnEligible())
	{
		return;
	}

	// ensure the NPC class is valid
	if (IsValid(NPCClass))
	{
		// spawn the NPC at the reference capsule's transform
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AShooterNPC* SpawnedNPC = GetWorld()->SpawnActor<AShooterNPC>(NPCClass, SpawnCapsule->GetComponentTransform(), SpawnParams);

		// was the NPC successfully created?
		if (SpawnedNPC)
		{
			CurrentNPC = SpawnedNPC;
			bHasSpawned = true;
			bWaitingForDeath = true;

			// subscribe to the death delegate
			SpawnedNPC->OnPawnDeath.AddDynamic(this, &AShooterNPCSpawner::OnNPCDied);
		}
	}
}

void AShooterNPCSpawner::OnNPCDied()
{
	if (!bWaitingForDeath)
	{
		return;
	}

	// Unbind the dead NPC immediately so a stale/duplicate broadcast can never
	// be mistaken for the death of a future replacement zombie.
	if (AShooterNPC* DeadNPC = CurrentNPC.Get())
	{
		DeadNPC->OnPawnDeath.RemoveDynamic(this, &AShooterNPCSpawner::OnNPCDied);
	}

	bWaitingForDeath = false;
	CurrentNPC.Reset();

	// decrease the spawn counter
	--SpawnCount;

	// is this the last NPC we should spawn?
	if (SpawnCount <= 0)
	{
		return;
	}

	// Hard minimum: this spawner cannot create another zombie until at least
	// 5 seconds after its own previous zombie's death event.
	NextSpawnTime = GetWorld()->GetTimeSeconds() + FMath::Max(5.0f, RespawnDelay);
	EvaluateSpawnState();
}
