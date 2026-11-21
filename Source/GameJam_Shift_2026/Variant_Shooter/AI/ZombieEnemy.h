// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "ZombieEnemy.generated.h"

class UAnimSequence;
class USoundBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;

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

	/** Play-rate multiplier for the death animation (the imported death clips are long, so this speeds them up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Visuals", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float DeathPlayRate = 2.2f;

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

	/** Max distance in cm at which the zombie can see the player */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "500.0"))
	float DetectionRange = 2500.0f;

	/** Half-angle in degrees of the zombie's field of view; the player must be inside this cone to be seen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "10.0", ClampMax = "180.0"))
	float SightHalfAngle = 70.0f;

	/** Seconds the zombie keeps going to the last place it saw the player before it gives up and roams */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "0.0"))
	float LoseSightDelay = 3.0f;

	/** Radius in cm around the zombie used to pick roam destinations on the navmesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "100.0"))
	float RoamRadius = 1500.0f;

	/** Fraction of ZombieMoveSpeed used while roaming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RoamSpeedScale = 0.5f;

	/** Seconds a roaming zombie pauses between destinations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|AI", meta = (ClampMin = "0.0"))
	float RoamPauseSeconds = 2.0f;

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

	/**
	 * Applies a Tom Looman-style 3D sphere-mask wound on the zombie skin shader at the exact bullet impact location,
	 * plus CodeLikeMe-style limb dismemberment (head, arms, legs -> crawling) & arterial blood bursts!
	 */
	UFUNCTION(BlueprintCallable, Category = "Zombie|Combat")
	void ApplyBulletWound(const FVector& WorldHitLocation, const FVector& ShotDirection);

	/** True once either leg has been blown off and the zombie is crawling on the ground */
	UFUNCTION(BlueprintPure, Category = "Zombie|Combat")
	bool IsCrawling() const { return bIsCrawling; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Die() override;

	/** Evaluates whether the bullet ray hit a dismemberable bone (Head, Arm, Leg) and severs it cleanly */
	void TryDismemberAtHit(const FVector& WorldHitLocation, const FVector& ShotDirection);

	/** Hides SeverBone (zero mesh stretching), attaches a bloody stump cap, and spawns a flying physics limb gib */
	void DismemberLimb(FName SeverBone, FName StumpSocketBone, bool bIsHead, bool bIsLeg, const FVector& ShotDirection);

	/** Transitions the zombie into a belly-down ground crawl when a leg is blown off */
	void EnterCrawlState();

	/** Spawns a physics-simulating severed limb piece that tumbles across the ground */
	void SpawnSeveredLimbGib(const FVector& SpawnLoc, const FRotator& SpawnRot, bool bIsHead, bool bIsLeg, const FVector& ShotDirection);

	/** Attaches a gory flesh-and-bone stump cap to the parent joint socket */
	void AttachStumpCap(FName SocketBone, bool bIsHead);

	/** Executes a melee hit on the target player */
	void PerformMeleeAttack(AActor* TargetActor);

	/** Switches single-node animation cleanly and scales play rate to actual ground speed */
	void UpdateLocomotionAnimation();

	/** True if the player is within sight range, inside the field of view, and not blocked by geometry */
	bool CanSeePlayer(const APawn* PlayerPawn) const;

	/** Picks random navmesh destinations near the zombie and walks between them */
	void RoamAround(float Now);

	void StopAIMovement();

	/** Turns the zombie toward its current movement direction */
	void FaceVelocity(float DeltaTime);

private:
	float LastAttackTime = -100.0f;
	float ActionAnimEndTime = 0.0f;
	TObjectPtr<UAnimSequence> CurrentLoopAnim;
	float CurrentLocoPlayRate = 1.0f;

	bool bAwarePlayer = false;
	bool bCanSeePlayer = false;
	bool bMemoryMoveIssued = false;
	bool bHasRoamTarget = false;
	float LastSeenTime = -100.0f;
	float NextRoamTime = 0.0f;
	float RoamTargetSetTime = 0.0f;
	FVector LastKnownPlayerLocation = FVector::ZeroVector;
	FVector RoamTarget = FVector::ZeroVector;

	/** Dynamic material instances for each skeletal mesh slot so per-zombie 3D wound spheres are rendered */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ZombieWoundMIDs;

	/** Next wound slot index (cycles 0..3 across the 4 shader wound channels HitPosition0..3) */
	int32 NextWoundSlotIndex = 0;

	/** CodeLikeMe-style dismemberment tracking */
	bool bHeadSevered = false;
	bool bLeftArmSevered = false;
	bool bRightArmSevered = false;
	bool bLeftLegSevered = false;
	bool bRightLegSevered = false;
	bool bIsCrawling = false;
	float CrawlBlendAlpha = 0.0f;
	float CrawlRockTime = 0.0f;
	float DefaultMeshRelativeZ = -90.0f;
};
