// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterWeapon.h"
#include "ShooterCharacter.h"
#include "DimensionShiftSubsystem.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"

AShooterWeapon::AShooterWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);

	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;

	// create the third person mesh
	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;

	// create custom static mesh components for the 3D Gun Pack models
	FirstPersonGunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonGunMesh"));
	FirstPersonGunMesh->SetupAttachment(FirstPersonMesh);
	FirstPersonGunMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonGunMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonGunMesh->bOnlyOwnerSee = true;
	FirstPersonGunMesh->SetCastShadow(false);

	ThirdPersonGunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonGunMesh"));
	ThirdPersonGunMesh->SetupAttachment(ThirdPersonMesh);
	ThirdPersonGunMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonGunMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonGunMesh->bOwnerNoSee = true;

	// Create 3D crossed-starburst muzzle flash flare components & dynamic muzzle flash point light
	MuzzleFlashCoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlashCoreMesh"));
	MuzzleFlashCoreMesh->SetupAttachment(FirstPersonMesh);
	MuzzleFlashCoreMesh->SetCollisionProfileName(FName("NoCollision"));
	MuzzleFlashCoreMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	MuzzleFlashCoreMesh->bOnlyOwnerSee = true;
	MuzzleFlashCoreMesh->SetCastShadow(false);
	MuzzleFlashCoreMesh->SetVisibility(false);

	MuzzleFlashCrossMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlashCrossMesh"));
	MuzzleFlashCrossMesh->SetupAttachment(MuzzleFlashCoreMesh);
	MuzzleFlashCrossMesh->SetCollisionProfileName(FName("NoCollision"));
	MuzzleFlashCrossMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	MuzzleFlashCrossMesh->bOnlyOwnerSee = true;
	MuzzleFlashCrossMesh->SetCastShadow(false);
	MuzzleFlashCrossMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	MuzzleFlashCrossMesh->SetVisibility(false);

	MuzzleFlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlashLight"));
	MuzzleFlashLight->SetupAttachment(FirstPersonMesh);
	MuzzleFlashLight->SetIntensityUnits(ELightUnits::Candelas);
	MuzzleFlashLight->SetIntensity(140.0f);
	MuzzleFlashLight->SetAttenuationRadius(680.0f);
	MuzzleFlashLight->SetLightColor(FColor(255, 195, 95));
	MuzzleFlashLight->SetCastShadows(false);
	MuzzleFlashLight->SetVisibility(false);

	AutoFireAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AutoFireAudioComp"));
	AutoFireAudioComp->SetupAttachment(FirstPersonMesh);
	AutoFireAudioComp->bAutoActivate = false;

	// Create stylized Cubic Low-Poly Farmer First-Person Hands & Flannel Forearms (matching SKM_Farmer_male)
	auto InitHandPiece = [this](const TCHAR* Name, USceneComponent* Parent) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetCollisionProfileName(FName("NoCollision"));
		Comp->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		Comp->bOnlyOwnerSee = true;
		Comp->SetCastShadow(false);
		return Comp;
	};
	FarmerRightHand = InitHandPiece(TEXT("FarmerRightHand"), FirstPersonMesh);
	FarmerRightArm  = InitHandPiece(TEXT("FarmerRightArm"), FarmerRightHand);
	FarmerLeftHand  = InitHandPiece(TEXT("FarmerLeftHand"), FirstPersonMesh);
	FarmerLeftArm   = InitHandPiece(TEXT("FarmerLeftArm"), FarmerLeftHand);
}

void AShooterWeapon::BeginPlay()
{
	Super::BeginPlay();

	// subscribe to the owner's destroyed delegate
	GetOwner()->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

	// cast the weapon owner
	WeaponOwner = Cast<IShooterWeaponHolder>(GetOwner());
	PawnOwner = Cast<APawn>(GetOwner());

	// Enforce sensible max magazine capacities per weapon archetype
	const FString ClassStr = GetClass()->GetName();
	if (ClassStr.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 15);
	}
	else if (ClassStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Grenade"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Launcher"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 8);
	}
	else if (ClassStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 40);
	}

	// fill the first ammo clip
	CurrentBullets = MagazineSize;

	// attach the meshes to the owner
	WeaponOwner->AttachWeaponMeshes(this);

	// Initialize 3D starburst muzzle flash mesh & emissive material
	if (UStaticMesh* ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")))
	{
		if (MuzzleFlashCoreMesh)
		{
			MuzzleFlashCoreMesh->SetStaticMesh(ConeMesh);
		}
		if (MuzzleFlashCrossMesh)
		{
			MuzzleFlashCrossMesh->SetStaticMesh(ConeMesh);
		}
	}
	if (UMaterialInterface* BaseGlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase")))
	{
		MuzzleFlashMID = UMaterialInstanceDynamic::Create(BaseGlowMat, this);
		if (MuzzleFlashMID)
		{
			MuzzleFlashMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.85f, 0.35f, 1.0f));
			MuzzleFlashMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(28.0f, 16.0f, 3.5f, 1.0f));
			if (MuzzleFlashCoreMesh) { MuzzleFlashCoreMesh->SetMaterial(0, MuzzleFlashMID); }
			if (MuzzleFlashCrossMesh) { MuzzleFlashCrossMesh->SetMaterial(0, MuzzleFlashMID); }
		}
	}

	SetupFarmerHandsVisuals();
}

