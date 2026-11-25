# Exact C++ Reference: Planting Animation Implementation (`ShooterCharacter.cpp`)

From `GameJam_Shift_2026` commit `1538f30` (`Source/GameJam_Shift_2026/Variant_Shooter/ShooterCharacter.cpp`).

## 1. Includes
```cpp
#include "Components/PrimitiveComponent.h"
#include "Variant_Shooter/AI/ZombieAnimInstance.h"
```

## 2. Animation Instance & Visibility Helpers (Anonymous Namespace)
```cpp
UZombieAnimInstance* EnsureFarmerAnimInstance(USkeletalMeshComponent* BodyMesh)
{
	if (!BodyMesh || !BodyMesh->GetSkeletalMeshAsset())
	{
		return nullptr;
	}
	BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	BodyMesh->bEnableUpdateRateOptimizations = false;

	UZombieAnimInstance* FAnim = Cast<UZombieAnimInstance>(BodyMesh->GetAnimInstance());
	if (BodyMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint || !FAnim)
	{
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		BodyMesh->SetAnimInstanceClass(UZombieAnimInstance::StaticClass());
		FAnim = Cast<UZombieAnimInstance>(BodyMesh->GetAnimInstance());
	}
	if (FAnim)
	{
		FAnim->SetHeadLookAngles(false, false, 0.0f, 0.0f);
	}
	return FAnim;
}

void PlayFarmerAnimSafe(USkeletalMeshComponent* BodyMesh, UAnimSequence* Anim, bool bLooping, float PlayRate)
{
	if (!BodyMesh || !Anim)
	{
		return;
	}
	if (UZombieAnimInstance* FAnim = EnsureFarmerAnimInstance(BodyMesh))
	{
		FAnim->PlayZombieSequence(Anim, bLooping, PlayRate);
	}
}

void ApplyFarmerViewVisibility(AShooterCharacter* Character, bool bShowBodyToOwner)
{
	if (!Character)
	{
		return;
	}
	if (USkeletalMeshComponent* Body = Character->GetMesh())
	{
		Body->SetFirstPersonPrimitiveType(
			bShowBodyToOwner
				? EFirstPersonPrimitiveType::None
				: EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		Body->SetOwnerNoSee(!bShowBodyToOwner);
		Body->SetOnlyOwnerSee(false);
		Body->SetHiddenInGame(false, false);
		Body->SetVisibility(true, false);
		Body->SetCastShadow(true);
		Body->bCastHiddenShadow = true;
		Body->UnHideBoneByName(FName("Head"));
		Body->MarkRenderStateDirty();
	}
	if (USkeletalMeshComponent* FP = Character->GetFirstPersonMesh())
	{
		FP->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
		FP->SetVisibility(false, false);
		FP->SetHiddenInGame(true, false);
		FP->SetCastShadow(false);
		FP->bCastHiddenShadow = false;
	}
}
```

## 3. Character Mesh Initialization (`InitializeFarmerCharacterMesh`)
```cpp
BodyMesh->SetAnimInstanceClass(nullptr);
BodyMesh->SetSkeletalMesh(FarmerMesh, true); // bReinitPose = true!
BodyMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));
BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
BodyMesh->bEnableUpdateRateOptimizations = false;
BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
BodyMesh->SetAnimInstanceClass(UZombieAnimInstance::StaticClass());

// Disable auxiliary character SkeletalMeshComponents WITHOUT propagating hidden flags to camera children
TInlineComponentArray<USkeletalMeshComponent*> CharSkelMeshes(this);
for (USkeletalMeshComponent* Comp : CharSkelMeshes)
{
	if (Comp && Comp != BodyMesh && Comp->GetOwner() == this)
	{
		Comp->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
		Comp->SetVisibility(false, false);
		Comp->SetHiddenInGame(true, false);
		Comp->SetCastShadow(false);
		Comp->bCastHiddenShadow = false;
		Comp->SetAnimInstanceClass(nullptr);
		Comp->MarkRenderStateDirty();
	}
}

ApplyFarmerViewVisibility(this, bIsThirdPersonView);
```

## 4. Planting Trigger (`DoStartFiring`)
- Compute `DesiredKneelLoc = PlotLoc - ToPlotDir * 48.0f`, floor-snapped via `ECC_WorldStatic`.
- Hide `TargetPlot->Mesh_Sprout` / `TargetPlot->CropMesh` until `Progress >= 0.45f`.
- Call `BeginPlantThirdPerson(this, ActionState, bIsThirdPersonView);`
- Call `PlayFarmerOneShotAnim(ActionState.PlantAnim.Get(), 1.65f);`
- Set `ActionState.TotalOneShotDuration = FarmerOneShotRemaining;`

## 5. Inside-the-Fence 4-Angle 3/4 Planting Camera (`UpdateFarmerAnimation`)
```cpp
const FVector KneelBase(ActionState.PlantTargetActorLoc.X, ActionState.PlantTargetActorLoc.Y, GroundZ);
const FRotator PlantFacing(0.0f, ActionState.PlantTargetYaw, 0.0f);
const FVector Fwd = FRotationMatrix(PlantFacing).GetUnitAxis(EAxis::X);
const FVector Right = FRotationMatrix(PlantFacing).GetUnitAxis(EAxis::Y);

const float PelvisHeightAboveGround = (BodyMesh->GetBoneIndex(FName("pelvis")) != INDEX_NONE)
	? FMath::Clamp(BodyMesh->GetBoneLocation(FName("pelvis")).Z - GroundZ, 36.0f, 88.0f)
	: 48.0f;

const FVector ActionFocusPoint = KneelBase + Fwd * 24.0f + FVector(0.0f, 0.0f, PelvisHeightAboveGround + 4.0f);
const FVector TraceOrigin = KneelBase + Fwd * 24.0f + FVector(0.0f, 0.0f, 54.0f);

const FVector Candidates[4] = {
	TraceOrigin - Fwd * 110.0f + Right * 122.0f + FVector(0.0f, 0.0f, 62.0f),
	TraceOrigin - Fwd * 110.0f - Right * 122.0f + FVector(0.0f, 0.0f, 62.0f),
	TraceOrigin + Fwd * 92.0f  + Right * 130.0f + FVector(0.0f, 0.0f, 60.0f),
	TraceOrigin + Fwd * 92.0f  - Right * 130.0f + FVector(0.0f, 0.0f, 60.0f)
};
```
Sweep with `FCollisionShape::MakeSphere(15.0f)` on `ECC_Visibility`, ignoring `this` and all `AFarmPlotBase` actors, lock `ActionState.ChosenCamSideIndex` on frame 1, blend `InAlpha = SmoothStep(0, 0.28f, Elapsed)` and `OutAlpha = SmoothStep(0, 0.35f, Remaining)`, and hide `"Head"` only when `Elapsed < 0.05f || Remaining < 0.08f`.
