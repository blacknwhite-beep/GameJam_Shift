// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Variant_Shooter/AI/ZombieAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"

UZombieAnimInstance::UZombieAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Proxy(this)
{
}

void UZombieAnimInstance::PlayZombieSequence(UAnimSequence* NewSequence, bool bLooping, float PlayRate)
{
	if (DesiredSequence != NewSequence || bDesiredLooping != bLooping || !bLooping)
	{
		DesiredSequence = NewSequence;
		bDesiredLooping = bLooping;
		bSequenceChanged = true;
	}
	DesiredPlayRate = PlayRate;
}

void UZombieAnimInstance::SetZombiePlayRate(float PlayRate)
{
	DesiredPlayRate = PlayRate;
}

void UZombieAnimInstance::SetHeadLookAngles(bool bEnable, bool bCrawling, float InPitchDeg, float InYawDeg)
{
	bDesiredEnableHeadLook = bEnable;
	bDesiredCrawling = bCrawling;
	TargetPitchDeg = InPitchDeg;
	TargetYawDeg = InYawDeg;
}

void FZombieAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	Super::Initialize(InAnimInstance);
	UpdateCounter.Increment();
}

void FZombieAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	Super::PreUpdate(InAnimInstance, DeltaSeconds);

	if (UZombieAnimInstance* ZAnim = Cast<UZombieAnimInstance>(InAnimInstance))
	{
		if (ZAnim->bSequenceChanged || ActiveSequence != ZAnim->DesiredSequence)
		{
			ActiveSequence = ZAnim->DesiredSequence;
			bSequenceLooping = ZAnim->bDesiredLooping;
			SequenceTime = 0.0f;
			ZAnim->bSequenceChanged = false;
		}
		SequencePlayRate = ZAnim->DesiredPlayRate;
		bEnableHeadLook = ZAnim->bDesiredEnableHeadLook;
		bIsCrawling = ZAnim->bDesiredCrawling;
		DesiredPitchDeg = ZAnim->TargetPitchDeg;
		DesiredYawDeg = ZAnim->TargetYawDeg;
	}
}

void FZombieAnimInstanceProxy::Update(float DeltaSeconds)
{
	UpdateCounter.Increment();

	if (!ActiveSequence)
	{
		return;
	}

	const float SeqLen = ActiveSequence->GetPlayLength();
	if (SeqLen <= KINDA_SMALL_NUMBER)
	{
		SequenceTime = 0.0f;
		return;
	}

	SequenceTime += DeltaSeconds * SequencePlayRate;
	if (bSequenceLooping)
	{
		SequenceTime = FMath::Fmod(SequenceTime, SeqLen);
		if (SequenceTime < 0.0f)
		{
			SequenceTime += SeqLen;
		}
	}
	else
	{
		SequenceTime = FMath::Clamp(SequenceTime, 0.0f, SeqLen);
	}
}

void FZombieAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	UpdateCounter.Increment();
}