void AShooterWeapon::SetupFarmerHandsVisuals()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!CubeMesh)
	{
		return;
	}

	UMaterialInstanceDynamic* SkinMID = BaseMat ? UMaterialInstanceDynamic::Create(BaseMat, this) : nullptr;
	if (SkinMID)
	{
		// Warm cubic low-poly Farmer skin tone matching SKM_Farmer_male
		SkinMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.88f, 0.67f, 0.52f, 1.0f));
		SkinMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.03f, 0.02f, 0.015f, 1.0f));
	}

	UMaterialInstanceDynamic* SleeveMID = BaseMat ? UMaterialInstanceDynamic::Create(BaseMat, this) : nullptr;
	if (SleeveMID)
	{
		// Farmer blue denim/flannel sleeve matching SKM_Farmer_male
		SleeveMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.18f, 0.36f, 0.60f, 1.0f));
		SleeveMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.01f, 0.02f, 0.035f, 1.0f));
	}

	if (FarmerRightHand)
	{
		FarmerRightHand->SetStaticMesh(CubeMesh);
		FarmerRightHand->SetRelativeScale3D(FVector(0.044f, 0.068f, 0.064f));
		if (SkinMID) { FarmerRightHand->SetMaterial(0, SkinMID); }
	}
	if (FarmerRightArm)
	{
		FarmerRightArm->SetStaticMesh(CubeMesh);
		FarmerRightArm->SetRelativeLocation(FVector(-65.0f, -210.0f, -55.0f));
		FarmerRightArm->SetRelativeRotation(FRotator(12.0f, 16.0f, 0.0f));
		FarmerRightArm->SetRelativeScale3D(FVector(1.25f, 3.6f, 1.25f));
		if (SleeveMID) { FarmerRightArm->SetMaterial(0, SleeveMID); }
	}
	if (FarmerLeftHand)
	{
		FarmerLeftHand->SetStaticMesh(CubeMesh);
		FarmerLeftHand->SetRelativeScale3D(FVector(0.044f, 0.068f, 0.062f));
		if (SkinMID) { FarmerLeftHand->SetMaterial(0, SkinMID); }
	}
	if (FarmerLeftArm)
	{
		FarmerLeftArm->SetStaticMesh(CubeMesh);
		FarmerLeftArm->SetRelativeLocation(FVector(80.0f, -215.0f, -65.0f));
		FarmerLeftArm->SetRelativeRotation(FRotator(14.0f, -22.0f, 0.0f));
		FarmerLeftArm->SetRelativeScale3D(FVector(1.25f, 3.8f, 1.25f));
		if (SleeveMID) { FarmerLeftArm->SetMaterial(0, SleeveMID); }
	}
}

void AShooterWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsHidden() || !FirstPersonGunMesh || !FirstPersonGunMesh->GetStaticMesh())
	{
		return;
	}

	// 1. Decay procedural recoil spring back to zero
	CurrentRecoilOffset = FMath::VInterpTo(CurrentRecoilOffset, FVector::ZeroVector, DeltaTime, 18.0f);
	CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, 16.0f);

	// 2. Smooth 4-stage procedural reload animation (gun tilt + animated left-hand mag swap & slide/bolt rack)
	FVector ReloadOffset = FVector::ZeroVector;
	FRotator ReloadRot = FRotator::ZeroRotator;
	FVector LeftHandAnimOffset = LeftHandRestOffset;
	FRotator LeftHandAnimRot = bIsPistolArchetype ? FRotator(0.0f, -24.0f, 8.0f) : FRotator(6.0f, -10.0f, -12.0f);

	if (bIsReloading && ReloadDuration > 0.05f)
	{
		ReloadAnimElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(ReloadAnimElapsed / ReloadDuration, 0.0f, 1.0f);

		float DipFactor = 0.0f;
		float BoltRackBack = 0.0f;
		if (Alpha < 0.28f)
		{
			// Stage 1: Tilt gun cleanly to the side while left hand reaches to magwell & pulls empty mag down
			const float T = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha / 0.28f, 2.0f);
			DipFactor = T;
			const FVector MagDropPos = LeftHandMagOffset + FVector(1.0f, -1.5f, -12.0f);
			LeftHandAnimOffset = FMath::Lerp(LeftHandRestOffset, MagDropPos, T);
			LeftHandAnimRot = FRotator(-18.0f * T, -25.0f, 15.0f * T);
		}
		else if (Alpha < 0.68f)
		{
			// Stage 2: Left hand lifts a fresh magazine cleanly up into the magwell with a crisp seat pulse
			const float T = (Alpha - 0.28f) / 0.40f;
			const float SmoothT = FMath::InterpEaseInOut(0.0f, 1.0f, T, 2.0f);
			const FVector MagDropPos = LeftHandMagOffset + FVector(1.0f, -1.5f, -12.0f);
			LeftHandAnimOffset = FMath::Lerp(MagDropPos, LeftHandMagOffset + FVector(0.0f, 0.0f, 2.5f), SmoothT);
			LeftHandAnimRot = FRotator(-10.0f * (1.0f - SmoothT), -22.0f, 10.0f);
			const float SeatJolt = (T > 0.75f) ? FMath::Sin(((T - 0.75f) / 0.25f) * PI) * 0.22f : 0.0f;
			DipFactor = 1.0f - SeatJolt;
		}
		else if (Alpha < 0.86f)
		{
			// Stage 3: Left hand reaches up to the slide/charging handle and racks it backward
			const float T = (Alpha - 0.68f) / 0.18f;
			const float RackPulse = FMath::Sin(T * PI);
			DipFactor = FMath::Lerp(1.0f, 0.35f, T);
			BoltRackBack = RackPulse * 4.5f;
			LeftHandAnimOffset = FMath::Lerp(LeftHandMagOffset, LeftHandRackOffset, FMath::Min(1.0f, T * 1.4f))
				+ FVector(0.0f, -RackPulse * 6.5f, 0.0f);
			LeftHandAnimRot = FRotator(8.0f, -35.0f, -15.0f);
		}
		else
		{
			// Stage 4: Gun snaps back to aim-ready while left hand returns cleanly to the grip/foregrip
			const float T = FMath::InterpEaseInOut(0.0f, 1.0f, (Alpha - 0.86f) / 0.14f, 2.0f);
			DipFactor = FMath::Lerp(0.35f, 0.0f, T);
			LeftHandAnimOffset = FMath::Lerp(LeftHandRackOffset, LeftHandRestOffset, T);
		}

		ReloadOffset = FVector(-1.5f * DipFactor, -BoltRackBack - 3.5f * DipFactor, -7.5f * DipFactor);
		ReloadRot = FRotator(22.0f * DipFactor, -10.0f * DipFactor, -30.0f * DipFactor);
	}
	else
	{
		ReloadAnimElapsed = 0.0f;
	}

	// 3. Quick equip / draw rise animation when switching weapons on the wheel
	FVector EquipOffset = FVector::ZeroVector;
	FRotator EquipRot = FRotator::ZeroRotator;
	if (EquipAnimElapsed < 0.22f)
	{
		EquipAnimElapsed += DeltaTime;
		const float EquipAlpha = 1.0f - FMath::Clamp(EquipAnimElapsed / 0.22f, 0.0f, 1.0f);
		const float Smooth = EquipAlpha * EquipAlpha;
		EquipOffset = FVector(0.0f, -3.0f * Smooth, -9.0f * Smooth);
		EquipRot = FRotator(-18.0f * Smooth, 0.0f, -15.0f * Smooth);
	}

	// 4. Subtle organic breathing sway so the Cubic Farmer hands & gun feel alive
	IdleSwayTime += DeltaTime;
	const FVector SwayOffset(
		FMath::Sin(IdleSwayTime * 1.6f) * 0.22f,
		0.0f,
		FMath::Sin(IdleSwayTime * 3.2f) * 0.18f
	);

	const FVector TotalMotionOffset = CurrentRecoilOffset + ReloadOffset + EquipOffset + SwayOffset;
	const FRotator TotalMotionRot = CurrentRecoilRot + ReloadRot + EquipRot;

	FirstPersonGunMesh->SetRelativeLocation(BaseGunRelativeLocation + TotalMotionOffset);
	const FQuat FinalQuat = FQuat(BaseGunRelativeRotation) * FQuat(TotalMotionRot);
	FirstPersonGunMesh->SetRelativeRotation(FinalQuat.Rotator());

	// Keep the Cubic Farmer's right hand locked to the pistol grip & left hand gripping / reloading cleanly
	if (FarmerRightHand)
	{
		FarmerRightHand->SetRelativeLocation(FVector(-0.4f, 0.2f, -1.2f) + TotalMotionOffset);
		FarmerRightHand->SetRelativeRotation(FRotator(0.0f, 8.0f, 0.0f) + TotalMotionRot);
	}
	if (FarmerLeftHand)
	{
		FarmerLeftHand->SetRelativeLocation(LeftHandAnimOffset + TotalMotionOffset);
		FarmerLeftHand->SetRelativeRotation(LeftHandAnimRot + TotalMotionRot);
	}

	// 4. Update & decay the 3D muzzle flash flare at the barrel tip
	if (MuzzleFlashTimeRemaining > 0.0f)
	{
		MuzzleFlashTimeRemaining = FMath::Max(0.0f, MuzzleFlashTimeRemaining - DeltaTime);
		const float FlashAlpha = FMath::Clamp(MuzzleFlashTimeRemaining / 0.055f, 0.0f, 1.0f);
		const FVector LiveMuzzlePos = MuzzleTipRelativeLocation + CurrentRecoilOffset;

		if (MuzzleFlashCoreMesh)
		{
			MuzzleFlashCoreMesh->SetRelativeLocation(LiveMuzzlePos);
			MuzzleFlashCoreMesh->SetRelativeScale3D(FVector(0.045f * FlashAlpha, 0.14f * FlashAlpha, 0.045f * FlashAlpha));
		}
		if (MuzzleFlashLight)
		{
			MuzzleFlashLight->SetRelativeLocation(LiveMuzzlePos);
			MuzzleFlashLight->SetIntensity(165.0f * FlashAlpha);
		}

		if (MuzzleFlashTimeRemaining <= 0.0f)
		{
			if (MuzzleFlashCoreMesh)  { MuzzleFlashCoreMesh->SetVisibility(false, true); }
			if (MuzzleFlashCrossMesh) { MuzzleFlashCrossMesh->SetVisibility(false, false); }
			if (MuzzleFlashLight)     { MuzzleFlashLight->SetVisibility(false); }
		}
	}
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the timers
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	// ensure this weapon is destroyed when the owner is destroyed
	Destroy();
}

