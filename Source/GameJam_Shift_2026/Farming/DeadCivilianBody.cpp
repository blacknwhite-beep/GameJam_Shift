// Copyright GameJam Shift 2026. All Rights Reserved.

#include "DeadCivilianBody.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

ADeadCivilianBody::ADeadCivilianBody()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootScene);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(true);

	// Generous pickup hitbox covering the lying corpse for easy Right-Click pickup in Farm
	PickupBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PickupBounds"));
	PickupBounds->SetupAttachment(RootScene);
	PickupBounds->SetBoxExtent(FVector(95.0f, 60.0f, 38.0f));
	PickupBounds->SetRelativeLocation(FVector(0.0f, 0.0f, 24.0f));
	PickupBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupBounds->SetCollisionObjectType(ECC_WorldDynamic);
	PickupBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupBounds->SetCanEverAffectNavigation(false);
}

void ADeadCivilianBody::BeginPlay()
{
	Super::BeginPlay();

	if (!bInitialized)
	{
		InitializeRandomCorpse();
	}
}

void ADeadCivilianBody::InitializeRandomCorpse()
{
	if (bInitialized || !BodyMesh)
	{
		return;
	}
	bInitialized = true;

	// 1. Randomly pick between our 2 civilian NPC variations (50/50)
	const bool bUseBlueShorts = FMath::RandBool();
	const TCHAR* MeshPath = bUseBlueShorts
		? TEXT("/Game/Dev_Blueprint/Characters/Civilians/SKM_Civilian_BlueShirt_Shorts.SKM_Civilian_BlueShirt_Shorts")
		: TEXT("/Game/Dev_Blueprint/Characters/Civilians/SKM_Civilian_RedShirt_Jeans.SKM_Civilian_RedShirt_Jeans");

	if (USkeletalMesh* CivMesh = LoadObject<USkeletalMesh>(nullptr, MeshPath))
	{
		BodyMesh->SetSkeletalMesh(CivMesh, true);
	}

	// 2. Apply one of 5 varied lying dead poses
	LyingPoseIndex = FMath::RandRange(0, 4);
	ApplyLyingPose(LyingPoseIndex);

	// 3. Randomly sever head and/or 1-2 limbs with gory crimson + pale bone stumps
	ApplyRandomDismemberment();

	// 4. Attach zombie-style bullet hole wound markers onto intact body bones
	SpawnBulletHoleMarkers();

	// 5. Place a glossy crimson blood pool underneath the body on the farm ground
	SpawnGroundBloodPool();
}

void ADeadCivilianBody::ApplyLyingPose(int32 PoseIdx)
{
	if (!BodyMesh)
	{
		return;
	}

	const TCHAR* AnimPath = TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_death.anim_Farmer_death");
	float NormalizedTime = 0.95f;
	FVector MeshRelLoc(0.0f, 0.0f, 4.0f);
	FRotator MeshRelRot(0.0f, 0.0f, 0.0f);

	switch (PoseIdx % 5)
	{
	case 0:
		// Sprawled flat from death animation final frame
		AnimPath = TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_death.anim_Farmer_death");
		NormalizedTime = FMath::FRandRange(0.88f, 1.0f);
		MeshRelLoc = FVector(0.0f, 0.0f, 4.0f);
		MeshRelRot = FRotator(0.0f, 0.0f, 0.0f);
		break;

	case 1:
		// Face-down slumped on the dirt
		AnimPath = TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_idle_basic.anim_Farmer_idle_basic");
		NormalizedTime = 0.25f;
		MeshRelLoc = FVector(-75.0f, 0.0f, 14.0f);
		MeshRelRot = FRotator(-88.0f, 0.0f, 0.0f);
		break;

	case 2:
		// Flat on back looking up at the sky
		AnimPath = TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_idle_basic_02.anim_Farmer_idle_basic_02");
		NormalizedTime = 0.45f;
		MeshRelLoc = FVector(75.0f, 0.0f, 14.0f);
		MeshRelRot = FRotator(88.0f, 0.0f, 0.0f);
		break;

	case 3:
		// Curled on left side (recoil pose frozen)
		AnimPath = TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_hit_reaction.anim_Farmer_hit_reaction");
		NormalizedTime = 0.55f;
		MeshRelLoc = FVector(0.0f, -72.0f, 16.0f);
		MeshRelRot = FRotator(8.0f, 0.0f, 88.0f);
		break;

	case 4:
	default:
		// Crumpled on right side (hunched forward)
		AnimPath = TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_grab_low.anim_Farmer_grab_low");
		NormalizedTime = 0.50f;
		MeshRelLoc = FVector(0.0f, 72.0f, 16.0f);
		MeshRelRot = FRotator(-10.0f, 0.0f, -88.0f);
		break;
	}

	BodyMesh->SetRelativeLocation(MeshRelLoc);
	BodyMesh->SetRelativeRotation(MeshRelRot);
	BodyMesh->SetRelativeScale3D(FVector(1.0f));

	if (UAnimSequence* PoseAnim = LoadObject<UAnimSequence>(nullptr, AnimPath))
	{
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BodyMesh->SetAnimation(PoseAnim);
		const float PlayLen = PoseAnim->GetPlayLength();
		BodyMesh->SetPosition(FMath::Clamp(PlayLen * NormalizedTime, 0.0f, PlayLen), false);
		BodyMesh->SetPlayRate(0.0f);
		BodyMesh->Play(false);
		BodyMesh->TickAnimation(0.0f, false);
		BodyMesh->RefreshBoneTransforms();
	}
}

