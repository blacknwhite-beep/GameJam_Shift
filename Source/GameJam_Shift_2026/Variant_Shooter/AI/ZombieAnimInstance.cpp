// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Variant_Shooter/AI/ZombieAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"

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

void UZombieAnimInstance::SetHeadLookTarget(bool bEnable, bool bCrawling, const FVector& InTargetEyeCS)
{
	bDesiredEnableHeadLook = bEnable;
	bDesiredCrawling = bCrawling;
	DesiredTargetEyeCS = InTargetEyeCS;
}

void FZombieAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	Super::PreUpdate(InAnimInstance, DeltaSeconds);

	if (UZombieAnimInstance* ZAnim = Cast<UZombieAnimInstance>(InAnimInstance))
	{
		if (ZAnim->bSequenceChanged)
		{
			ActiveSequence = ZAnim->DesiredSequence;
			bSequenceLooping = ZAnim->bDesiredLooping;
			SequenceTime = 0.0f;
			ZAnim->bSequenceChanged = false;
		}
		SequencePlayRate = ZAnim->DesiredPlayRate;
		bEnableHeadLook = ZAnim->bDesiredEnableHeadLook;
		bIsCrawling = ZAnim->bDesiredCrawling;
		TargetEyeCS = ZAnim->DesiredTargetEyeCS;
	}
}

void FZombieAnimInstanceProxy::Update(float DeltaSeconds)
{
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

	// 2. Apply pre-finalization component-space head & neck look-at so GPU skinning always stares at the player!
	if (!bEnableHeadLook)
	{
		return true;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkel = BoneContainer.GetReferenceSkeleton();
	const int32 HeadMeshIdx = RefSkel.FindBoneIndex(FName("Zombie_Head"));
	const int32 NeckMeshIdx = RefSkel.FindBoneIndex(FName("Zombie_Neck"));

	if (HeadMeshIdx == INDEX_NONE)
	{
		return true;
	}

	const FCompactPoseBoneIndex HeadCompactIdx = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(HeadMeshIdx));
	if (HeadCompactIdx == INDEX_NONE)
	{
		return true;
	}

	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);

	const FTransform HeadCS = CSPose.GetComponentSpaceTransform(HeadCompactIdx);
	const FVector DesiredDirCS = (TargetEyeCS - HeadCS.GetLocation()).GetSafeNormal();
	if (DesiredDirCS.IsNearlyZero())
	{
		return true;
	}

	// On SK_Zombie, Zombie_Head's local +Z (Blue arrow) points straight out of the zombie's nose/face!
	const FVector CurrentFaceCS = HeadCS.GetRotation().RotateVector(FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal();
	FQuat DesiredDeltaQuat = FQuat::FindBetweenNormals(CurrentFaceCS, DesiredDirCS);

	// Clamp crane angle: 140 deg while crawling belly-down so head cranes all the way up at the player; 95 deg standing
	FVector Axis;
	float AngleRad = 0.0f;
	DesiredDeltaQuat.ToAxisAndAngle(Axis, AngleRad);
	const float MaxAngleRad = FMath::DegreesToRadians(bIsCrawling ? 140.0f : 95.0f);
	if (AngleRad > MaxAngleRad)
	{
		DesiredDeltaQuat = FQuat(Axis, MaxAngleRad);
	}

	SmoothedHeadLookQuat = FQuat::Slerp(SmoothedHeadLookQuat, DesiredDeltaQuat, 0.25f);
	SmoothedHeadLookQuat.Normalize();

	const FCompactPoseBoneIndex NeckCompactIdx = (NeckMeshIdx != INDEX_NONE)
		? BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(NeckMeshIdx))
		: FCompactPoseBoneIndex(INDEX_NONE);

	if (NeckCompactIdx != INDEX_NONE)
	{
		const FQuat NeckDelta = FQuat::Slerp(FQuat::Identity, SmoothedHeadLookQuat, 0.45f);
		const FQuat HeadDelta = FQuat::Slerp(FQuat::Identity, SmoothedHeadLookQuat, 0.55f);

		FTransform NeckCS = CSPose.GetComponentSpaceTransform(NeckCompactIdx);
		const FVector NeckPosCS = NeckCS.GetLocation();
		NeckCS.SetRotation((NeckDelta * NeckCS.GetRotation()).GetNormalized());
		CSPose.SetComponentSpaceTransform(NeckCompactIdx, NeckCS);

		// Re-fetch HeadCS after Neck rotation and apply the remaining 55% Head crane
		FTransform UpdatedHeadCS = CSPose.GetComponentSpaceTransform(HeadCompactIdx);
		UpdatedHeadCS.SetRotation((HeadDelta * UpdatedHeadCS.GetRotation()).GetNormalized());
		CSPose.SetComponentSpaceTransform(HeadCompactIdx, UpdatedHeadCS);
	}
	else
	{
		FTransform NewHeadCS = HeadCS;
		NewHeadCS.SetRotation((SmoothedHeadLookQuat * NewHeadCS.GetRotation()).GetNormalized());
		CSPose.SetComponentSpaceTransform(HeadCompactIdx, NewHeadCS);
	}

	// Convert the modified component-space pose cleanly back to local-space Output.Pose!
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(CSPose, Output.Pose);
	return true;
}