void AShooterWeapon::ActivateWeapon()
{
	// unhide this weapon
	SetActorHiddenInGame(false);
	EquipAnimElapsed = 0.0f;

	// Always ensure muzzle flash cones & light start hidden until a shot is actually fired
	MuzzleFlashTimeRemaining = 0.0f;
	if (MuzzleFlashCoreMesh)  { MuzzleFlashCoreMesh->SetVisibility(false, true); }
	if (MuzzleFlashCrossMesh) { MuzzleFlashCrossMesh->SetVisibility(false, false); }
	if (MuzzleFlashLight)     { MuzzleFlashLight->SetVisibility(false); }

	// Respect the Farmer's current first/third-person view when a weapon is
	// activated after a wheel swap or a return from the Farm dimension.
	const AShooterCharacter* OwnerCharacter = Cast<AShooterCharacter>(GetOwner());
	const UCameraComponent* OwnerCamera = OwnerCharacter ? OwnerCharacter->GetFirstPersonCameraComponent() : nullptr;
	const bool bThirdPerson = OwnerCamera && OwnerCamera->IsUsingAbsoluteLocation();

	if (FirstPersonGunMesh && FirstPersonGunMesh->GetStaticMesh())
	{
		FirstPersonGunMesh->SetVisibility(!bThirdPerson, true);
		if (FarmerRightHand) { FarmerRightHand->SetVisibility(!bThirdPerson, true); }
		if (FarmerLeftHand)  { FarmerLeftHand->SetVisibility(!bThirdPerson, true); }
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(false, false);
		}
	}

	if (ThirdPersonGunMesh && ThirdPersonGunMesh->GetStaticMesh())
	{
		ThirdPersonGunMesh->SetVisibility(true, true);
		ThirdPersonGunMesh->SetFirstPersonPrimitiveType(
			bThirdPerson
				? EFirstPersonPrimitiveType::None
				: EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		ThirdPersonGunMesh->SetOwnerNoSee(!bThirdPerson);
		ThirdPersonGunMesh->MarkRenderStateDirty();
		if (ThirdPersonMesh)
		{
			ThirdPersonMesh->SetVisibility(false, false);
		}
	}

	// notify the owner
	if (WeaponOwner)
	{
		WeaponOwner->OnWeaponActivated(this);
	}
}

void AShooterWeapon::DeactivateWeapon()
{
	// ensure we're no longer firing this weapon while deactivated
	StopFiring();

	// ensure we're no longer reloading this weapon while deactivated
	CancelReload();

	// hide the weapon
	SetActorHiddenInGame(true);
	if (FarmerRightHand) { FarmerRightHand->SetVisibility(false, true); }
	if (FarmerLeftHand)  { FarmerLeftHand->SetVisibility(false, true); }
	MuzzleFlashTimeRemaining = 0.0f;
	if (MuzzleFlashCoreMesh)  { MuzzleFlashCoreMesh->SetVisibility(false, true); }
	if (MuzzleFlashCrossMesh) { MuzzleFlashCrossMesh->SetVisibility(false, false); }
	if (MuzzleFlashLight)     { MuzzleFlashLight->SetVisibility(false); }

	// notify the owner
	WeaponOwner->OnWeaponDeactivated(this);
}

void AShooterWeapon::StartFiring()
{
	// weapons can't fire while hidden or deactivated
	if (IsHidden())
	{
		return;
	}

	// If already holding trigger on a full-auto weapon, don't reset the active refire loop
	if (bIsFiring && bFullAuto)
	{
		return;
	}

	// raise the firing flag
	bIsFiring = true;

	// check how much time has passed since we last shot
	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot >= RefireRate)
	{
		// fire the weapon right away
		Fire();
	}
	else if (bFullAuto)
	{
		// schedule the next shot for the remaining cooldown time (not TimeSinceLastShot!)
		const float RemainingDelay = FMath::Max(0.01f, RefireRate - TimeSinceLastShot);
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RemainingDelay, false);
	}
}

void AShooterWeapon::StopAutoFireAudio()
{
	if (AutoFireAudioComp && AutoFireAudioComp->IsPlaying())
	{
		AutoFireAudioComp->FadeOut(0.08f, 0.0f);
	}
}

void AShooterWeapon::StopFiring()
{
	// lower the firing flag
	bIsFiring = false;

	// stop continuous full-auto firing sound cleanly
	StopAutoFireAudio();

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::StartReload()
{
	// only reload when the weapon can actually accept more ammunition
	if (!CanReload())
	{
		if (CurrentBullets < MagazineSize && GetAvailableInventoryAmmo() <= 0 && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				9160, 3.0f, FColor::Orange,
				FString::Printf(TEXT("[NO AMMO] No %s in Tarkov Backpack! Harvest crops on the Farm!"), *GetRequiredAmmoDisplayName())
			);
		}
		return;
	}

	// raise the reloading flag, this blocks firing until the reload completes
	bIsReloading = true;
	ReloadAnimElapsed = 0.0f;

	// note: firing is locked out by the bIsReloading checks in Fire() rather than
	// StopFiring(), so a held full auto trigger resumes automatically in FinishReload()
	// while semi auto weapons still require a fresh trigger press

	// play the reload montage on the weapon's owner
	if (ReloadMontage)
	{
		WeaponOwner->PlayReloadMontage(ReloadMontage);
	}

	// play the reload sound
	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	}

	// schedule the magazine refill
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &AShooterWeapon::FinishReload, ReloadDuration, false);
}