void ADeadCivilianBody::ApplyRandomDismemberment()
{
	if (!BodyMesh)
	{
		return;
	}

	SeveredBones.Reset();

	// ~35% chance missing head
	if (FMath::FRand() < 0.35f)
	{
		if (BodyMesh->GetBoneIndex(FName("head")) != INDEX_NONE)
		{
			BodyMesh->HideBoneByName(FName("head"), PBO_Term);
			SeveredBones.Add(FName("head"));
			AttachStumpCap(FName("neck_01"), true);
		}
	}

	// ~55% chance missing 1 or 2 random limbs
	if (FMath::FRand() < 0.55f)
	{
		struct FLimbDef
		{
			FName SeverBone;
			FName ParentBone;
		};

		TArray<FLimbDef> LimbPool = {
			{ FName("upperarm_l"), FName("clavicle_l") },
			{ FName("lowerarm_l"), FName("upperarm_l") },
			{ FName("upperarm_r"), FName("clavicle_r") },
			{ FName("lowerarm_r"), FName("upperarm_r") },
			{ FName("calf_l"),     FName("thigh_l") },
			{ FName("calf_r"),     FName("thigh_r") },
			{ FName("thigh_l"),    FName("pelvis") },
			{ FName("thigh_r"),    FName("pelvis") }
		};

		const int32 NumLimbsToSever = (FMath::FRand() < 0.38f) ? 2 : 1;
		for (int32 i = 0; i < NumLimbsToSever && LimbPool.Num() > 0; ++i)
		{
			const int32 PickIdx = FMath::RandRange(0, LimbPool.Num() - 1);
			const FLimbDef Pick = LimbPool[PickIdx];
			LimbPool.RemoveAtSwap(PickIdx);

			if (SeveredBones.Contains(Pick.SeverBone) || SeveredBones.Contains(Pick.ParentBone))
			{
				continue;
			}
			if (BodyMesh->GetBoneIndex(Pick.SeverBone) != INDEX_NONE)
			{
				BodyMesh->HideBoneByName(Pick.SeverBone, PBO_Term);
				SeveredBones.Add(Pick.SeverBone);
				AttachStumpCap(Pick.ParentBone, false);
			}
		}
	}
}

