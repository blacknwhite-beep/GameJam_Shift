// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "ZombieEnemy.generated.h"

class UAnimSequence;
class USoundBase;
class UMaterialInterface;

/**
 * Melee-only Zombie enemy inheriting from AShooterNPC so it integrates with
 * AShooterNPCSpawner, OnPawnDeath, Gold rewards, and team scoring while chasing
 * and melee-attacking the player with speed-matched animations and proper grounding.
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API AZombieEnemy : public AShooterNPC
{
	GENERATED_BODY()

public:
	AZombieEnemy();

	/** Walk/run movement speed for this zombie variant (matched to ReferenceAnimSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (ClampMin = "50.0"))
	float ZombieMoveSpeed = 140.0f;

	/** True for fast running zombies (uses RunAnim), false for slow shamblers (uses WalkAnim) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement")
	bool bUseRunAnim = false;

	/** Ground speed (cm/s) that corresponds to 1.0x play rate of the active locomotion animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (ClampMin = "40.0"))
	float ReferenceAnimSpeed = 140.0f;

	/** Yaw offset applied to SkeletalMeshComponent so the FBX forward axis aligns with actor +X */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Visuals")
	float MeshYawOffset = -90.0f;

	/** Distance in cm at which the zombie can land a melee strike */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (ClampMin = "60.0"))
	float MeleeRange = 165.0f;

	/** Damage dealt per melee strike */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (ClampMin = "1.0"))
	float MeleeDamage = 18.0f;

	/** Seconds between melee strikes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (ClampMin = "0.25"))
	float AttackCooldown = 1.25f;

	/** Max detection distance in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "500.0"))
	float DetectionRange = 6500.0f;

	/** Optional material override applied to the zombie mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Visuals")
	TObjectPtr<UMaterialInterface> ZombieSkinMaterial;

	/** Imported FBX Animation Sequences */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animations")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animations")
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animations")
	TObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animations")
	TObjectPtr<UAnimSequence> AttackAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animations")
	TObjectPtr<UAnimSequence> DeathAnim;

	/** Optional sound played when the zombie lunges / attacks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Audio")
	TObjectPtr<USoundBase> AttackGroanSound;

	/** Gold awarded to the player when this zombie is killed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Economy", meta = (ClampMin = "0"))
	int32 KillGoldReward = 100;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Die() override;

	/** Executes a melee hit on the target player */
	void PerformMeleeAttack(AActor* TargetActor);

	/** Switches single-node animation cleanly and scales play rate to actual ground speed */
	void UpdateLocomotionAnimation();

private:
	float LastAttackTime = -100.0f;
	float ActionAnimEndTime = 0.0f;
	TObjectPtr<UAnimSequence> CurrentLoopAnim;
};
