// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadCivilianBody.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * Dead civilian body spawned in the Farm dimension for every 2 zombies killed in Combat.
 * Randomly selects between the two civilian clothing variations (Red Shirt + Jeans,
 * Blue Shirt + Shorts + Mustache), lies in varied dead poses with zombie-style bullet
 * hole markers, random missing limbs/head with blood stumps, and can be picked up onto
 * the player's left shoulder with Right Click in the Farm world.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API ADeadCivilianBody : public AActor
{
	GENERATED_BODY()

public:
	ADeadCivilianBody();

	/** Initializes mesh variant, lying pose, random dismemberment, and bullet hole wounds. */
	void InitializeRandomCorpse();

	/** Attaches the corpse to the left side of the player's first-person camera like it is slung on their left shoulder. */
	void PickUpToShoulder(USceneComponent* CameraComponent);

	/** Detaches the corpse from the player's shoulder and places it back on the ground at DropLocation. */
	void DropToGround(const FVector& DropLocation, float DropYaw);

	/** Detaches the corpse from the player's shoulder and dumps it permanently into a grave pit. */
	void DumpIntoGrave(const FVector& GraveDropLocation, float GraveYaw);

	bool IsCarried() const { return bIsCarried; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Corpse")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, Category = "Corpse")
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Corpse")
	TObjectPtr<UBoxComponent> PickupBounds;

	/** Saved lying pose index (0..4) so dropping restores a natural dead pose. */
	int32 LyingPoseIndex = 0;

	/** True once randomized setup has run. */
	bool bInitialized = false;

	/** True while slung over the player's left shoulder. */
	bool bIsCarried = false;

	/** Tracks severed bones so bullet wounds are only placed on intact body parts. */
	TSet<FName> SeveredBones;

	void ApplyLyingPose(int32 PoseIdx);
	void ApplyRandomDismemberment();
	void AttachStumpCap(FName ParentBone, bool bIsHead);
	void SpawnBulletHoleMarkers();
	void SpawnGroundBloodPool();
};