void ADeadCivilianBody::AttachStumpCap(FName ParentBone, bool bIsHead)
{
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!BodyMesh || !SphereMesh || !BaseMat || BodyMesh->GetBoneIndex(ParentBone) == INDEX_NONE)
	{
		return;
	}

	// Exact same glossy crimson flesh + pale bone stump look as AZombieEnemy::AttachStumpCap
	UStaticMeshComponent* FleshCap = NewObject<UStaticMeshComponent>(this);
	FleshCap->SetStaticMesh(SphereMesh);
	FleshCap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FleshCap->SetCastShadow(false);
	const float CapScale = bIsHead ? 0.14f : 0.11f;
	FleshCap->SetRelativeScale3D(FVector(CapScale, CapScale, CapScale * 0.72f));
	if (UMaterialInstanceDynamic* FleshMID = UMaterialInstanceDynamic::Create(BaseMat, this))
	{
		FleshMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.008f, 0.008f, 1.0f));
		FleshMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.06f, 0.002f, 0.002f, 1.0f));
		FleshCap->SetMaterial(0, FleshMID);
	}
	FleshCap->SetupAttachment(BodyMesh, ParentBone);
	FleshCap->RegisterComponent();

	if (CylinderMesh)
	{
		UStaticMeshComponent* BoneStub = NewObject<UStaticMeshComponent>(this);
		BoneStub->SetStaticMesh(CylinderMesh);
		BoneStub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoneStub->SetCastShadow(false);
		const float BoneScale = bIsHead ? 0.045f : 0.036f;
		BoneStub->SetRelativeScale3D(FVector(BoneScale, BoneScale, 0.07f));
		BoneStub->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
		if (UMaterialInstanceDynamic* BoneMID = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			BoneMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.84f, 0.79f, 0.70f, 1.0f));
			BoneMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
			BoneStub->SetMaterial(0, BoneMID);
		}
		BoneStub->SetupAttachment(BodyMesh, ParentBone);
		BoneStub->RegisterComponent();
	}
}

void ADeadCivilianBody::SpawnBulletHoleMarkers()
{
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!BodyMesh || !SphereMesh || !BaseMat)
	{
		return;
	}

	struct FWoundSite
	{
		FName Bone;
		FVector OffsetMin;
		FVector OffsetMax;
	};

	TArray<FWoundSite> Sites = {
		{ FName("spine_03"),   FVector(-10.0f, -12.0f, -8.0f),  FVector(10.0f, 12.0f, 10.0f) },
		{ FName("spine_02"),   FVector(-10.0f, -11.0f, -8.0f),  FVector(10.0f, 11.0f, 8.0f) },
		{ FName("pelvis"),     FVector(-9.0f,  -10.0f, -6.0f),  FVector(9.0f,  10.0f, 6.0f) },
		{ FName("upperarm_l"), FVector(-5.0f,  -5.0f,  -5.0f),  FVector(5.0f,  5.0f,  5.0f) },
		{ FName("upperarm_r"), FVector(-5.0f,  -5.0f,  -5.0f),  FVector(5.0f,  5.0f,  5.0f) },
		{ FName("thigh_l"),    FVector(-6.0f,  -6.0f,  -8.0f),  FVector(6.0f,  6.0f,  8.0f) },
		{ FName("thigh_r"),    FVector(-6.0f,  -6.0f,  -8.0f),  FVector(6.0f,  6.0f,  8.0f) },
		{ FName("head"),       FVector(-5.0f,  -8.0f,  -4.0f),  FVector(5.0f,  8.0f,  6.0f) }
	};

	const int32 NumWounds = FMath::RandRange(3, 6);
	for (int32 i = 0; i < NumWounds; ++i)
	{
		const FWoundSite& Site = Sites[FMath::RandRange(0, Sites.Num() - 1)];
		if (SeveredBones.Contains(Site.Bone) || BodyMesh->GetBoneIndex(Site.Bone) == INDEX_NONE)
		{
			continue;
		}

		const FVector WoundOffset(
			FMath::FRandRange(Site.OffsetMin.X, Site.OffsetMax.X),
			FMath::FRandRange(Site.OffsetMin.Y, Site.OffsetMax.Y),
			FMath::FRandRange(Site.OffsetMin.Z, Site.OffsetMax.Z)
		);

		// Outer crimson arterial bullet wound blotch (matches AZombieEnemy::ApplyBulletWound)
		UStaticMeshComponent* OuterWound = NewObject<UStaticMeshComponent>(this);
		OuterWound->SetStaticMesh(SphereMesh);
		OuterWound->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OuterWound->SetCastShadow(false);
		const float OuterRadius = FMath::FRandRange(0.08f, 0.13f);
		OuterWound->SetRelativeScale3D(FVector(OuterRadius, OuterRadius, OuterRadius));
		OuterWound->SetRelativeLocation(WoundOffset);
		if (UMaterialInstanceDynamic* WoundMID = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			WoundMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.24f, 0.005f, 0.005f, 1.0f));
			WoundMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.05f, 0.001f, 0.001f, 1.0f));
			OuterWound->SetMaterial(0, WoundMID);
		}
		OuterWound->SetupAttachment(BodyMesh, Site.Bone);
		OuterWound->RegisterComponent();

		// Inner dark bullet entry hole
		UStaticMeshComponent* EntryHole = NewObject<UStaticMeshComponent>(this);
		EntryHole->SetStaticMesh(SphereMesh);
		EntryHole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EntryHole->SetCastShadow(false);
		const float InnerRadius = OuterRadius * 0.45f;
		EntryHole->SetRelativeScale3D(FVector(InnerRadius, InnerRadius, InnerRadius * 1.15f));
		EntryHole->SetRelativeLocation(WoundOffset + WoundOffset.GetSafeNormal() * 1.2f);
		if (UMaterialInstanceDynamic* HoleMID = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			HoleMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.025f, 0.003f, 0.003f, 1.0f));
			HoleMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
			EntryHole->SetMaterial(0, HoleMID);
		}
		EntryHole->SetupAttachment(BodyMesh, Site.Bone);
		EntryHole->RegisterComponent();
	}
}