bool FZombieAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (!ActiveSequence)
	{
		Output.ResetToRefPose();
		return true;
	}

	// 1. Evaluate the active zombie animation sequence into the official FCompactPose
	FAnimationPoseData PoseData(Output);
	ActiveSequence->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(SequenceTime), false));

	// 2. Apply pre-finalization local-bone-space look-at arch across Spine1, Spine2, Neck, and Head!
	if (!bEnableHeadLook && !bIsCrawling && FMath::Abs(SmoothedPitchUpDeg) < 0.25f && FMath::Abs(SmoothedYawDeg) < 0.25f)
	{
		return true;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkel = BoneContainer.GetReferenceSkeleton();

	const auto GetCompactIdx = [&](const TCHAR* BoneName) -> FCompactPoseBoneIndex
	{
		const int32 MeshIdx = RefSkel.FindBoneIndex(FName(BoneName));
		return (MeshIdx != INDEX_NONE)
			? BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIdx))
			: FCompactPoseBoneIndex(INDEX_NONE);
	};

	const FCompactPoseBoneIndex Spine1Idx = GetCompactIdx(TEXT("Zombie_Spine1"));
	const FCompactPoseBoneIndex Spine2Idx = GetCompactIdx(TEXT("Zombie_Spine2"));
	const FCompactPoseBoneIndex NeckIdx   = GetCompactIdx(TEXT("Zombie_Neck"));
	const FCompactPoseBoneIndex HeadIdx   = GetCompactIdx(TEXT("Zombie_Head"));

	if (HeadIdx == INDEX_NONE && NeckIdx == INDEX_NONE)
	{
		return true;
	}

	// While crawling belly-down (mesh tilted forward 64 deg + walk animation 30 deg hunch = 94 deg face-down),
	// we arch the upper spine, neck, and head UP by 94 + DesiredPitchDeg (100..135 deg total upward crane)
	// so the zombie's chest, neck, and face lift right off the dirt and stare straight at the player!
	// When bEnableHeadLook turns false on death:
	//   - Crawling zombies smoothly slump their raised head/chest down onto the dirt (+16 deg neutral rest so face lies flush with belly)
	//   - Standing zombies smoothly relax look-at back to 0 deg so DeathAnim plays cleanly
	float TargetTotalPitchUpDeg = 0.0f;
	float TargetTotalYawDeg = 0.0f;
	float InterpSpeed = 10.0f;
	if (bEnableHeadLook)
	{
		TargetTotalPitchUpDeg = bIsCrawling
			? FMath::Clamp(94.0f + DesiredPitchDeg, 96.0f, 136.0f)
			: FMath::Clamp(18.0f + DesiredPitchDeg, -25.0f, 65.0f);
		TargetTotalYawDeg = FMath::Clamp(DesiredYawDeg, -65.0f, 65.0f);
	}
	else
	{
		TargetTotalPitchUpDeg = bIsCrawling ? 16.0f : 0.0f;
		TargetTotalYawDeg = 0.0f;
		InterpSpeed = 7.5f;
	}

	SmoothedPitchUpDeg = FMath::FInterpTo(SmoothedPitchUpDeg, TargetTotalPitchUpDeg, 0.016f, InterpSpeed);
	SmoothedYawDeg     = FMath::FInterpTo(SmoothedYawDeg, TargetTotalYawDeg, 0.016f, InterpSpeed);

	// On SK_Zombie (confirmed in Skeleton Editor):
	//   Local +X (Red arrow)   = Left ear/shoulder (horizontal pitch hinge: +angle lifts face & spine UP off the floor!)
	//   Local +Y (Green arrow) = Down the spine (yaw/twist axis: turns face left/right toward the player!)
	//   Local +Z (Blue arrow)  = Straight out of the zombie's nose/face!
	const auto ApplyLocalArchAndYaw = [&](FCompactPoseBoneIndex BoneIdx, float PitchWeight, float YawWeight)
	{
		if (BoneIdx == INDEX_NONE)
		{
			return;
		}
		const float PitchRad = FMath::DegreesToRadians(SmoothedPitchUpDeg * PitchWeight);
		const float YawRad   = FMath::DegreesToRadians(SmoothedYawDeg * YawWeight);
		const FQuat PitchQuat(FVector(1.0f, 0.0f, 0.0f), PitchRad);
		const FQuat YawQuat(FVector(0.0f, 1.0f, 0.0f), YawRad);
		FTransform& BoneLocal = Output.Pose[BoneIdx];
		BoneLocal.SetRotation((BoneLocal.GetRotation() * PitchQuat * YawQuat).GetNormalized());
	};

	if (bIsCrawling)
	{
		// Distribute the +100..+135 deg upward arch across Spine1 + Spine2 + Neck + Head so the entire upper torso, neck, and head lift off the ground
		ApplyLocalArchAndYaw(Spine1Idx, 0.22f, 0.00f);
		ApplyLocalArchAndYaw(Spine2Idx, 0.26f, 0.20f);
		ApplyLocalArchAndYaw(NeckIdx,   0.28f, 0.40f);
		ApplyLocalArchAndYaw(HeadIdx,   0.24f, 0.40f);
	}
	else
	{
		// Standing: distribute look-at pitch and yaw across Spine2 + Neck + Head
		ApplyLocalArchAndYaw(Spine2Idx, 0.20f, 0.15f);
		ApplyLocalArchAndYaw(NeckIdx,   0.40f, 0.40f);
		ApplyLocalArchAndYaw(HeadIdx,   0.40f, 0.45f);
	}

	if ((++DebugEvalCounter % 180) == 1 && HeadIdx != INDEX_NONE)
	{
		FCSPose<FCompactPose> CheckCS;
		CheckCS.InitPose(Output.Pose);
		const FTransform HeadCS = CheckCS.GetComponentSpaceTransform(HeadIdx);
		const FVector FaceCS = HeadCS.GetRotation().RotateVector(FVector(0.0f, 0.0f, 1.0f));
		UE_LOG(LogTemp, Warning, TEXT("[ZOMBIE_HEADLOOK] Crawl=%d PitchUp=%.1f Yaw=%.1f HeadCS=(%.1f, %.1f, %.1f) FaceCS=(%.2f, %.2f, %.2f)"),
			bIsCrawling ? 1 : 0,
			SmoothedPitchUpDeg,
			SmoothedYawDeg,
			HeadCS.GetLocation().X, HeadCS.GetLocation().Y, HeadCS.GetLocation().Z,
			FaceCS.X, FaceCS.Y, FaceCS.Z);
	}

	return true;
}
