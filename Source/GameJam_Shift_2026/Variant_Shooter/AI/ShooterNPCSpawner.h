// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterNPCSpawner.generated.h"

class UCapsuleComponent;
class UArrowComponent;
class AShooterNPC;

/**
 *  A basic Actor in charge of spawning Shooter NPCs and monitoring their deaths.
 *  NPCs will be spawned one by one, and the spawner will wait until it dies before spawning a new one.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API AShooterNPCSpawner : public AActor
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* SpawnCapsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* SpawnDirection;

protected:

	/** Type of NPC to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC Spawner")
	TSubclassOf<AShooterNPC> NPCClass;

	/** Time to wait before spawning the first NPC on game start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC Spawner", meta = (ClampMin = 0, ClampMax = 10))
	float InitialSpawnDelay = 5.0f;

	/** Number of NPCs to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC Spawner", meta = (ClampMin = 0, ClampMax = 100))
	int32 SpawnCount = 1;

	/** Time to wait before spawning the next NPC after the current one dies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC Spawner", meta = (ClampMin = 0, ClampMax = 10))
	float RespawnDelay = 5.0f;

	/**
	 * Deterministic base order within the placed spawner set. Slow spawners are all
	 * available on Night 1; fast spawners use their relative order within the fast
	 * subset to unlock one per later night.
	 * Legacy map actors named ShooterNPCSpawner_N fall back to N in BeginPlay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC Spawner")
	int32 UnlockOrder = -1;

	/** Timer to spawn NPCs after a delay */
	FTimerHandle SpawnTimer;

	/** Low-frequency state polling for night/day and daily unlock transitions. */
	FTimerHandle StateTimer;

	TWeakObjectPtr<AShooterNPC> CurrentNPC;
	float NextSpawnTime = 0.0f;
	bool bHasSpawned = false;
	bool bWaitingForDeath = false;
	bool bFastSpawner = false;
	int32 TypeUnlockOrder = -1;

public:	
	
	/** Constructor */
	AShooterNPCSpawner();

public:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

protected:

	/** Spawn an NPC and subscribe to its death event */
	void SpawnNPC();

	/** Re-evaluates night eligibility, daily unlocks, and pending spawn timing. */
	void EvaluateSpawnState();

	/** Returns true when this spawner is unlocked, nighttime, and ready to spawn. */
	bool IsSpawnEligible() const;

	/** Returns the configured order, or the legacy numeric actor-name suffix. */
	int32 GetEffectiveUnlockOrder() const;

	/** Caches fast/slow type and deterministic order within that type. */
	void CacheTypeUnlockOrder();

	/** Returns true when the configured NPC class is a fast zombie variant. */
	bool IsFastSpawner() const;

	/** Called when the spawned NPC has died */
	UFUNCTION()
	void OnNPCDied();

};