void AShooterWeapon::FinishReload()
{
	bool bDevInfiniteAmmo = false;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bDevInfiniteAmmo = Sub->IsDeveloperModeEnabled();
			}
		}
	}

	if (bDevInfiniteAmmo)
	{
		CurrentBullets = MagazineSize;
	}
	else
	{
		// Pull matching ammo directly from the player's Tarkov Backpack
		const int32 RoundsNeeded = FMath::Max(0, MagazineSize - CurrentBullets);
		const int32 RoundsLoaded = ConsumeInventoryAmmo(RoundsNeeded);
		CurrentBullets = FMath::Clamp(CurrentBullets + RoundsLoaded, 0, MagazineSize);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (UTarkovInventoryComponent* Inv = OwnerActor->FindComponentByClass<UTarkovInventoryComponent>())
		{
			if (Inv->bHasEquippedWeapon)
			{
				Inv->EquippedWeaponItem.AmmoCount = CurrentBullets;
			}
		}
	}

	// lower the reloading flag, the weapon can fire again
	bIsReloading = false;

	// update the weapon HUD
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}

	// if the trigger is still held on a full auto weapon, resume firing
	if (bIsFiring && bFullAuto)
	{
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	}
}

void AShooterWeapon::CancelReload()
{
	// nothing to cancel if a reload isn't in progress
	if (!bIsReloading)
	{
		return;
	}

	// clear the reload timer
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);

	// lower the reloading flag
	bIsReloading = false;

	// stop the reload montage if it is still playing on the owner
	if (ReloadMontage)
	{
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			TArray<USkeletalMeshComponent*> OwnerMeshes;
			OwnerCharacter->GetComponents<USkeletalMeshComponent>(OwnerMeshes);

			for (USkeletalMeshComponent* OwnerMesh : OwnerMeshes)
			{
				if (UAnimInstance* AnimInstance = OwnerMesh->GetAnimInstance())
				{
					AnimInstance->Montage_Stop(0.25f, ReloadMontage);
				}
			}
		}
	}
}

bool AShooterWeapon::CanReload() const
{
	// can't reload while a reload is already in progress
	if (bIsReloading)
	{
		return false;
	}

	// nothing to gain from reloading a full magazine
	if (CurrentBullets >= MagazineSize)
	{
		return false;
	}

	// require matching ammo in the player's Tarkov Backpack (unless Developer Mode infinite ammo is active)
	bool bDevInfiniteAmmo = false;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bDevInfiniteAmmo = Sub->IsDeveloperModeEnabled();
			}
		}
	}
	if (!bDevInfiniteAmmo && GetAvailableInventoryAmmo() <= 0)
	{
		return false;
	}

	// need a valid owner to play animations, sounds and update the HUD
	return WeaponOwner != nullptr;
}

bool AShooterWeapon::IsReloading() const
{
	return bIsReloading;
}

void AShooterWeapon::Fire()
{
	// weapons can't fire while hidden or deactivated
	if (IsHidden())
	{
		return;
	}

	// ensure the player still wants to fire. They may have let go of the trigger
	if (!bIsFiring)
	{
		return;
	}

	// weapons can't fire while reloading
	if (bIsReloading)
	{
		return;
	}

	// In Developer Mode, weapons have infinite ammo and never run dry
	bool bDevInfiniteAmmo = false;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bDevInfiniteAmmo = Sub->IsDeveloperModeEnabled();
			}
		}
	}
	if (bDevInfiniteAmmo && CurrentBullets <= 0)
	{
		CurrentBullets = MagazineSize;
	}

	// handle pulling the trigger on an empty magazine
	if (CurrentBullets <= 0)
	{
		StopAutoFireAudio();

		// play the dry fire click on the empty chamber
		if (DryFireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DryFireSound, FirstPersonMesh->GetSocketLocation(MuzzleSocketName));
		}

		// update the time of this trigger pull so the refire rate still applies
		TimeOfLastShot = GetWorld()->GetTimeSeconds();

		// auto reload instead of silently refilling the magazine
		StartReload();

		// run the standard cooldown so the trigger behaves normally
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);

		return;
	}
	
	// fire a projectile at the target
	FireProjectile(WeaponOwner->GetWeaponTargetLocation());

	// update the time of our last shot
	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	// make noise so the AI perception system can hear us
	MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, ShotNoiseTag);

	// are we full auto?
	if (bFullAuto)
	{
		// schedule the next shot
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	} else {

		// for semi-auto weapons, schedule the cooldown notification
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);

	}
}