void ADeadCivilianBody::SpawnGroundBloodPool()
{
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!CylinderMesh || !BaseMat)
	{
		return;
	}

	GroundBloodPool = NewObject<UStaticMeshComponent>(this, TEXT("GroundBloodPool"));
	GroundBloodPool->SetStaticMesh(CylinderMesh);
	GroundBloodPool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundBloodPool->SetCastShadow(false);
	GroundBloodPool->SetRelativeLocation(FVector(0.0f, 0.0f, 0.8f));
	GroundBloodPool->SetRelativeRotation(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
	GroundBloodPool->SetRelativeScale3D(FVector(FMath::FRandRange(0.55f, 0.95f), FMath::FRandRange(0.45f, 0.85f), 0.008f));

	if (UMaterialInstanceDynamic* PoolMID = UMaterialInstanceDynamic::Create(BaseMat, this))
	{
		PoolMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.20f, 0.005f, 0.005f, 1.0f));
		PoolMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.04f, 0.001f, 0.001f, 1.0f));
		GroundBloodPool->SetMaterial(0, PoolMID);
	}

	GroundBloodPool->SetupAttachment(RootScene);
	GroundBloodPool->RegisterComponent();
}

void ADeadCivilianBody::PickUpToShoulder(USceneComponent* CameraComponent)
{
	if (!CameraComponent || !BodyMesh)
	{
		return;
	}

	bIsCarried = true;

	if (PickupBounds)
	{
		PickupBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GroundBloodPool)
	{
		GroundBloodPool->SetVisibility(false);
	}

	// Slightly slumped limp pose while slung over the player's left shoulder
	if (UAnimSequence* SlumpAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_grab_low.anim_Farmer_grab_low")))
	{
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BodyMesh->SetAnimation(SlumpAnim);
		BodyMesh->SetPosition(SlumpAnim->GetPlayLength() * 0.55f, false);
		BodyMesh->SetPlayRate(0.0f);
		BodyMesh->Play(false);
		BodyMesh->TickAnimation(0.0f, false);
		BodyMesh->RefreshBoneTransforms();
	}

	BodyMesh->SetCastShadow(false);

	// Attach directly to the First-Person Camera on the LEFT-hand side like resting on the left shoulder
	AttachToComponent(CameraComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector::ZeroVector);
	SetActorRelativeRotation(FRotator::ZeroRotator);

	// Left-shoulder carry transform in camera space:
	// +X = forward, -Y = left side of screen, -Z = shoulder height
	BodyMesh->SetRelativeLocation(FVector(42.0f, -34.0f, -26.0f));
	BodyMesh->SetRelativeRotation(FRotator(-84.0f, 168.0f, -14.0f));
	BodyMesh->SetRelativeScale3D(FVector(0.88f));
}

void ADeadCivilianBody::DropToGround(const FVector& DropLocation, float DropYaw)
{
	bIsCarried = false;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(DropLocation);
	SetActorRotation(FRotator(0.0f, DropYaw, 0.0f));

	if (BodyMesh)
	{
		BodyMesh->SetCastShadow(true);
	}
	ApplyLyingPose(LyingPoseIndex);

	if (PickupBounds)
	{
		PickupBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	if (GroundBloodPool)
	{
		GroundBloodPool->SetVisibility(true);
	}
}
