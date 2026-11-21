// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ZombieAnimInstance.generated.h"

class UAnimSequence;

/**
 * AnimInstanceProxy that evaluates the zombie's active AnimSequence and applies
 * a genuine pre-finalization component-space look-at rotation to Zombie_Neck and
 * Zombie_Head via FCSPose<FCompactPose> so the GPU skinning matrices ALWAYS point
 * the zombie's head toward the player — including while crawling belly-down!
 */
USTRUCT()
struct FZombieAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FZombieAnimInstanceProxy() = default;
	explicit FZombieAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveSequence;

	float SequenceTime = 0.0f;
	float SequencePlayRate = 1.0f;
	bool bSequenceLooping = true;

	/** Head look-at state copied safely on game thread in PreUpdate */
	bool bEnableHeadLook = false;
	bool bIsCrawling = false;
	FVector TargetEyeCS = FVector(0.0f, 300.0f, 150.0f);
	FQuat SmoothedHeadLookQuat = FQuat::Identity;
};

/**
 * Custom C++ UAnimInstance for AZombieEnemy.
 * Replaces AnimationSingleNode so we can modify Zombie_Neck & Zombie_Head cleanly
 * inside the official UE5 animation evaluation pass before FinalizeBoneTransform().
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UZombieAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Plays a new AnimSequence (looping or one-shot) at the specified play rate */
	void PlayZombieSequence(UAnimSequence* NewSequence, bool bLooping, float PlayRate = 1.0f);

	/** Updates the playback rate of the currently playing sequence */
	void SetZombiePlayRate(float PlayRate);

	/** Updates the head look-at target (in SkelMesh component space) and crawl flag */
	void SetHeadLookTarget(bool bEnable, bool bCrawling, const FVector& InTargetEyeCS);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return &Proxy; }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override {}

private:
	UPROPERTY(Transient)
	FZombieAnimInstanceProxy Proxy;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> DesiredSequence;

	bool bDesiredLooping = true;
	float DesiredPlayRate = 1.0f;
	bool bSequenceChanged = false;

	bool bDesiredEnableHeadLook = false;
	bool bDesiredCrawling = false;
	FVector DesiredTargetEyeCS = FVector(0.0f, 300.0f, 150.0f);

	friend struct FZombieAnimInstanceProxy;
};