void AShooterWeapon::FireCooldownExpired()
{
	// notify the owner
	WeaponOwner->OnSemiWeaponRefire();
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
	const FVector MuzzleLoc = GetMuzzleWorldLocation();

	// get the projectile transform
	FTransform ProjectileTransform = CalculateProjectileSpawnTransform(TargetLocation);
	
	// spawn the projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = PawnOwner;

	AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);
	if (Projectile)
	{
		Projectile->SetTracerStartLocation(MuzzleLoc);
	}

	// Play a crisp, per-shot gunshot transient synced 1:1 with every single bullet leaving the barrel
	if (FireSound)
	{
		const float PitchJitter = bFullAuto ? FMath::FRandRange(0.96f, 1.04f) : FMath::FRandRange(0.98f, 1.02f);
		UGameplayStatics::PlaySoundAtLocation(
			this,
			FireSound,
			MuzzleLoc,
			1.0f,
			PitchJitter
		);
	}

	// play the firing montage
	WeaponOwner->PlayFiringMontage(FiringMontage);

	// add recoil
	WeaponOwner->AddWeaponRecoil(FiringRecoil);

	// apply procedural 3D weapon recoil impulse on the gun mesh
	CurrentRecoilOffset += FVector(0.0f, -3.2f, 0.9f);
	CurrentRecoilRot += FRotator(8.5f, FMath::FRandRange(-1.8f, 1.8f), -5.0f);

	// Trigger bright 3D muzzle flash flare & dynamic point light at the barrel tip
	MuzzleFlashTimeRemaining = 0.055f;
	const FVector LiveMuzzlePos = MuzzleTipRelativeLocation + CurrentRecoilOffset;
	const float RandomRoll = FMath::FRandRange(0.0f, 360.0f);
	if (MuzzleFlashCoreMesh)
	{
		MuzzleFlashCoreMesh->SetRelativeLocation(LiveMuzzlePos);
		MuzzleFlashCoreMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, RandomRoll));
		MuzzleFlashCoreMesh->SetRelativeScale3D(FVector(0.048f, 0.145f, 0.048f));
		MuzzleFlashCoreMesh->SetVisibility(true, true);
	}
	if (MuzzleFlashCrossMesh)
	{
		MuzzleFlashCrossMesh->SetVisibility(true, false);
	}
	if (MuzzleFlashLight)
	{
		MuzzleFlashLight->SetRelativeLocation(LiveMuzzlePos);
		MuzzleFlashLight->SetIntensity(165.0f);
		MuzzleFlashLight->SetVisibility(true);
	}

	// consume bullets (unless Developer Mode infinite ammo is enabled)
	bool bDevInfiniteAmmo = false;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bDevInfiniteAmmo = Sub->IsDeveloperModeEnabled();
			}
		}
	}

	if (bDevInfiniteAmmo)
	{
		CurrentBullets = MagazineSize;
	}
	else
	{
		--CurrentBullets;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (UTarkovInventoryComponent* Inv = OwnerActor->FindComponentByClass<UTarkovInventoryComponent>())
		{
			if (Inv->bHasEquippedWeapon)
			{
				Inv->EquippedWeaponItem.AmmoCount = CurrentBullets;
			}
		}
	}

	// update the weapon HUD
	// note: an empty magazine is no longer refilled silently here.
	// pulling the trigger while empty plays a dry fire and starts a reload instead
	WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
}

FVector AShooterWeapon::GetMuzzleWorldLocation() const
{
	const AShooterCharacter* OwnerCharacter = Cast<AShooterCharacter>(GetOwner());
	const UCameraComponent* OwnerCamera = OwnerCharacter ? OwnerCharacter->GetFirstPersonCameraComponent() : nullptr;
	const bool bThirdPerson = OwnerCamera && OwnerCamera->IsUsingAbsoluteLocation();

	// In third person, use the gun attached to the Farmer's hand rather than the
	// camera-attached first-person representation.
	if (bThirdPerson && ThirdPersonGunMesh && ThirdPersonGunMesh->GetStaticMesh())
	{
		if (bHasGunMuzzleLocalPoint)
		{
			return ThirdPersonGunMesh->GetComponentTransform().TransformPosition(GunMuzzleLocalPoint);
		}
		if (ThirdPersonMesh)
		{
			return ThirdPersonMesh->GetComponentTransform().TransformPosition(MuzzleTipRelativeLocation);
		}
	}

	if (FirstPersonGunMesh && FirstPersonGunMesh->GetStaticMesh())
	{
		if (bHasGunMuzzleLocalPoint)
		{
			// The component transform includes recoil, reload, equip and idle sway.
			return FirstPersonGunMesh->GetComponentTransform().TransformPosition(GunMuzzleLocalPoint);
		}
		if (FirstPersonMesh)
		{
			return FirstPersonMesh->GetComponentTransform().TransformPosition(MuzzleTipRelativeLocation + CurrentRecoilOffset);
		}
	}

	return FirstPersonMesh ? FirstPersonMesh->GetSocketLocation(MuzzleSocketName) : GetActorLocation();
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
	// Use the same barrel location for the projectile and its visual/sound origin.
	const FVector MuzzleLoc = GetMuzzleWorldLocation();

	// calculate the spawn location ahead of the muzzle
	const FVector SpawnLoc = MuzzleLoc + ((TargetLocation - MuzzleLoc).GetSafeNormal() * MuzzleOffset);

	// find the aim rotation vector while applying some variance to the target 
	const FRotator AimRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, TargetLocation + (UKismetMathLibrary::RandomUnitVector() * AimVariance));

	// return the built transform
	return FTransform(AimRot, SpawnLoc, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
	return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
	return ThirdPersonAnimInstanceClass;
}

