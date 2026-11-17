// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "ZombieEnemy.generated.h"

class USoundBase;
class UMaterialInterface;

/**
 * Melee-only Zombie enemy inheriting from AShooterNPC so it integrates with
 * AShooterNPCSpawner, OnPawnDeath, and team scoring while chasing and melee-attacking
 * the player at either slow shambling or fast sprinting speeds.
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API AZombieEnemy : public AShooterNPC
{
	GENERATED_BODY()

public:
	AZombieEnemy();

	/** Walk/run movement speed for this zombie variant (e.g. 200 = slow, 480 = fast) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (ClampMin = "50.0"))
	float ZombieMoveSpeed = 220.0f;

	/** Distance in cm at which the zombie can land a melee strike */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (ClampMin = "60.0"))
	float MeleeRange = 165.0f;

	/** Damage dealt per melee strike */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (ClampMin = "1.0"))
	float MeleeDamage = 18.0f;

	/** Seconds between melee strikes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (ClampMin = "0.25"))
	float AttackCooldown = 1.15f;

	/** Max detection distance in cm (chases player whenever within this range in Combat world) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "500.0"))
	float DetectionRange = 6000.0f;

	/** Optional material override applied to the zombie mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Visuals")
	TObjectPtr<UMaterialInterface> ZombieSkinMaterial;

	/** Optional sound played when the zombie lunges / attacks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Audio")
	TObjectPtr<USoundBase> AttackGroanSound;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Executes a melee hit on the target player */
	void PerformMeleeAttack(AActor* TargetActor);

private:
	float LastAttackTime = -100.0f;
};
