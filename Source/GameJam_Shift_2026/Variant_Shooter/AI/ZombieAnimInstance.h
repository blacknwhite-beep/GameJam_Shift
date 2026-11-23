// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ZombieAnimInstance.generated.h"

class UAnimSequence;

/**
 * AnimInstanceProxy that evaluates the zombie's active AnimSequence and applies
 * a direct pre-finalization local-bone-space look-at arch across Zombie_Spine1,
 * Zombie_Spine2, Zombie_Neck, and Zombie_Head in Output.Pose so the GPU skinning
 * matrices ALWAYS lift and point the zombie's chest, neck, and head toward the
 * player — especially while crawling belly-down!
 */
USTRUCT()
struct FZombieAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FZombieAnimInstanceProxy() = default;
	explicit FZombieAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveSequence;

	float SequenceTime = 0.0f;
	float SequencePlayRate = 1.0f;
	bool bSequenceLooping = true;

	/** Head look-at state copied safely on game thread in PreUpdate */
	bool bEnableHeadLook = false;
	bool bIsCrawling = false;
	float DesiredPitchDeg = 0.0f;
	float DesiredYawDeg = 0.0f;

	float SmoothedPitchUpDeg = 18.0f;
	float SmoothedYawDeg = 0.0f;
	int32 DebugEvalCounter = 0;
};

/**
 * Custom C++ UAnimInstance for AZombieEnemy.
 * Evaluates single AnimSequences and bends the upper spine, neck, and head cleanly
 * inside the official UE5 animation evaluation pass before FinalizeBoneTransform().
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UZombieAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UZombieAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Plays a new AnimSequence (looping or one-shot) at the specified play rate */
	void PlayZombieSequence(UAnimSequence* NewSequence, bool bLooping, float PlayRate = 1.0f);

	/** Updates the playback rate of the currently playing sequence */
	void SetZombiePlayRate(float PlayRate);

	/** Updates the head look-at target angles (Pitch above horizon & Yaw toward player in degrees) and crawl flag */
	void SetHeadLookAngles(bool bEnable, bool bCrawling, float InPitchDeg, float InYawDeg);

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

	bool bDesiredEnableHeadLook = true;
	bool bDesiredCrawling = false;
	float TargetPitchDeg = 15.0f;
	float TargetYawDeg = 0.0f;

	friend struct FZombieAnimInstanceProxy;
};