void AShooterWeapon::SetBulletCount(int32 NewCount)
{
	CurrentBullets = FMath::Clamp(NewCount, 0, MagazineSize);
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}
}

FString AShooterWeapon::GetRequiredAmmoDisplayName() const
{
	if (!ExplicitAmmoType.IsEmpty())
	{
		return ExplicitAmmoType;
	}

	const FString ClassStr = GetClass()->GetName();
	if (ClassStr.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase))
	{
		return TEXT("9mm Pistol Ammo");
	}
	if (ClassStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Grenade"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Launcher"), ESearchCase::IgnoreCase))
	{
		return TEXT("12G Shotgun Shells");
	}
	if (ClassStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase))
	{
		return TEXT("5.56mm Rifle Ammo");
	}
	return TEXT("9mm Pistol Ammo");
}

namespace
{
	bool DoesItemMatchWeaponAmmo(const FInventoryGridItem& Item, const FString& RequiredAmmoName)
	{
		if (Item.WeaponClass != nullptr || Item.AmmoCount <= 0)
		{
			return false;
		}
		if (Item.ItemName.Equals(RequiredAmmoName, ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (RequiredAmmoName.Contains(TEXT("9mm")) && (Item.ItemName.Contains(TEXT("9mm"), ESearchCase::IgnoreCase) || Item.ItemName.Contains(TEXT("Pistol Ammo"), ESearchCase::IgnoreCase)))
		{
			return true;
		}
		if (RequiredAmmoName.Contains(TEXT("12G")) && (Item.ItemName.Contains(TEXT("12G"), ESearchCase::IgnoreCase) || Item.ItemName.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase)))
		{
			return true;
		}
		if (RequiredAmmoName.Contains(TEXT("5.56")) && (Item.ItemName.Contains(TEXT("5.56"), ESearchCase::IgnoreCase) || Item.ItemName.Contains(TEXT("Rifle Ammo"), ESearchCase::IgnoreCase)))
		{
			return true;
		}
		return false;
	}
}

int32 AShooterWeapon::GetAvailableInventoryAmmo() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0;
	}

	const UTarkovInventoryComponent* Inv = OwnerActor->FindComponentByClass<UTarkovInventoryComponent>();
	if (!Inv)
	{
		return 0;
	}

	const FString ReqName = GetRequiredAmmoDisplayName();
	int32 Total = 0;
	for (const FInventoryGridItem& Item : Inv->BackpackItems)
	{
		if (DoesItemMatchWeaponAmmo(Item, ReqName))
		{
			Total += Item.AmmoCount;
		}
	}
	return Total;
}

int32 AShooterWeapon::ConsumeInventoryAmmo(int32 RoundsNeeded)
{
	if (RoundsNeeded <= 0)
	{
		return 0;
	}

	AActor* OwnerActor = GetOwner();
	UTarkovInventoryComponent* Inv = OwnerActor ? OwnerActor->FindComponentByClass<UTarkovInventoryComponent>() : nullptr;
	if (!Inv)
	{
		return 0;
	}

	const FString ReqName = GetRequiredAmmoDisplayName();
	int32 RemainingToConsume = RoundsNeeded;
	int32 TotalConsumed = 0;

	for (int32 i = Inv->BackpackItems.Num() - 1; i >= 0 && RemainingToConsume > 0; --i)
	{
		FInventoryGridItem& Item = Inv->BackpackItems[i];
		if (!DoesItemMatchWeaponAmmo(Item, ReqName))
		{
			continue;
		}

		const int32 Take = FMath::Min(RemainingToConsume, Item.AmmoCount);
		Item.AmmoCount -= Take;
		RemainingToConsume -= Take;
		TotalConsumed += Take;

		if (Item.AmmoCount <= 0)
		{
			Inv->BackpackItems.RemoveAt(i);
		}
	}

	if (TotalConsumed > 0)
	{
		Inv->OnInventoryUpdated.Broadcast();
	}
	return TotalConsumed;
}

void AShooterWeapon::ConfigureCustomGunPackArchetype(
	const FString& InDisplayName,
	const FString& InSubtitle,
	const FString& InStaticMeshPath,
	const FString& InAmmoType,
	int32 InMagSize,
	bool bInFullAuto,
	float InRefireRate,
	float InReloadDuration,
	float InRecoil,
	float InAimVariance,
	const FVector& InMeshOffset,
	const FRotator& InMeshRotation,
	float InUniformScale,
	TSubclassOf<AShooterProjectile> InProjectileClass,
	TSubclassOf<UAnimInstance> InFPAnimClass,
	TSubclassOf<UAnimInstance> InTPAnimClass,
	UAnimMontage* InFireMontage,
	UAnimMontage* InReloadMontage,
	USoundBase* InFireSound,
	USoundBase* InReloadSound,
	USoundBase* InDryFireSound)
{
	WeaponDisplayName = InDisplayName;
	WeaponSubtitle = InSubtitle;
	ExplicitAmmoType = InAmmoType;
	MagazineSize = InMagSize;
	CurrentBullets = InMagSize;
	bFullAuto = bInFullAuto;
	RefireRate = InRefireRate;
	ReloadDuration = InReloadDuration;
	FiringRecoil = InRecoil;
	AimVariance = InAimVariance;
	MuzzleSocketName = FName("Muzzle");
	MuzzleOffset = 15.0f;

	if (InProjectileClass) { ProjectileClass = InProjectileClass; }
	if (InFPAnimClass) { FirstPersonAnimInstanceClass = InFPAnimClass; }
	if (InTPAnimClass) { ThirdPersonAnimInstanceClass = InTPAnimClass; }
	if (InFireMontage) { FiringMontage = InFireMontage; }
	if (InReloadMontage) { ReloadMontage = InReloadMontage; }
	if (InFireSound) { FireSound = InFireSound; }
	if (InReloadSound) { ReloadSound = InReloadSound; }
	if (InDryFireSound) { DryFireSound = InDryFireSound; }

	BaseGunRelativeLocation = InMeshOffset;
	BaseGunRelativeRotation = InMeshRotation;
	BaseGunRelativeScale = FVector(InUniformScale, InUniformScale, InUniformScale);

	// Configure per-archetype Cubic Farmer left-hand grip & reload keyframes
	// Local frame: +Y = forward along barrel, +X = left, +Z = up
	bIsPistolArchetype = !bInFullAuto || InDisplayName.Contains(TEXT("M1911"), ESearchCase::IgnoreCase);
	if (bIsPistolArchetype)
	{
		// Two-handed Weaver pistol grip: left hand cups the front/left side of the grip & trigger guard (not flat under mag!)
		LeftHandRestOffset = FVector(1.8f, 3.2f, -1.0f);
		LeftHandMagOffset  = FVector(0.5f, 0.8f, -5.8f);
		LeftHandRackOffset = FVector(2.0f, 6.5f, 5.0f);
	}
	else if (InDisplayName.Contains(TEXT("MP5"), ESearchCase::IgnoreCase) || InDisplayName.Contains(TEXT("MP7"), ESearchCase::IgnoreCase))
	{
		// Compact SMG foregrip hold: left hand grips cleanly under the forward barrel shroud
		LeftHandRestOffset = FVector(1.4f, 15.5f, 0.8f);
		LeftHandMagOffset  = FVector(1.0f, 10.5f, -5.5f);
		LeftHandRackOffset = FVector(2.5f, 12.0f, 5.5f);
	}
	else
	{
		// Assault Rifle / Battle Rifle handguard hold: left hand supports well forward under the handguard
		LeftHandRestOffset = FVector(1.4f, 20.5f, 1.0f);
		LeftHandMagOffset  = FVector(1.0f, 11.5f, -6.0f);
		LeftHandRackOffset = FVector(2.5f, 8.5f, 5.5f);
	}

	if (UStaticMesh* LoadedGunMesh = LoadObject<UStaticMesh>(nullptr, *InStaticMeshPath))
	{
		// Compute exact barrel tip offset in FirstPersonMesh space from the mesh bounding box
		const FBoxSphereBounds GunBounds = LoadedGunMesh->GetBounds();
		const float HalfLenY = GunBounds.BoxExtent.Y * InUniformScale;
		const float BarrelZ = (GunBounds.BoxExtent.Z * 0.42f) * InUniformScale;
		MuzzleTipRelativeLocation = InMeshOffset + FVector(0.0f, HalfLenY + 2.5f, BarrelZ);

		// Store the muzzle in the imported mesh's own local space. This lets the
		// active gun component's transform handle yaw-180 layouts, scale, recoil,
		// reload motion and idle sway without a fixed camera-space approximation.
		const FVector LocalBarrelForward = FQuat(InMeshRotation).RotateVector(FVector(0.0f, 1.0f, 0.0f));
		const float MuzzleY = LocalBarrelForward.Y >= 0.0f ? GunBounds.BoxExtent.Y : -GunBounds.BoxExtent.Y;
		GunMuzzleLocalPoint = GunBounds.Origin + FVector(0.0f, MuzzleY, GunBounds.BoxExtent.Z * 0.42f);
		GunMuzzleLocalPoint += LocalBarrelForward.GetSafeNormal() * (2.5f / FMath::Max(InUniformScale, 0.001f));
		bHasGunMuzzleLocalPoint = true;

		if (FirstPersonGunMesh)
		{
			FirstPersonGunMesh->SetStaticMesh(LoadedGunMesh);
			FirstPersonGunMesh->SetRelativeLocation(BaseGunRelativeLocation);
			FirstPersonGunMesh->SetRelativeRotation(BaseGunRelativeRotation);
			FirstPersonGunMesh->SetRelativeScale3D(BaseGunRelativeScale);
			FirstPersonGunMesh->SetVisibility(true, true);
		}
		if (ThirdPersonGunMesh)
		{
			ThirdPersonGunMesh->SetStaticMesh(LoadedGunMesh);
			ThirdPersonGunMesh->SetRelativeLocation(BaseGunRelativeLocation);
			ThirdPersonGunMesh->SetRelativeRotation(BaseGunRelativeRotation);
			ThirdPersonGunMesh->SetRelativeScale3D(BaseGunRelativeScale);
			ThirdPersonGunMesh->SetVisibility(true, true);
		}
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(false, false);
		}
		if (ThirdPersonMesh)
		{
			ThirdPersonMesh->SetVisibility(false, false);
		}
	}
}
