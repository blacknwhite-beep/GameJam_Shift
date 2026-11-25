// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "DimensionShiftSubsystem.h"
#include "DimensionShiftCameraShake.h"
#include "FarmingComponent.h"
#include "Farming/BarnShopActor.h"
#include "Farming/FarmBedActor.h"
#include "Farming/FarmPlotBase.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "InterWorldBridge.h"
#include "EngineUtils.h"
#include "GameJam_Shift_2026.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/PrimitiveComponent.h"
#include "Variant_Shooter/AI/ZombieAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerController.h"
#include "ShooterBulletCounterUI.h"
#include "UI/TarkovInventoryWidgets.h"
#include "UI/ShooterWeaponWheelWidget.h"
#include "Weapons/ShooterProjectile.h"

namespace
{
	/**
	 *  Plays a montage on the given mesh if its skeleton matches the montage's skeleton.
	 *  Prevents engine warnings when a montage is played on an incompatible rig
	 *  (e.g. a mannequin montage played on first person arms with a different skeleton)
	 */
	void TryPlayCharacterMontage(USkeletalMeshComponent* MeshComponent, UAnimMontage* Montage)
	{
		if (!MeshComponent || !Montage)
		{
			return;
		}

		USkeletalMesh* SkeletalMesh = MeshComponent->GetSkeletalMeshAsset();

		if (!SkeletalMesh || SkeletalMesh->GetSkeleton() != Montage->GetSkeleton())
		{
			return;
		}

		if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage);
		}
	}

	struct FFarmerActionAnimState
	{
		TStrongObjectPtr<UAnimSequence> PlantAnim;

		FVector StandingCamActorLocalOffset = FVector(0.0f, 0.0f, 64.0f);
		FVector StandingHeadActorLocalPos = FVector(0.0f, 0.0f, 68.0f);
		FQuat HeadBoneToEyeLocalQuat = FQuat::Identity;
		bool bCalibrated = false;

		bool bIsPlanting = false;
		bool bDrivingFPCamera = false;
		bool bSeedRevealed = true;
		float TotalOneShotDuration = 0.0f;

		FVector PlantStartActorLoc = FVector::ZeroVector;
		FVector PlantTargetActorLoc = FVector::ZeroVector;
		float PlantStartYaw = 0.0f;
		float PlantTargetYaw = 0.0f;
		FVector PlantTargetSoilWorld = FVector::ZeroVector;

		FVector SmoothedAnimCamLoc = FVector::ZeroVector;
		FQuat SmoothedAnimCamQuat = FQuat::Identity;
		bool bHasSmoothedCam = false;

		TWeakObjectPtr<AFarmPlotBase> PendingPlantPlot;
		bool bWasSproutVisible = false;
		bool bWasCropFallbackVisible = false;

		// Third-person view is forced for the duration of the planting one-shot so the full
		// kneel/dig/pat animation is actually visible; the player's prior view is restored after.
		bool bSavedThirdPersonForPlant = false;
		bool bForcedThirdPersonForPlant = false;
		int32 ChosenCamSideIndex = -1;
	};

	/**
	 * Guarantees BodyMesh runs a live UZombieAnimInstance (our skeleton-agnostic C++ FAnimInstanceProxy
	 * sequence evaluator proven on every character in Lvl_Combat) with head-look disabled.
	 * Eliminates UE5 SingleNode silent T-pose no-ops after runtime mesh swaps and dimension shifts.
	 */
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

	/**
	 * Switches BodyMesh between:
	 *  - Third-Person (bShowBodyToOwner = true): FirstPersonPrimitiveType::None + OwnerNoSee=false so the
	 *    full 3D Farmer is visible in the viewport AND casts a dynamic shadow.
	 *    (Fixes the invisible-body bug caused by BaseClass WorldSpaceRepresentation forcing bOwnerNoSee=true!)
	 *  - First-Person (bShowBodyToOwner = false): WorldSpaceRepresentation + OwnerNoSee=true so the body
	 *    never clips into the First-Person camera/gun while still casting a live animated VSM ground shadow.
	 */
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

	/** Forces Third-Person view for the planting one-shot and remembers the player's prior view. */
	void BeginPlantThirdPerson(AShooterCharacter* Character, FFarmerActionAnimState& State, bool& bIsThirdPersonView)
	{
		State.bSavedThirdPersonForPlant = bIsThirdPersonView;
		State.bForcedThirdPersonForPlant = !bIsThirdPersonView;
		State.ChosenCamSideIndex = -1;
		bIsThirdPersonView = true;

		ApplyFarmerViewVisibility(Character, true);
		if (State.bForcedThirdPersonForPlant)
		{
			// Hide head only for the first 50ms while camera begins pulling back out of the Farmer's eyes
			if (USkeletalMeshComponent* Body = Character->GetMesh())
			{
				Body->HideBoneByName(FName("Head"), PBO_None);
			}
		}

		if (UCameraComponent* Cam = Character->GetFirstPersonCameraComponent())
		{
			Cam->bUsePawnControlRotation = false;
			Cam->SetUsingAbsoluteLocation(true);
			Cam->SetUsingAbsoluteRotation(true);
		}
	}

	/** Restores the view that was active before the planting one-shot forced Third-Person. */
	void EndPlantThirdPerson(AShooterCharacter* Character, FFarmerActionAnimState& State, bool& bIsThirdPersonView,
		const FVector& DefaultRelLoc, const FRotator& DefaultRelRot, bool bSavedPawnRot)
	{
		State.ChosenCamSideIndex = -1;
		if (!State.bForcedThirdPersonForPlant)
		{
			ApplyFarmerViewVisibility(Character, bIsThirdPersonView);
			return;
		}
		State.bForcedThirdPersonForPlant = false;
		bIsThirdPersonView = State.bSavedThirdPersonForPlant;
		ApplyFarmerViewVisibility(Character, bIsThirdPersonView);
		if (State.bSavedThirdPersonForPlant)
		{
			return; // player was already in Third-Person; its own camera tick handles it
		}

		if (UCameraComponent* Cam = Character->GetFirstPersonCameraComponent())
		{
			Cam->SetUsingAbsoluteLocation(false);
			Cam->SetUsingAbsoluteRotation(false);
			Cam->SetRelativeLocationAndRotation(DefaultRelLoc, DefaultRelRot);
			Cam->bUsePawnControlRotation = bSavedPawnRot;
		}
	}

	TMap<TWeakObjectPtr<const AShooterCharacter>, FFarmerActionAnimState> GFarmerActionStates;

	FFarmerActionAnimState& GetFarmerActionState(const AShooterCharacter* Character)
	{
		return GFarmerActionStates.FindOrAdd(Character);
	}

	void RevealPendingPlantSprout(FFarmerActionAnimState& State)
	{
		if (!State.bSeedRevealed)
		{
			State.bSeedRevealed = true;
			if (AFarmPlotBase* Plot = State.PendingPlantPlot.Get())
			{
				if (Plot->GetCurrentStage() != ECropStage::Empty)
				{
					if (State.bWasSproutVisible && Plot->Mesh_Sprout)
					{
						Plot->Mesh_Sprout->SetVisibility(true);
					}
					if (State.bWasCropFallbackVisible && Plot->CropMesh)
					{
						Plot->CropMesh->SetVisibility(true);
					}
				}
			}
		}
		State.PendingPlantPlot.Reset();
	}

	void RestoreFarmerActionCamera(AShooterCharacter* Character, FFarmerActionAnimState& State, const FVector& DefaultRelLoc, const FRotator& DefaultRelRot, bool bSavedPawnRot, bool bThirdPerson, bool bSleeping)
	{
		RevealPendingPlantSprout(State);
		State.bIsPlanting = false;
		State.bHasSmoothedCam = false;

		if (!State.bDrivingFPCamera)
		{
			return;
		}
		State.bDrivingFPCamera = false;

		if (!Character || bThirdPerson || bSleeping)
		{
			return;
		}

		if (UCameraComponent* Camera = Character->GetFirstPersonCameraComponent())
		{
			Camera->SetUsingAbsoluteLocation(false);
			Camera->SetUsingAbsoluteRotation(false);
			Camera->SetRelativeLocationAndRotation(DefaultRelLoc, DefaultRelRot);
			Camera->bUsePawnControlRotation = bSavedPawnRot;
		}
	}
}

AShooterCharacter::AShooterCharacter()
{
	// create the noise emitter component
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// create the Tarkov inventory component
	TarkovInventory = CreateDefaultSubobject<UTarkovInventoryComponent>(TEXT("TarkovInventory"));

	// create the Farming component
	FarmingComponent = CreateDefaultSubobject<UFarmingComponent>(TEXT("FarmingComponent"));

	// create the tactical flashlight attached to the first-person camera (off by default, Combat world only)
	TacticalFlashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("TacticalFlashlight"));
	if (GetFirstPersonCameraComponent())
	{
		TacticalFlashlight->SetupAttachment(GetFirstPersonCameraComponent());
	}
	else
	{
		TacticalFlashlight->SetupAttachment(RootComponent);
	}
	TacticalFlashlight->SetRelativeLocation(FVector(25.0f, 0.0f, -4.0f));
	TacticalFlashlight->SetRelativeRotation(FRotator::ZeroRotator);
	TacticalFlashlight->bUseInverseSquaredFalloff = false;
	TacticalFlashlight->SetLightFalloffExponent(1.6f);
	TacticalFlashlight->SetIntensityUnits(ELightUnits::Unitless);
	TacticalFlashlight->SetIntensity(24.0f);
	TacticalFlashlight->SetAttenuationRadius(6500.0f);
	TacticalFlashlight->SetInnerConeAngle(18.0f);
	TacticalFlashlight->SetOuterConeAngle(38.0f);
	TacticalFlashlight->SetVolumetricScatteringIntensity(2.5f);
	TacticalFlashlight->SetLightColor(FColor(238, 246, 255));
	TacticalFlashlight->SetVisibility(false);

	// default weapon input mapping context
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> WeaponContextFinder(TEXT("/Game/Variant_Shooter/Input/IMC_Weapons.IMC_Weapons"));
	if (WeaponContextFinder.Succeeded())
	{
		WeaponMappingContext = WeaponContextFinder.Object;
	}

	// configure movement
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// reset HP to max
	CurrentHP = MaxHP;

	// Enforce long-range tactical flashlight settings at runtime (overrides any stale Blueprint CDO values)
	if (TacticalFlashlight)
	{
		TacticalFlashlight->SetRelativeLocation(FVector(25.0f, 0.0f, -4.0f));
		TacticalFlashlight->SetRelativeRotation(FRotator::ZeroRotator);
		TacticalFlashlight->bUseInverseSquaredFalloff = false;
		TacticalFlashlight->SetLightFalloffExponent(1.6f);
		TacticalFlashlight->SetIntensityUnits(ELightUnits::Unitless);
		TacticalFlashlight->SetIntensity(24.0f);
		TacticalFlashlight->SetAttenuationRadius(6500.0f);
		TacticalFlashlight->SetInnerConeAngle(18.0f);
		TacticalFlashlight->SetOuterConeAngle(38.0f);
		TacticalFlashlight->SetVolumetricScatteringIntensity(2.5f);
		TacticalFlashlight->SetLightColor(FColor(238, 246, 255));
	}

	// update the HUD
	OnDamaged.Broadcast(1.0f);

	// Apply the Animated Cubic Low-Poly Farmer FIRST so SpawnAllGunPackWeapons sees bUsingFarmerMesh == true
	InitializeFarmerCharacterMesh();

	// Spawn all 7 Gun Pack weapons into the 7-slot radial weapon wheel (no purchasing restrictions)
	SpawnAllGunPackWeapons();

	// Register with DimensionShiftSubsystem
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Subsystem->OnDimensionShifted.AddDynamic(this, &AShooterCharacter::HandleOnDimensionShifted);

			// Start streaming Lvl_Farm (Z = -20,000) early so it is ready before the first Shift
			if (IsPlayerControlled())
			{
				Subsystem->EnsureFarmLevelLoaded(GetWorld());
				if (Subsystem->bHasDismissedStartMenu && GetActorLocation().Z > -10000.0f)
				{
					Subsystem->TeleportToFarmStart(this);
				}
			}
		}
	}

	// Cache Dimension Shift Phase 1 game-feel assets (with safe C++ fallbacks)
	if (!ShiftCameraShakeClass)
	{
		if (UClass* LoadedShake = StaticLoadClass(UCameraShakeBase::StaticClass(), nullptr, TEXT("/Game/Dev_Blueprint/VFX/CS_DimensionShift.CS_DimensionShift_C")))
		{
			ShiftCameraShakeClass = LoadedShake;
		}
		else
		{
			ShiftCameraShakeClass = UDimensionShiftCameraShake::StaticClass();
		}
	}

	if (!ShiftSoundCue)
	{
		ShiftSoundCue = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/Cue_DimensionShift.Cue_DimensionShift"));
		if (!ShiftSoundCue)
		{
			ShiftSoundCue = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_DimensionShift.SFX_DimensionShift"));
		}
	}

	if (!ShiftPortalVFX)
	{
		ShiftPortalVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Dev_Blueprint/VFX/NS_DimensionPortal.NS_DimensionPortal"));
	}

	if (!ShiftPostProcessMaterial)
	{
		ShiftPostProcessMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/VFX/M_PP_DimensionShift.M_PP_DimensionShift"));
	}

	if (ShiftPostProcessMaterial && GetFirstPersonCameraComponent())
	{
		ShiftPostProcessMID = UMaterialInstanceDynamic::Create(ShiftPostProcessMaterial, this);
		if (ShiftPostProcessMID)
		{
			ShiftPostProcessMID->SetScalarParameterValue(TEXT("GlitchIntensity"), 0.0f);
			ShiftPostProcessBlendableIndex = GetFirstPersonCameraComponent()->PostProcessSettings.WeightedBlendables.Array.Add(
				FWeightedBlendable(0.0f, ShiftPostProcessMID));
		}
	}
}

void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Advance the 24-hour day/night clock and update ONLY the Farm sun
	if (IsPlayerControlled())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Sub->TickDayNightClock(GetWorld(), DeltaTime);
			}
		}
	}

	// Update Animated Cubic Farmer locomotion & camera
	UpdateFarmerAnimation(DeltaTime);

	if (ShiftGlitchRemaining > 0.0f)
	{
		ShiftGlitchRemaining = FMath::Max(0.0f, ShiftGlitchRemaining - DeltaTime);
		const float Alpha = (ShiftGlitchDuration > KINDA_SMALL_NUMBER)
			? FMath::Clamp(ShiftGlitchRemaining / ShiftGlitchDuration, 0.0f, 1.0f)
			: 0.0f;

		if (ShiftPostProcessMID)
		{
			ShiftPostProcessMID->SetScalarParameterValue(TEXT("GlitchIntensity"), Alpha);
		}
		if (GetFirstPersonCameraComponent() && GetFirstPersonCameraComponent()->PostProcessSettings.WeightedBlendables.Array.IsValidIndex(ShiftPostProcessBlendableIndex))
		{
			GetFirstPersonCameraComponent()->PostProcessSettings.WeightedBlendables.Array[ShiftPostProcessBlendableIndex].Weight = Alpha;
		}
	}
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// Unregister from DimensionShiftSubsystem
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Subsystem->OnDimensionShifted.RemoveDynamic(this, &AShooterCharacter::HandleOnDimensionShifted);
		}
	}

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);

	if (FFarmerActionAnimState* State = GFarmerActionStates.Find(this))
	{
		RevealPendingPlantSprout(*State);
		GFarmerActionStates.Remove(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// base class handles move, aim and jump inputs
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Firing
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);

		// Switch weapon: disabled on EnhancedInput SwitchWeaponAction because IMC_Weapons binds Left Shift
		// to IA_SwapWeapon! Weapon switching is handled cleanly via [Hold TAB] Radial Wheel and keys [1]-[7].

		// Reload
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &AShooterCharacter::DoReload);

		// Toggle Inventory (only wire Enhanced Input IA_ToggleInventory if it is not intercepting Tab in Combat)
		// We handle I (Inventory) and Tab (7-Weapon Radial Wheel in Combat / Inventory in Farm) directly via BindKey.

		// Dimension Shift (Shift key)
		if (DimensionShiftAction)
		{
			EnhancedInputComponent->BindAction(DimensionShiftAction, ETriggerEvent::Triggered, this, &AShooterCharacter::HandleDimensionShift);
		}
	}

	// Direct key fallback: guarantees Left Shift ALWAYS triggers even if the Enhanced Input asset is unassigned!
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AShooterCharacter::HandleDimensionShift);

	// Direct key fallback for Left Mouse Button so clicking triggers DoStartFiring (firing in combat, farm tool in farm)
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AShooterCharacter::DoStartFiring);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AShooterCharacter::DoStopFiring);

	// Farming tool (1..4) & Combat Weapon Wheel (1..7) direct keybindings
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AShooterCharacter::SelectTool1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AShooterCharacter::SelectTool2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AShooterCharacter::SelectTool3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AShooterCharacter::SelectTool4);
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AShooterCharacter::SelectWeaponSlot5);
	PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AShooterCharacter::SelectWeaponSlot6);
	PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AShooterCharacter::SelectWeaponSlot7);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AShooterCharacter::CycleToolPrevious);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AShooterCharacter::InteractOrCycleToolNext);
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AShooterCharacter::CycleToolPrevious);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AShooterCharacter::CycleToolNext);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &AShooterCharacter::CycleSeedNext);

	// Quick use medkit action (H key)
	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AShooterCharacter::UseMedkit);

	// Inventory toggle (I key in both dimensions; Holding TAB in Combat opens the 7-Weapon Radial Wheel!)
	PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &AShooterCharacter::DoToggleInventory);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AShooterCharacter::OnTabPressed);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Released, this, &AShooterCharacter::OnTabReleased);

	// Tactical flashlight toggle (F key — Combat / Zombie dimension only)
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AShooterCharacter::ToggleFlashlight);

	// Barn Shop interaction (B key) & Plot placement cancel (Right Mouse Button)
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AShooterCharacter::InteractBarnShop);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AShooterCharacter::CancelPlotPlacementInput);

	// Developer Mode time-advance hotkey (+ / = and Numpad +)
	PlayerInputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &AShooterCharacter::DevAdvanceTimeInput);
	PlayerInputComponent->BindKey(EKeys::Add, IE_Pressed, this, &AShooterCharacter::DevAdvanceTimeInput);

	// Toggle First-Person / Third-Person view (V key) to admire the Animated Farmer character
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &AShooterCharacter::ToggleThirdPersonView);

}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Ignore damage while the Start Menu is active or if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			if (Sub->bIsStartMenuActive)
			{
				return 0.0f;
			}
		}
	}

	// Reduce HP
	CurrentHP -= Damage;

	// Play clean rotational camera shake on hit
	if (Damage > 0.0f)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ClientStartCameraShake(UHitCameraShake::StaticClass(), 1.0f);
		}
		if (bUsingFarmerMesh && CurrentHP > 0.0f && FarmerAnim_HitReact)
		{
			PlayFarmerOneShotAnim(FarmerAnim_HitReact, 1.25f);
		}
	}

	// Have we depleted HP?
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	// update the HUD
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));

	return Damage;
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
	// the view stays locked while lying in bed or kneeling in the full planting animation
	if (bIsSleepingInBed || GetFarmerActionState(this).bIsPlanting)
	{
		return;
	}

	// only route inputs if the character is not dead
	if (!IsDead())
	{
		float Sens = 1.0f;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Sens = Sub->MouseSensitivity;
			}
		}
		Super::DoAim(Yaw * Sens, Pitch * Sens);
	}
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
	// no walking while lying in bed or kneeling to plant a crop
	if (bIsSleepingInBed || GetFarmerActionState(this).bIsPlanting)
	{
		return;
	}

	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoMove(Right, Forward);
	}
}

void AShooterCharacter::DoJumpStart()
{
	// no jumping while lying in bed or kneeling to plant a crop
	if (bIsSleepingInBed || GetFarmerActionState(this).bIsPlanting)
	{
		return;
	}

	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpStart();
	}
}

void AShooterCharacter::DoJumpEnd()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpEnd();
	}
}

void AShooterCharacter::DoStartFiring()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			if (Sub->bIsStartMenuActive)
			{
				return;
			}
		}
	}

	// In Farming dimension, route Left Click to plot placement, Barn Shop, or FarmingComponent!
	if (CurrentDimension == EGameDimension::Farming)
	{
		if (FarmingComponent && FarmingComponent->IsPlacingPlot())
		{
			FarmingComponent->ConfirmPlotPlacement();
			return;
		}

		// Check if aiming at the Barn Shop counter
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BarnShopTrace), false);
			Params.AddIgnoredActor(this);
			if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, ViewLoc + ViewRot.Vector() * 480.0f, ECC_Visibility, Params))
			{
				if (ABarnShopActor* Shop = Cast<ABarnShopActor>(Hit.GetActor()))
				{
					Shop->OpenBarnShop(PC);
					return;
				}
			}
		}

		if (FarmingComponent)
		{
			FFarmerActionAnimState& ActionState = GetFarmerActionState(this);
			if (ActionState.bIsPlanting)
			{
				return;
			}

			AFarmPlotBase* TargetPlot = FarmingComponent->GetTargetedPlot();
			const EFarmTool Tool = FarmingComponent->ActiveTool;
			const bool bWillPlant = (Tool == EFarmTool::None)
				&& TargetPlot
				&& TargetPlot->CanPlant()
				&& !FarmingComponent->SelectedSeedID.IsNone()
				&& FarmingComponent->GetSeedCount(FarmingComponent->SelectedSeedID) > 0;
			const bool bWillHarvest = (Tool == EFarmTool::Harvester || Tool == EFarmTool::None)
				&& TargetPlot
				&& TargetPlot->CanHarvest();

			FarmingComponent->UseActiveTool();

			if (bUsingFarmerMesh)
			{
				if (bWillPlant && TargetPlot && TargetPlot->GetCurrentStage() == ECropStage::Planted && ActionState.PlantAnim.IsValid())
				{
					if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
					{
						MoveComp->StopMovementImmediately();
					}

					const FVector ActorLoc = GetActorLocation();
					const FVector PlotLoc = TargetPlot->GetActorLocation();
					FVector ToPlot2D(PlotLoc.X - ActorLoc.X, PlotLoc.Y - ActorLoc.Y, 0.0f);
					if (ToPlot2D.SizeSquared2D() < 1.0f)
					{
						ToPlot2D = GetActorForwardVector();
						ToPlot2D.Z = 0.0f;
					}
					const FVector ToPlotDir = ToPlot2D.GetSafeNormal();

					// Position Farmer 48 cm in front of the plot center so both retargeted hands (+45..+52 cm forward) cup the soil dead-center!
					FVector DesiredKneelLoc = FVector(PlotLoc.X, PlotLoc.Y, ActorLoc.Z) - ToPlotDir * 48.0f;
					const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.0f;
					FHitResult FloorHit;
					FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(PlantKneelFloorTrace), false);
					FloorParams.AddIgnoredActor(this);
					FloorParams.AddIgnoredActor(TargetPlot);
					if (GetWorld()->LineTraceSingleByChannel(
						FloorHit,
						DesiredKneelLoc + FVector(0.0f, 0.0f, 80.0f),
						DesiredKneelLoc - FVector(0.0f, 0.0f, 240.0f),
						ECC_WorldStatic,
						FloorParams))
					{
						DesiredKneelLoc.Z = FloorHit.ImpactPoint.Z + HalfHeight;
					}

					ActionState.bIsPlanting = true;
					ActionState.bHasSmoothedCam = false;
					ActionState.PlantStartActorLoc = ActorLoc;
					ActionState.PlantTargetActorLoc = DesiredKneelLoc;
					ActionState.PlantStartYaw = GetActorRotation().Yaw;
					ActionState.PlantTargetYaw = ToPlotDir.Rotation().Yaw;
					ActionState.PlantTargetSoilWorld = PlotLoc + FVector(0.0f, 0.0f, 10.0f);

					// Hold the seedling mesh hidden while digging the hole, then pop it visible right between both hands at 46% progress!
					ActionState.PendingPlantPlot = TargetPlot;
					ActionState.bWasSproutVisible = TargetPlot->Mesh_Sprout && TargetPlot->Mesh_Sprout->IsVisible();
					ActionState.bWasCropFallbackVisible = TargetPlot->CropMesh && TargetPlot->CropMesh->IsVisible();
					if (ActionState.bWasSproutVisible || ActionState.bWasCropFallbackVisible)
					{
						ActionState.bSeedRevealed = false;
						if (TargetPlot->Mesh_Sprout) { TargetPlot->Mesh_Sprout->SetVisibility(false); }
						if (TargetPlot->CropMesh)    { TargetPlot->CropMesh->SetVisibility(false); }
					}
					else
					{
						ActionState.bSeedRevealed = true;
					}

					// Show the full-body kneel/dig/pat animation in Third-Person (the player can
					// see the Farmer plant the seed); restore their previous view when it finishes.
					BeginPlantThirdPerson(this, ActionState, bIsThirdPersonView);
					PlayFarmerOneShotAnim(ActionState.PlantAnim.Get(), 1.65f);
					ActionState.TotalOneShotDuration = FarmerOneShotRemaining;
				}
				else if (bWillHarvest && FarmerAnim_GrabLow)
				{
					ActionState.bIsPlanting = false;
					ActionState.bHasSmoothedCam = false;
					PlayFarmerOneShotAnim(FarmerAnim_GrabLow, 1.35f);
					ActionState.TotalOneShotDuration = FarmerOneShotRemaining;
				}
				else if ((Tool == EFarmTool::Hoe || Tool == EFarmTool::WateringCan) && FarmerAnim_WorkHoe)
				{
					ActionState.bIsPlanting = false;
					ActionState.bHasSmoothedCam = false;
					PlayFarmerOneShotAnim(FarmerAnim_WorkHoe, 1.35f);
					ActionState.TotalOneShotDuration = FarmerOneShotRemaining;
				}
			}
		}
		return;
	}

	// fire the current weapon (ignore if the radial weapon wheel is currently open)
	if (bIsWeaponWheelOpen)
	{
		return;
	}

	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StartFiring();
	}
}

void AShooterCharacter::DoStopFiring()
{
	if (CurrentDimension == EGameDimension::Farming)
	{
		return;
	}

	// stop firing the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StopFiring();
	}
}

void AShooterCharacter::DoSwitchWeapon()
{
	if (CurrentDimension == EGameDimension::Farming)
	{
		if (FarmingComponent)
		{
			FarmingComponent->CycleTool(true);
		}
		return;
	}

	if (OwnedWeapons.Num() <= 1 || IsDead())
	{
		return;
	}

	const int32 CurrentIndex = OwnedWeapons.Find(CurrentWeapon);
	const int32 StartIdx = (CurrentIndex == INDEX_NONE) ? 0 : CurrentIndex;

	for (int32 Step = 1; Step < OwnedWeapons.Num(); ++Step)
	{
		const int32 CandidateIdx = (StartIdx + Step) % OwnedWeapons.Num();
		if (IsWeaponSlotUnlocked(CandidateIdx))
		{
			EquipWeaponByIndex(CandidateIdx);
			return;
		}
	}
}

void AShooterCharacter::DoReload()
{
	if (CurrentDimension == EGameDimension::Farming)
	{
		return;
	}

	// Do not reload weapon while inventory overlay is open
	if (TarkovInventory && TarkovInventory->IsInventoryOpen())
	{
		return;
	}

	// reload the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StartReload();
	}
}

void AShooterCharacter::DoToggleInventory()
{
	if (TarkovInventory && !IsDead())
	{
		TarkovInventory->ToggleInventory();
	}
}

void AShooterCharacter::DoToggleInventoryFromFarming()
{
	if (CurrentDimension == EGameDimension::Farming)
	{
		DoToggleInventory();
	}
}

void AShooterCharacter::ToggleFlashlight()
{
	if (IsDead() || !TacticalFlashlight)
	{
		return;
	}

	// Only allow flashlight activation in the Combat / Zombie dimension
	if (CurrentDimension != EGameDimension::Combat)
	{
		if (bIsFlashlightOn)
		{
			bIsFlashlightOn = false;
			TacticalFlashlight->SetVisibility(false);
		}
		return;
	}

	bIsFlashlightOn = !bIsFlashlightOn;
	TacticalFlashlight->SetVisibility(bIsFlashlightOn);
}

void AShooterCharacter::InteractOrCycleToolNext()
{
	UDimensionShiftSubsystem* Sub = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Sub = GI->GetSubsystem<UDimensionShiftSubsystem>();
	}

	const bool bInFarm = (Sub && Sub->GetCurrentDimension() == EGameDimension::Farming) || GetActorLocation().Z < -10000.0f;
	if (!Sub || !bInFarm || !GetWorld())
	{
		CycleToolNext();
		return;
	}

	if (bIsSleepingInBed)
	{
		ExitBedSleep(true);
		return;
	}

	UCameraComponent* Camera = GetFirstPersonCameraComponent();
	if (!Camera)
	{
		CycleToolNext();
		return;
	}

	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * 350.0f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FarmBedInteractTrace), false);
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		if (AFarmBedActor* Bed = Cast<AFarmBedActor>(Hit.GetActor()))
		{
			if (!Sub->IsNightWindow())
			{
				Sub->ShowCenterMessage(TEXT("It's work time! Sleeping is from 12 AM."), 2.0f);
				return;
			}

			EnterBedSleep(Bed);
			return;
		}
	}

	CycleToolNext();
}

void AShooterCharacter::EnterBedSleep(AFarmBedActor* Bed, bool bIsWakingMorning)
{
	UCameraComponent* Camera = GetFirstPersonCameraComponent();
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Bed || !Camera || !MoveComp || !Capsule || !PC || bIsSleepingInBed)
	{
		return;
	}

	if (bIsThirdPersonView)
	{
		ToggleThirdPersonView();
	}

	PreSleepLocation = GetActorLocation();
	DefaultCameraRelLocation = Camera->GetRelativeLocation();
	DefaultCameraRelRotation = Camera->GetRelativeRotation();
	bSavedUsePawnControlRotation = Camera->bUsePawnControlRotation;
	ActiveBed = Bed;
	bIsSleepingInBed = true;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Sub->SetPlayerSleepingInBed(true);
			Sub->SetLastSleptBed(Bed);
		}
	}

	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const FRotator BedFacing = Bed->GetSleepFacingRotation();
	const FVector SleepEyeLocation = Bed->GetSleepAnchorLocation();
	SetActorLocationAndRotation(SleepEyeLocation, BedFacing, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(BedFacing);

	// Minecraft-style: lie on the bed with a 90-degree roll in world space (bypasses head-bone socket axis)
	Camera->bUsePawnControlRotation = false;
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->SetWorldLocationAndRotation(SleepEyeLocation, FRotator(12.0f, BedFacing.Yaw, 90.0f));

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			if (bIsWakingMorning)
			{
				Sub->ShowCenterMessage(TEXT("Good morning! Press [E] to get out of bed."), 3.0f, FLinearColor(0.45f, 1.0f, 0.60f, 1.0f));
			}
			else
			{
				Sub->ShowCenterMessage(TEXT("Sleeping... Press [LEFT SHIFT] to enter Zombie World  |  [E] to get up"), 3.0f);
			}
		}
	}
}

void AShooterCharacter::ExitBedSleep(bool bRestorePosition)
{
	if (!bIsSleepingInBed)
	{
		return;
	}

	bIsSleepingInBed = false;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Sub->SetPlayerSleepingInBed(false);
		}
	}

	if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		Camera->SetUsingAbsoluteLocation(false);
		Camera->SetUsingAbsoluteRotation(false);
		Camera->SetRelativeLocationAndRotation(DefaultCameraRelLocation, DefaultCameraRelRotation);
		Camera->bUsePawnControlRotation = bSavedUsePawnControlRotation;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	const AFarmBedActor* Bed = ActiveBed.Get();
	ActiveBed.Reset();

	if (bRestorePosition)
	{
		const FVector WakeLocation = Bed ? Bed->GetWakeUpLocation() : PreSleepLocation;
		SetActorLocation(WakeLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	const FRotator UprightRotation(0.0f, GetActorRotation().Yaw, 0.0f);
	SetActorRotation(UprightRotation);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(UprightRotation);
	}
}

void AShooterCharacter::InteractBarnShop()
{
	if (IsDead() || CurrentDimension != EGameDimension::Farming)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !GetWorld())
	{
		return;
	}

	for (TActorIterator<ABarnShopActor> It(GetWorld()); It; ++It)
	{
		ABarnShopActor* Shop = *It;
		if (Shop && (Shop->IsPlayerInRange() || FVector::Dist(GetActorLocation(), Shop->GetActorLocation()) <= 500.0f))
		{
			Shop->OpenBarnShop(PC);
			return;
		}
	}
}

void AShooterCharacter::CancelPlotPlacementInput()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent && FarmingComponent->IsPlacingPlot())
	{
		FarmingComponent->CancelPlotPlacement();
	}
}

void AShooterCharacter::DevAdvanceTimeInput()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Sub->AdvanceTimeByRealMinute(GetWorld());
		}
	}
}

void AShooterCharacter::HandleDimensionShift()
{
	if (IsDead())
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT(">>> [INPUT] Left Shift Pressed! Requesting Dimension Shift..."));
	}

	// Close weapon wheel or inventory overlay if open before shifting
	if (bIsWeaponWheelOpen)
	{
		OnTabReleased();
	}
	if (TarkovInventory && TarkovInventory->IsInventoryOpen())
	{
		TarkovInventory->ToggleInventory();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Subsystem->RequestShift(this);
		}
	}
}

void AShooterCharacter::HandleOnDimensionShifted(EGameDimension NewDimension, FVector TargetLocation)
{
	CurrentDimension = NewDimension;

	// Waking from bed or cancelling active planting camera on shift restores camera and movement immediately
	if (bIsSleepingInBed)
	{
		ExitBedSleep(false);
	}
	{
		FFarmerActionAnimState& ActionState = GetFarmerActionState(this);
		if (ActionState.bIsPlanting || ActionState.bDrivingFPCamera)
		{
			FarmerOneShotRemaining = 0.0f;
			CurrentFarmerAnim = nullptr;
			EndPlantThirdPerson(this, ActionState, bIsThirdPersonView, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation);
			RestoreFarmerActionCamera(this, ActionState, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation, bIsThirdPersonView, bIsSleepingInBed);
		}
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Phase 1 Game Feel: 1) Camera Shake, 2) Audio Stinger, 3) Portal Sparkle VFX, 4) Post-Process Glitch Fade
	if (ShiftCameraShakeClass)
	{
		PC->ClientStartCameraShake(ShiftCameraShakeClass, 1.0f);
	}

	if (ShiftSoundCue)
	{
		UGameplayStatics::PlaySound2D(this, ShiftSoundCue);
	}

	if (ShiftPortalVFX)
	{
		const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
		const FVector FeetLocation = TargetLocation - FVector(0.0f, 0.0f, HalfHeight);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ShiftPortalVFX, FeetLocation, FRotator::ZeroRotator, FVector::OneVector, true, true);
	}

	if (ShiftPostProcessMID)
	{
		ShiftGlitchRemaining = ShiftGlitchDuration;
		ShiftPostProcessMID->SetScalarParameterValue(TEXT("GlitchIntensity"), 1.0f);
		if (GetFirstPersonCameraComponent() && GetFirstPersonCameraComponent()->PostProcessSettings.WeightedBlendables.Array.IsValidIndex(ShiftPostProcessBlendableIndex))
		{
			GetFirstPersonCameraComponent()->PostProcessSettings.WeightedBlendables.Array[ShiftPostProcessBlendableIndex].Weight = 1.0f;
		}
	}

	BP_OnDimensionShifted(NewDimension, TargetLocation);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	// Make sure WeaponMappingContext is cached
	if (!WeaponMappingContext)
	{
		WeaponMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Variant_Shooter/Input/IMC_Weapons.IMC_Weapons"));
	}

	if (NewDimension == EGameDimension::Farming)
	{
		// Automatically turn off tactical flashlight when leaving the dark Zombie world
		if (bIsFlashlightOn && TacticalFlashlight)
		{
			bIsFlashlightOn = false;
			TacticalFlashlight->SetVisibility(false);
		}

		// Entering Farming dimension: remove weapon mapping context so clicks don't trigger fire actions
		if (WeaponMappingContext)
		{
			Subsystem->RemoveMappingContext(WeaponMappingContext);
		}

		// Apply farming input mapping context
		if (FarmingMappingContext)
		{
			Subsystem->AddMappingContext(FarmingMappingContext, FarmingMappingPriority);
		}

		// Holster weapon while in farming dimension (keep weapon actor valid so animation blueprints don't error)
		if (CurrentWeapon)
		{
			CurrentWeapon->DeactivateWeapon();
			if (CurrentWeapon->GetFirstPersonMesh())
			{
				CurrentWeapon->GetFirstPersonMesh()->SetVisibility(false, true);
			}
			if (CurrentWeapon->GetThirdPersonMesh())
			{
				CurrentWeapon->GetThirdPersonMesh()->SetVisibility(false, true);
			}
			if (CurrentWeapon->GetFirstPersonGunMesh())
			{
				CurrentWeapon->GetFirstPersonGunMesh()->SetVisibility(false, true);
			}
			if (CurrentWeapon->GetThirdPersonGunMesh())
			{
				CurrentWeapon->GetThirdPersonGunMesh()->SetVisibility(false, true);
			}
		}

		// Clear character weapon anim instances so ABP_FP_Pistol does not tick on inactive weapon
		// (Do NOT propagate hidden flags to FirstPersonCameraComponent children, and never clear GetMesh()'s UZombieAnimInstance!)
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
			GetFirstPersonMesh()->SetVisibility(false, false);
			GetFirstPersonMesh()->SetHiddenInGame(true, false);
			GetFirstPersonMesh()->SetCastShadow(false);
			GetFirstPersonMesh()->bCastHiddenShadow = false;
			GetFirstPersonMesh()->SetAnimInstanceClass(nullptr);
		}
		if (GetMesh() && !bUsingFarmerMesh)
		{
			GetMesh()->SetAnimInstanceClass(nullptr);
		}
		else if (bUsingFarmerMesh)
		{
			EnsureFarmerAnimInstance(GetMesh());
			ApplyFarmerViewVisibility(this, bIsThirdPersonView);
		}

		// Show Farming HUD (WBP_FarmingHUD) and hide Combat HUD / weapon slot
		if (FarmingComponent)
		{
			FarmingComponent->SetFarmingHUDVisible(true);
		}
		if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(PC))
		{
			if (ShooterPC->GetBulletCounterUI())
			{
				ShooterPC->GetBulletCounterUI()->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		if (TarkovInventory && TarkovInventory->HUDRoot)
		{
			TarkovInventory->HUDRoot->SetVisibility(ESlateVisibility::Hidden);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				9122, 5.0f, FColor::Emerald,
				TEXT(">>> [FARM WORLD] Left Click: Use Tool | 1-4 / Scroll / Q-E: Tool | T: Seed | Shift: Combat")
			);
		}

		UE_LOG(LogGameJam_Shift_2026, Log, TEXT("ShooterCharacter: Swapped to Farming input context and HUD."));
	}
	else
	{
		// Returning to Combat dimension: hide Farming HUD and restore Combat HUD / weapon slot
		if (FarmingComponent)
		{
			FarmingComponent->SetFarmingHUDVisible(false);
		}
		if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(PC))
		{
			if (ShooterPC->GetBulletCounterUI())
			{
				ShooterPC->GetBulletCounterUI()->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
		if (TarkovInventory && TarkovInventory->HUDRoot)
		{
			TarkovInventory->HUDRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		// Returning to Combat dimension: remove farming input context
		if (FarmingMappingContext)
		{
			Subsystem->RemoveMappingContext(FarmingMappingContext);
		}

		// Restore weapon mapping context
		if (WeaponMappingContext)
		{
			Subsystem->AddMappingContext(WeaponMappingContext, 0);
		}

		// Keep old gray Mannequin FP arms hidden when using the Animated Cubic Farmer
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->SetVisibility(!bUsingFarmerMesh, false);
		}

		// Unholster and reactivate weapon when returning to combat
		if (CurrentWeapon)
		{
			CurrentWeapon->ActivateWeapon();

			// Restore character mesh AnimInstances (skip when using the Animated Cubic Farmer)
			if (GetFirstPersonMesh() && !bUsingFarmerMesh)
			{
				GetFirstPersonMesh()->SetAnimInstanceClass(CurrentWeapon->GetFirstPersonAnimInstanceClass());
			}
			if (GetMesh() && !bUsingFarmerMesh)
			{
				GetMesh()->SetAnimInstanceClass(CurrentWeapon->GetThirdPersonAnimInstanceClass());
			}
		}
		else if (InitialWeaponClass)
		{
			EquipWeaponClass(InitialWeaponClass);
		}

		UE_LOG(LogGameJam_Shift_2026, Log, TEXT("ShooterCharacter: Swapped to Combat input context."));
	}
}

void AShooterCharacter::UnequipCurrentWeapon()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->DeactivateWeapon();
		CurrentWeapon = nullptr;
	}
}

void AShooterCharacter::EquipWeaponClass(TSubclassOf<AShooterWeapon> InWeaponClass)
{
	if (!InWeaponClass)
	{
		return;
	}

	// Check if we already own a weapon of this class
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(InWeaponClass);
	if (OwnedWeapon)
	{
		// Deactivate current weapon if different
		if (CurrentWeapon && CurrentWeapon != OwnedWeapon)
		{
			CurrentWeapon->DeactivateWeapon();
		}

		// Switch to and activate the owned weapon
		CurrentWeapon = OwnedWeapon;
		CurrentWeapon->ActivateWeapon();
	}
	else
	{
		AddWeaponClass(InWeaponClass);
	}
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	Weapon->AttachToActor(this, AttachmentRule);

	// Attach the First-Person weapon + Cubic Farmer hands cleanly to the First-Person Camera
	// (eliminates the old gray Unreal Mannequin arms & broken pistol/SMG left-hand anim offsets!)
	if (UCameraComponent* FPCam = GetFirstPersonCameraComponent())
	{
		Weapon->GetFirstPersonMesh()->AttachToComponent(FPCam, AttachmentRule);
		Weapon->GetFirstPersonMesh()->SetRelativeLocationAndRotation(
			FVector(27.0f, 10.5f, -12.5f),
			FRotator(0.0f, -90.0f, 0.0f)
		);
	}
	else
	{
		Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	}

	// Attach Third-Person weapon mesh to the Farmer's right hand bone
	const FName ThirdPersonSocket = GetMesh() && GetMesh()->DoesSocketExist(FName("hand_r"))
		? FName("hand_r")
		: FirstPersonWeaponSocket;
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonSocket);
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	// play on the first person mesh
	TryPlayCharacterMontage(GetFirstPersonMesh(), Montage);

	// play on the third person mesh so other players see the shot
	TryPlayCharacterMontage(GetMesh(), Montage);
}

void AShooterCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	// play on the first person mesh
	TryPlayCharacterMontage(GetFirstPersonMesh(), Montage);

	// play on the third person mesh so other players see the reload
	TryPlayCharacterMontage(GetMesh(), Montage);
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
	// apply the recoil as pitch input
	AddControllerPitchInput(Recoil);
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
	// trace ahead from the camera viewpoint
	FHitResult OutHit;

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	// do we already own this weapon?
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (!OwnedWeapon)
	{
		// spawn the new weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (AddedWeapon)
		{
			// add the weapon to the owned list
			OwnedWeapons.Add(AddedWeapon);

			// if we have an existing weapon, deactivate it
			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			// switch to the new weapon
			CurrentWeapon = AddedWeapon;
			CurrentWeapon->ActivateWeapon();
		}
	}
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// update the bullet counter
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

	// set the character mesh AnimInstances (skip when using the Animated Cubic Farmer)
	if (!bUsingFarmerMesh)
	{
		GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
		GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
	}
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	// unused
}

void AShooterCharacter::OnSemiWeaponRefire()
{
	// unused
}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	// check each owned weapon
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	// weapon not found
	return nullptr;

}

void AShooterCharacter::Die()
{
	// clear anim instances before unpossession to avoid ABP_FP_Pistol Accessed None errors
	if (GetFirstPersonMesh())
	{
		GetFirstPersonMesh()->SetAnimInstanceClass(nullptr);
	}
	if (GetMesh())
	{
		if (bUsingFarmerMesh && FarmerAnim_Death)
		{
			PlayFarmerAnimSafe(GetMesh(), FarmerAnim_Death, false, 1.0f);
			CurrentFarmerAnim = FarmerAnim_Death;
		}
		else
		{
			GetMesh()->SetAnimInstanceClass(nullptr);
		}
	}

	// deactivate the weapon
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// increment the team score
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	// grant the death tag to the character
	Tags.Add(DeathTag);
		
	// stop character movement
	GetCharacterMovement()->StopMovementImmediately();

	// disable controls
	DisableInput(nullptr);

	// reset the bullet counter UI
	OnBulletCountUpdated.Broadcast(0, 0);

	// call the BP handler
	BP_OnDeath();

	// schedule character respawn
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
}

void AShooterCharacter::OnRespawn()
{
	// destroy the character to force the PC to respawn
	Destroy();
}

bool AShooterCharacter::IsDead() const
{
	// the character is dead if their current HP drops to zero
	return CurrentHP <= 0.0f;
}

void AShooterCharacter::SelectTool1()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::None);
	}
	else if (CurrentDimension == EGameDimension::Combat)
	{
		if (bIsWeaponWheelOpen && WeaponWheelWidget) { WeaponWheelWidget->SelectSlotByIndex(0); }
		else { EquipWeaponByIndex(0); }
	}
}

void AShooterCharacter::SelectTool2()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::Hoe);
	}
	else if (CurrentDimension == EGameDimension::Combat)
	{
		if (bIsWeaponWheelOpen && WeaponWheelWidget) { WeaponWheelWidget->SelectSlotByIndex(1); }
		else { EquipWeaponByIndex(1); }
	}
}

void AShooterCharacter::SelectTool3()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::WateringCan);
	}
	else if (CurrentDimension == EGameDimension::Combat)
	{
		if (bIsWeaponWheelOpen && WeaponWheelWidget) { WeaponWheelWidget->SelectSlotByIndex(2); }
		else { EquipWeaponByIndex(2); }
	}
}

void AShooterCharacter::SelectTool4()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::Harvester);
	}
	else if (CurrentDimension == EGameDimension::Combat)
	{
		if (bIsWeaponWheelOpen && WeaponWheelWidget) { WeaponWheelWidget->SelectSlotByIndex(3); }
		else { EquipWeaponByIndex(3); }
	}
}

void AShooterCharacter::SelectWeaponSlot5()
{
	if (CurrentDimension == EGameDimension::Combat)
	{
		if (bIsWeaponWheelOpen && WeaponWheelWidget) { WeaponWheelWidget->SelectSlotByIndex(4); }
		else { EquipWeaponByIndex(4); }
	}
}

void AShooterCharacter::SelectWeaponSlot6()
{
	if (CurrentDimension == EGameDimension::Combat)
	{
		if (bIsWeaponWheelOpen && WeaponWheelWidget) { WeaponWheelWidget->SelectSlotByIndex(5); }
		else { EquipWeaponByIndex(5); }
	}
}

void AShooterCharacter::SelectWeaponSlot7()
{
	if (CurrentDimension == EGameDimension::Combat)
	{
		if (bIsWeaponWheelOpen && WeaponWheelWidget) { WeaponWheelWidget->SelectSlotByIndex(6); }
		else { EquipWeaponByIndex(6); }
	}
}

void AShooterCharacter::OnTabPressed()
{
	if (IsDead())
	{
		return;
	}

	// In Farming dimension, Tab toggles the Tarkov Backpack inventory as before
	if (CurrentDimension == EGameDimension::Farming)
	{
		DoToggleInventory();
		return;
	}

	// Close Tarkov Inventory if open before bringing up the Radial Weapon Wheel
	if (TarkovInventory && TarkovInventory->IsInventoryOpen())
	{
		TarkovInventory->ToggleInventory();
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (!WeaponWheelWidget)
	{
		WeaponWheelWidget = CreateWidget<UShooterWeaponWheelWidget>(PC, UShooterWeaponWheelWidget::StaticClass());
		if (WeaponWheelWidget)
		{
			WeaponWheelWidget->AddToViewport(60);
		}
	}

	if (WeaponWheelWidget)
	{
		bIsWeaponWheelOpen = true;
		DoStopFiring();
		WeaponWheelWidget->OpenWheel(this);
	}
}

void AShooterCharacter::OnTabReleased()
{
	if (!bIsWeaponWheelOpen)
	{
		return;
	}

	bIsWeaponWheelOpen = false;
	if (WeaponWheelWidget)
	{
		WeaponWheelWidget->CloseWheelAndCommit();
	}
}

bool AShooterCharacter::IsWeaponSlotUnlocked(int32 WeaponIndex) const
{
	if (WeaponIndex == 0)
	{
		return true; // Slot 0 (M1911 Classic) is always owned at the start
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			return Sub->IsWeaponUnlocked(WeaponIndex);
		}
	}
	return false;
}

int32 AShooterCharacter::GetWeaponSlotPrice(int32 WeaponIndex)
{
	static const int32 Prices[7] = { 0, 1000, 1800, 2600, 3500, 4500, 6000 };
	return (WeaponIndex >= 0 && WeaponIndex < 7) ? Prices[WeaponIndex] : 1000;
}

void AShooterCharacter::EquipWeaponByIndex(int32 WeaponIndex)
{
	if (IsDead() || !OwnedWeapons.IsValidIndex(WeaponIndex))
	{
		return;
	}

	AShooterWeapon* TargetWeapon = OwnedWeapons[WeaponIndex];
	if (!TargetWeapon)
	{
		return;
	}

	if (!IsWeaponSlotUnlocked(WeaponIndex))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				9175, 3.0f, FColor::Orange,
				FString::Printf(TEXT("[LOCKED WEAPON] %s is locked! Buy it in the Barn Shop -> GUNS Tab (%d Gold)."),
					*TargetWeapon->GetWeaponDisplayName(), GetWeaponSlotPrice(WeaponIndex))
			);
		}
		return;
	}

	if (CurrentWeapon && CurrentWeapon != TargetWeapon)
	{
		CurrentWeapon->DeactivateWeapon();
	}

	CurrentWeapon = TargetWeapon;
	CurrentWeapon->ActivateWeapon();

	if (TarkovInventory)
	{
		TarkovInventory->bHasEquippedWeapon = true;
		TarkovInventory->EquippedWeaponItem.ItemName = CurrentWeapon->GetWeaponDisplayName();
		TarkovInventory->EquippedWeaponItem.WeaponClass = CurrentWeapon->GetClass();
		TarkovInventory->EquippedWeaponItem.AmmoCount = CurrentWeapon->GetBulletCount();
		TarkovInventory->OnInventoryUpdated.Broadcast();
	}
}

void AShooterCharacter::SpawnAllGunPackWeapons()
{
	if (!GetWorld())
	{
		return;
	}

	// Load shared projectile, anim instances, montages, and sound waves from Variant_Shooter
	TSubclassOf<AShooterProjectile> BulletProjClass = LoadClass<AShooterProjectile>(
		nullptr, TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Projectiles/BP_ShooterProjectile_Bullet.BP_ShooterProjectile_Bullet_C"));
	TSubclassOf<UAnimInstance> FPPistolAnim = LoadClass<UAnimInstance>(
		nullptr, TEXT("/Game/Variant_Shooter/Anims/ABP_FP_Pistol.ABP_FP_Pistol_C"));
	TSubclassOf<UAnimInstance> TPPistolAnim = LoadClass<UAnimInstance>(
		nullptr, TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Pistol.ABP_TP_Pistol_C"));
	TSubclassOf<UAnimInstance> FPRifleAnim = LoadClass<UAnimInstance>(
		nullptr, TEXT("/Game/Variant_Shooter/Anims/ABP_FP_Weapon.ABP_FP_Weapon_C"));
	TSubclassOf<UAnimInstance> TPRifleAnim = LoadClass<UAnimInstance>(
		nullptr, TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C"));

	UAnimMontage* PistolFireMontage = LoadObject<UAnimMontage>(
		nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage.MM_Pistol_Fire_Montage"));
	UAnimMontage* PistolReloadMontage = LoadObject<UAnimMontage>(
		nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload_Montage.MM_Pistol_Reload_Montage"));
	UAnimMontage* RifleFireMontage = LoadObject<UAnimMontage>(
		nullptr, TEXT("/Game/Variant_Shooter/Anims/FP_Rifle_Shoot_Montage.FP_Rifle_Shoot_Montage"));

	USoundBase* ClassicPistolFireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_Pistol_Classic.SFX_Pistol_Classic"));
	USoundBase* TacticalPistolFireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_Pistol_Tactical.SFX_Pistol_Tactical"));
	USoundBase* MP5FireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_Auto_MP5.SFX_Auto_MP5"));
	USoundBase* MP7FireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_Auto_MP7.SFX_Auto_MP7"));
	USoundBase* M4A1FireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_Auto_M4A1.SFX_Auto_M4A1"));
	USoundBase* M16FireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_Auto_M16.SFX_Auto_M16"));
	USoundBase* SCARFireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Dev_Blueprint/Audio/SFX_Auto_SCAR.SFX_Auto_SCAR"));
	USoundBase* DefaultFireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Weapons/Pistol/Audio/S_Pistol_Fire.S_Pistol_Fire"));
	USoundBase* ReloadSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Weapons/Pistol/Audio/S_Pistol_Reload.S_Pistol_Reload"));
	USoundBase* DryFireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Weapons/Pistol/Audio/S_Pistol_DryFire.S_Pistol_DryFire"));

	TSubclassOf<AShooterWeapon> BaseWepClass = InitialWeaponClass
		? InitialWeaponClass
		: TSubclassOf<AShooterWeapon>(AShooterWeapon::StaticClass());

	struct FGunPackSpec
	{
		const TCHAR* DisplayName;
		const TCHAR* Subtitle;
		const TCHAR* MeshPath;
		const TCHAR* AmmoType;
		int32 MagSize;
		bool bFullAuto;
		float RefireRate;
		float ReloadDuration;
		float Recoil;
		float AimVariance;
		FVector Offset;
		FRotator Rotation;
		float Scale;
		bool bUsePistolRig;
	};

	const FGunPackSpec Specs[7] = {
		// 1. M1911 Classic (FBX authored pointing -Y, so rotate Yaw=180 deg so barrel points forward +Y and shift grip to HandGrip_R)
		{
			TEXT("M1911 Classic"),
			TEXT("Semi-Auto Sidearm"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M1911.SM_Gun_M1911"),
			TEXT("9mm Pistol Ammo"),
			12, false, 0.20f, 1.35f, -0.05f, 10.0f,
			FVector(0.0f, 9.8f, 3.2f), FRotator(0.0f, 180.0f, 0.0f), 0.052f, true
		},
		// 2. M1911 Tactical (FBX authored pointing -Y, so rotate Yaw=180 deg so barrel points forward +Y and shift grip to HandGrip_R)
		{
			TEXT("M1911 Tactical"),
			TEXT("Custom Match Pistol"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M1911Tactical.SM_Gun_M1911Tactical"),
			TEXT("9mm Pistol Ammo"),
			15, false, 0.15f, 1.25f, -0.04f, 8.0f,
			FVector(0.0f, 9.8f, 3.2f), FRotator(0.0f, 180.0f, 0.0f), 0.052f, true
		},
		// 3. MP5 Submachine Gun (FBX authored pointing +Y)
		{
			TEXT("MP5 SMG"),
			TEXT("9mm Tactical SMG"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_MP5.SM_Gun_MP5"),
			TEXT("9mm Pistol Ammo"),
			30, true, 0.085f, 1.55f, -0.06f, 16.0f,
			FVector(0.0f, 8.0f, 3.0f), FRotator(0.0f, 0.0f, 0.0f), 0.058f, false
		},
		// 4. MP7 PDW (FBX authored pointing +Y)
		{
			TEXT("MP7 Personal Defense"),
			TEXT("High-ROF Compact SMG"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_MP7.SM_Gun_MP7"),
			TEXT("9mm Pistol Ammo"),
			40, true, 0.068f, 1.45f, -0.055f, 18.0f,
			FVector(0.0f, 9.0f, 4.0f), FRotator(0.0f, 0.0f, 0.0f), 0.065f, false
		},
		// 5. M4A1 Carbine (FBX authored pointing +Y)
		{
			TEXT("M4A1 Carbine"),
			TEXT("Full-Auto Assault Carbine"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M4A1.SM_Gun_M4A1"),
			TEXT("5.56mm Rifle Ammo"),
			30, true, 0.095f, 1.65f, -0.085f, 14.0f,
			FVector(0.0f, 11.0f, 2.5f), FRotator(0.0f, 0.0f, 0.0f), 0.056f, false
		},
		// 6. M16A4 Assault Rifle (FBX authored pointing -Y, so rotate Yaw=180 deg so barrel points forward +Y)
		{
			TEXT("M16A4 Rifle"),
			TEXT("Precision Service Rifle"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M16.SM_Gun_M16"),
			TEXT("5.56mm Rifle Ammo"),
			30, true, 0.11f, 1.70f, -0.075f, 9.0f,
			FVector(0.0f, 15.0f, 3.0f), FRotator(0.0f, 180.0f, 0.0f), 0.048f, false
		},
		// 7. FN SCAR-H (FBX authored pointing +Y)
		{
			TEXT("SCAR-H Heavy"),
			TEXT("Heavy Battle Rifle"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_SCAR.SM_Gun_SCAR"),
			TEXT("5.56mm Rifle Ammo"),
			20, true, 0.135f, 1.80f, -0.11f, 7.0f,
			FVector(0.0f, 16.0f, 3.0f), FRotator(0.0f, 0.0f, 0.0f), 0.048f, false
		}
	};

	OwnedWeapons.Reset();

	for (int32 i = 0; i < 7; ++i)
	{
		const FGunPackSpec& S = Specs[i];

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* NewWep = GetWorld()->SpawnActor<AShooterWeapon>(BaseWepClass, GetActorTransform(), SpawnParams);
		if (!NewWep)
		{
			continue;
		}

		USoundBase* ChosenFireSnd = DefaultFireSnd;
		switch (i)
		{
		case 0: if (ClassicPistolFireSnd)  { ChosenFireSnd = ClassicPistolFireSnd; }  break;
		case 1: if (TacticalPistolFireSnd) { ChosenFireSnd = TacticalPistolFireSnd; } break;
		case 2: if (MP5FireSnd)            { ChosenFireSnd = MP5FireSnd; }            break;
		case 3: if (MP7FireSnd)            { ChosenFireSnd = MP7FireSnd; }            break;
		case 4: if (M4A1FireSnd)           { ChosenFireSnd = M4A1FireSnd; }           break;
		case 5: if (M16FireSnd)            { ChosenFireSnd = M16FireSnd; }            break;
		case 6: if (SCARFireSnd)           { ChosenFireSnd = SCARFireSnd; }           break;
		default: break;
		}

		NewWep->ConfigureCustomGunPackArchetype(
			S.DisplayName,
			S.Subtitle,
			S.MeshPath,
			S.AmmoType,
			S.MagSize,
			S.bFullAuto,
			S.RefireRate,
			S.ReloadDuration,
			S.Recoil,
			S.AimVariance,
			S.Offset,
			S.Rotation,
			S.Scale,
			BulletProjClass,
			S.bUsePistolRig ? FPPistolAnim : FPRifleAnim,
			S.bUsePistolRig ? TPPistolAnim : TPRifleAnim,
			S.bUsePistolRig ? PistolFireMontage : RifleFireMontage,
			PistolReloadMontage,
			ChosenFireSnd,
			ReloadSnd,
			DryFireSnd
		);

		NewWep->DeactivateWeapon();
		OwnedWeapons.Add(NewWep);
	}

	if (OwnedWeapons.Num() > 0)
	{
		EquipWeaponByIndex(0);
	}
}

void AShooterCharacter::CycleToolNext()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->CycleTool(true);
	}
}

void AShooterCharacter::CycleToolPrevious()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->CycleTool(false);
	}
}

void AShooterCharacter::CycleSeedNext()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->CycleSeed(true);
	}
}

void AShooterCharacter::Heal(float Amount)
{
	if (IsDead() || Amount <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));
}

void AShooterCharacter::UseMedkit()
{
	UInterWorldBridge::UseMedkitFromInventory(this, 50.0f);
}

void AShooterCharacter::InitializeFarmerCharacterMesh()
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (!BodyMesh)
	{
		return;
	}

	USkeletalMesh* FarmerMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/3D_LOW_POLY_FarmerPack/Characters/Mesh/SKM_Farmer_male.SKM_Farmer_male"));
	if (!FarmerMesh)
	{
		return;
	}

	bUsingFarmerMesh = true;

	// Save the default First-Person camera transform for [V] Third-Person toggle
	if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		DefaultCameraRelLocation = Camera->GetRelativeLocation();
		DefaultCameraRelRotation = Camera->GetRelativeRotation();
		bSavedUsePawnControlRotation = Camera->bUsePawnControlRotation;
	}

	// Swap the main character body mesh to the Animated Low-Poly Cubic Farmer with bReinitPose=true
	// and drive animations through UZombieAnimInstance (FAnimInstanceProxy) so poses & shadows NEVER freeze in T-Pose!
	BodyMesh->SetAnimInstanceClass(nullptr);
	BodyMesh->SetSkeletalMesh(FarmerMesh, true);
	BodyMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));
	BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	BodyMesh->bEnableUpdateRateOptimizations = false;
	BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	BodyMesh->SetAnimInstanceClass(UZombieAnimInstance::StaticClass());

	// Disable visibility and shadow casting on any auxiliary character SkeletalMeshComponents (e.g. FirstPersonMesh
	// SKM_Manny_Simple) WITHOUT propagating hidden flags to FirstPersonCameraComponent or its children!
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

	// Load the Farmer animation sequences from the pack + retargeted Mixamo "Plant A Plant" sequence
	const TCHAR* AnimBase = TEXT("/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/");
	FarmerAnim_Idle       = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_idle_basic.anim_Farmer_idle_basic"), AnimBase));
	FarmerAnim_IdleCombat = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_idle_combat.anim_Farmer_idle_combat"), AnimBase));
	FarmerAnim_Walk       = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_walk.anim_Farmer_walk"), AnimBase));
	FarmerAnim_Run        = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_run.anim_Farmer_run"), AnimBase));
	FarmerAnim_WorkHoe    = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_work_Hoe.anim_Farmer_work_Hoe"), AnimBase));
	FarmerAnim_GrabLow    = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_grab_low.anim_Farmer_grab_low"), AnimBase));
	FarmerAnim_HitReact   = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_hit_reaction.anim_Farmer_hit_reaction"), AnimBase));
	FarmerAnim_Death      = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sanim_Farmer_death.anim_Farmer_death"), AnimBase));

	FFarmerActionAnimState& ActionState = GetFarmerActionState(this);
	if (UAnimSequence* LoadedPlantAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Dev_Blueprint/Characters/Farmer/Animations/Anim_Farmer_Plant.Anim_Farmer_Plant")))
	{
		ActionState.PlantAnim = TStrongObjectPtr<UAnimSequence>(LoadedPlantAnim);
	}

	if (FarmerAnim_Idle)
	{
		PlayFarmerAnimSafe(BodyMesh, FarmerAnim_Idle, true, 1.0f);
		CurrentFarmerAnim = FarmerAnim_Idle;
		BodyMesh->TickPose(0.0f, false);
		BodyMesh->RefreshBoneTransforms();
	}

	// Calibrate exact standing head-bone-to-camera mapping so First-Person tool animations match head sway 1:1
	if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		ActionState.StandingCamActorLocalOffset = GetActorTransform().InverseTransformPosition(Camera->GetComponentLocation());
	}
	if (BodyMesh->GetBoneIndex(FName("Head")) != INDEX_NONE)
	{
		ActionState.StandingHeadActorLocalPos = GetActorTransform().InverseTransformPosition(BodyMesh->GetBoneLocation(FName("Head")));
		const FQuat IdleHeadWorldQuat = BodyMesh->GetBoneQuaternion(FName("Head"), EBoneSpaces::WorldSpace);
		ActionState.HeadBoneToEyeLocalQuat = IdleHeadWorldQuat.Inverse() * GetActorQuat();
		ActionState.bCalibrated = true;
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("ShooterCharacter: Equipped Animated Cubic Farmer (SKM_Farmer_male, AnimInst=%s, PlantAnim=%s)."),
		*GetNameSafe(BodyMesh->GetAnimInstance()),
		*GetNameSafe(ActionState.PlantAnim.Get()));
}

void AShooterCharacter::PlayFarmerOneShotAnim(UAnimSequence* Anim, float PlayRate)
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (!bUsingFarmerMesh || !BodyMesh || !Anim || IsDead())
	{
		return;
	}

	PlayFarmerAnimSafe(BodyMesh, Anim, false, PlayRate);
	CurrentFarmerAnim = Anim;
	FarmerOneShotRemaining = FMath::Max(0.25f, Anim->GetPlayLength() / FMath::Max(0.25f, PlayRate));
}

void AShooterCharacter::UpdateFarmerAnimation(float DeltaTime)
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	FFarmerActionAnimState& ActionState = GetFarmerActionState(this);

	// Update manual [V] Third-Person shoulder camera when NOT running the dedicated Planting 3/4 camera
	if (bIsThirdPersonView && !ActionState.bIsPlanting && !bIsSleepingInBed)
	{
		if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
		{
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				const FRotator RawControlRot = PC->GetControlRotation();
				const float SignedPitch = FMath::Clamp(FRotator::NormalizeAxis(RawControlRot.Pitch), -42.0f, 35.0f);
				const FRotator OrbitRot(SignedPitch, RawControlRot.Yaw, 0.0f);
				const float PelvisWorldZ = (BodyMesh && BodyMesh->GetBoneIndex(FName("pelvis")) != INDEX_NONE)
					? BodyMesh->GetBoneLocation(FName("pelvis")).Z
					: GetActorLocation().Z;
				const FVector CharacterCenter(
					GetActorLocation().X,
					GetActorLocation().Y,
					FMath::Clamp(PelvisWorldZ + 18.0f, GetActorLocation().Z - 48.0f, GetActorLocation().Z + 32.0f));
				const FVector DesiredPos = CharacterCenter
					- OrbitRot.Vector() * 235.0f
					+ FRotationMatrix(OrbitRot).GetUnitAxis(EAxis::Y) * 24.0f
					+ FVector(0.0f, 0.0f, 22.0f);

				FVector FinalPos = DesiredPos;
				FHitResult CamHit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(FarmerThirdPersonCam), false);
				Params.AddIgnoredActor(this);
				if (GetWorld() && GetWorld()->LineTraceSingleByChannel(CamHit, CharacterCenter, DesiredPos, ECC_Camera, Params))
				{
					FinalPos = CamHit.ImpactPoint + (CharacterCenter - DesiredPos).GetSafeNormal() * 15.0f;
				}

				const FVector Forward2D = FRotationMatrix(FRotator(0.0f, OrbitRot.Yaw, 0.0f)).GetUnitAxis(EAxis::X);
				const FVector LookFocus = CharacterCenter + Forward2D * 45.0f;
				const FRotator LookRot = (LookFocus - FinalPos).Rotation();

				Camera->SetUsingAbsoluteLocation(true);
				Camera->SetUsingAbsoluteRotation(true);
				Camera->SetWorldLocationAndRotation(FinalPos, LookRot);
			}
		}
	}

	if (!bUsingFarmerMesh || !BodyMesh || IsDead())
	{
		if (ActionState.bIsPlanting || ActionState.bDrivingFPCamera)
		{
			EndPlantThirdPerson(this, ActionState, bIsThirdPersonView, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation);
			RestoreFarmerActionCamera(this, ActionState, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation, bIsThirdPersonView, bIsSleepingInBed);
		}
		return;
	}

	if (FarmerOneShotRemaining > 0.0f)
	{
		FarmerOneShotRemaining -= DeltaTime;
		const float TotalDur = FMath::Max(0.25f, ActionState.TotalOneShotDuration);
		const float Elapsed = FMath::Clamp(TotalDur - FarmerOneShotRemaining, 0.0f, TotalDur);
		const float Progress = FMath::Clamp(Elapsed / TotalDur, 0.0f, 1.0f);

		// Ensure UZombieAnimInstance stays active on BodyMesh for the entire one-shot duration (never frozen in T-Pose!)
		EnsureFarmerAnimInstance(BodyMesh);

		// 1. During "Plant A Plant": position Farmer at plot edge, frame full VISIBLE Farmer + Arms + Soil Plot in an
		//    unobstructed inside-the-fence 3/4 Third-Person view, and pop the seedling visible right between both hands at 45%!
		if (ActionState.bIsPlanting)
		{
			// Guarantee BodyMesh is visible in Third-Person (FirstPersonPrimitiveType::None + OwnerNoSee=false)
			if (BodyMesh->FirstPersonPrimitiveType != EFirstPersonPrimitiveType::None)
			{
				ApplyFarmerViewVisibility(this, true);
			}

			const float StepAlpha = FMath::SmoothStep(0.0f, 0.30f, Elapsed);
			const FVector NewActorLoc = FMath::Lerp(ActionState.PlantStartActorLoc, ActionState.PlantTargetActorLoc, StepAlpha);
			const float NewYaw = FMath::FixedTurn(ActionState.PlantStartYaw, ActionState.PlantTargetYaw, FMath::Abs(FMath::FindDeltaAngleDegrees(ActionState.PlantStartYaw, ActionState.PlantTargetYaw)) * StepAlpha);
			SetActorLocationAndRotation(NewActorLoc, FRotator(0.0f, NewYaw, 0.0f), false, nullptr, ETeleportType::None);

			if (!ActionState.bSeedRevealed && Progress >= 0.45f)
			{
				RevealPendingPlantSprout(ActionState);
			}

			if (!bIsSleepingInBed)
			{
				UCameraComponent* Camera = GetFirstPersonCameraComponent();
				APlayerController* PC = Cast<APlayerController>(GetController());
				if (Camera && PC)
				{
					const FVector BaseEyeWorldLoc = GetActorTransform().TransformPosition(ActionState.StandingCamActorLocalOffset);
					const FRotator ExitFirstPersonRot(-18.0f, ActionState.PlantTargetYaw, 0.0f);
					PC->SetControlRotation(ExitFirstPersonRot);

					const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.0f;
					const float GroundZ = ActionState.PlantTargetActorLoc.Z - HalfHeight;
					const FVector KneelBase(ActionState.PlantTargetActorLoc.X, ActionState.PlantTargetActorLoc.Y, GroundZ);
					const FRotator PlantFacing(0.0f, ActionState.PlantTargetYaw, 0.0f);
					const FVector Fwd = FRotationMatrix(PlantFacing).GetUnitAxis(EAxis::X);
					const FVector Right = FRotationMatrix(PlantFacing).GetUnitAxis(EAxis::Y);

					const float PelvisHeightAboveGround = (BodyMesh->GetBoneIndex(FName("pelvis")) != INDEX_NONE)
						? FMath::Clamp(BodyMesh->GetBoneLocation(FName("pelvis")).Z - GroundZ, 36.0f, 88.0f)
						: 48.0f;

					// Midpoint between the kneeling Farmer and his hands on the soil plot so BOTH stay dead-center in frame!
					const FVector ActionFocusPoint = KneelBase + Fwd * 24.0f + FVector(0.0f, 0.0f, PelvisHeightAboveGround + 4.0f);
					const FVector TraceOrigin = KneelBase + Fwd * 24.0f + FVector(0.0f, 0.0f, 54.0f);

					// 4 closer 3/4 candidates (inside the wooden garden fence!) tested against ECC_Visibility (fences/walls/barn)
					const FVector Candidates[4] = {
						TraceOrigin - Fwd * 110.0f + Right * 122.0f + FVector(0.0f, 0.0f, 62.0f), // 0: Rear-Right 3/4
						TraceOrigin - Fwd * 110.0f - Right * 122.0f + FVector(0.0f, 0.0f, 62.0f), // 1: Rear-Left 3/4
						TraceOrigin + Fwd * 92.0f  + Right * 130.0f + FVector(0.0f, 0.0f, 60.0f), // 2: Front-Right 3/4 (if fence is behind Farmer)
						TraceOrigin + Fwd * 92.0f  - Right * 130.0f + FVector(0.0f, 0.0f, 60.0f)  // 3: Front-Left 3/4
					};

					UWorld* World = GetWorld();
					FCollisionQueryParams CamParams(SCENE_QUERY_STAT(PlantThirdPersonCam), false);
					CamParams.AddIgnoredActor(this);
					if (World)
					{
						for (TActorIterator<AFarmPlotBase> PlotIt(World); PlotIt; ++PlotIt)
						{
							CamParams.AddIgnoredActor(*PlotIt);
						}
					}

					const FCollisionShape ProbeSphere = FCollisionShape::MakeSphere(15.0f);
					if (ActionState.ChosenCamSideIndex < 0 || ActionState.ChosenCamSideIndex >= 4)
					{
						int32 BestIdx = 0;
						float BestClearFraction = -1.0f;
						for (int32 i = 0; i < 4; ++i)
						{
							FHitResult SweepHit;
							const bool bHitVis = World && World->SweepSingleByChannel(
								SweepHit, TraceOrigin, Candidates[i], FQuat::Identity, ECC_Visibility, ProbeSphere, CamParams);
							const float ClearFrac = bHitVis ? SweepHit.Time : 1.0f;
							if (ClearFrac > BestClearFraction + 0.05f)
							{
								BestClearFraction = ClearFrac;
								BestIdx = i;
								if (ClearFrac >= 0.99f)
								{
									break; // completely unobstructed!
								}
							}
						}
						ActionState.ChosenCamSideIndex = BestIdx;
					}

					FVector ChosenThirdPersonPos = Candidates[ActionState.ChosenCamSideIndex];
					if (World)
					{
						FHitResult SweepHit;
						if (World->SweepSingleByChannel(
							SweepHit, TraceOrigin, ChosenThirdPersonPos, FQuat::Identity, ECC_Visibility, ProbeSphere, CamParams))
						{
							const float SafeAlpha = FMath::Clamp(SweepHit.Time - 0.10f, 0.38f, 1.0f);
							ChosenThirdPersonPos = FMath::Lerp(TraceOrigin, ChosenThirdPersonPos, SafeAlpha);
						}
					}

					const FRotator ChosenThirdPersonRot = (ActionFocusPoint - ChosenThirdPersonPos).Rotation();

					// Smoothly pull out from First-Person (0.28s) and return to First-Person at completion (0.35s)
					float ThirdPersonBlend = 1.0f;
					if (ActionState.bForcedThirdPersonForPlant)
					{
						const float InAlpha  = FMath::SmoothStep(0.0f, 0.28f, Elapsed);
						const float OutAlpha = FMath::SmoothStep(0.0f, 0.35f, FMath::Max(0.0f, FarmerOneShotRemaining));
						ThirdPersonBlend = FMath::Min(InAlpha, OutAlpha);

						// Keep head visible whenever camera is pulled back; hide only right at the eye transition endpoints
						if (FarmerOneShotRemaining < 0.08f || Elapsed < 0.05f)
						{
							BodyMesh->HideBoneByName(FName("Head"), PBO_None);
						}
						else
						{
							BodyMesh->UnHideBoneByName(FName("Head"));
						}
					}

					if (!ActionState.bHasSmoothedCam)
					{
						ActionState.SmoothedAnimCamLoc = ChosenThirdPersonPos;
						ActionState.SmoothedAnimCamQuat = ChosenThirdPersonRot.Quaternion();
						ActionState.bHasSmoothedCam = true;
					}
					else
					{
						ActionState.SmoothedAnimCamLoc = FMath::VInterpTo(ActionState.SmoothedAnimCamLoc, ChosenThirdPersonPos, DeltaTime, 12.0f);
						ActionState.SmoothedAnimCamQuat = FMath::QInterpTo(ActionState.SmoothedAnimCamQuat, ChosenThirdPersonRot.Quaternion(), DeltaTime, 12.0f);
					}

					const FVector FinalCamLoc = FMath::Lerp(BaseEyeWorldLoc, ActionState.SmoothedAnimCamLoc, ThirdPersonBlend);
					const FQuat FinalCamQuat = FQuat::Slerp(ExitFirstPersonRot.Quaternion(), ActionState.SmoothedAnimCamQuat, ThirdPersonBlend).GetNormalized();

					ActionState.bDrivingFPCamera = true;
					Camera->bUsePawnControlRotation = false;
					Camera->SetUsingAbsoluteLocation(true);
					Camera->SetUsingAbsoluteRotation(true);
					Camera->SetWorldLocationAndRotation(FinalCamLoc, FinalCamQuat.Rotator());
				}
			}
		}
		// 2. Non-planting First-Person farming tools (Hoe / Watering Can / Harvester): natural head-motion follow-through
		else if (!bIsThirdPersonView && !bIsSleepingInBed && CurrentDimension == EGameDimension::Farming && ActionState.bCalibrated)
		{
			UCameraComponent* Camera = GetFirstPersonCameraComponent();
			APlayerController* PC = Cast<APlayerController>(GetController());
			if (Camera && PC && BodyMesh->GetBoneIndex(FName("head")) != INDEX_NONE)
			{
				const FVector BaseEyeWorldLoc = GetActorTransform().TransformPosition(ActionState.StandingCamActorLocalOffset);
				const FRotator BaseEyeWorldRot = PC->GetControlRotation();

				const float InAlpha = FMath::SmoothStep(0.0f, 0.14f, Elapsed);
				const float OutAlpha = FMath::SmoothStep(0.0f, 0.20f, FMath::Max(0.0f, FarmerOneShotRemaining));
				const float BlendAlpha = FMath::Min(InAlpha, OutAlpha);

				const FVector HeadActorLocal = GetActorTransform().InverseTransformPosition(BodyMesh->GetBoneLocation(FName("head")));
				const FVector HeadLocalDelta = HeadActorLocal - ActionState.StandingHeadActorLocalPos;
				const FVector TargetAnimCamLoc = BaseEyeWorldLoc + GetActorQuat().RotateVector(HeadLocalDelta * 0.65f);

				const FQuat HeadEyeQuat = BodyMesh->GetBoneQuaternion(FName("head"), EBoneSpaces::WorldSpace) * ActionState.HeadBoneToEyeLocalQuat;
				const FRotator HeadLocalRotDelta = (GetActorQuat().Inverse() * HeadEyeQuat).Rotator();
				FRotator FollowRot = BaseEyeWorldRot;
				FollowRot.Pitch = FMath::Clamp(BaseEyeWorldRot.Pitch + HeadLocalRotDelta.Pitch * 0.45f, -85.0f, 85.0f);
				FollowRot.Yaw   = BaseEyeWorldRot.Yaw + HeadLocalRotDelta.Yaw * 0.35f;
				FollowRot.Roll  = HeadLocalRotDelta.Roll * 0.20f;
				const FQuat TargetAnimCamQuat = FollowRot.Quaternion();

				if (!ActionState.bHasSmoothedCam)
				{
					ActionState.SmoothedAnimCamLoc = TargetAnimCamLoc;
					ActionState.SmoothedAnimCamQuat = TargetAnimCamQuat;
					ActionState.bHasSmoothedCam = true;
				}
				else
				{
					ActionState.SmoothedAnimCamLoc = FMath::VInterpTo(ActionState.SmoothedAnimCamLoc, TargetAnimCamLoc, DeltaTime, 14.0f);
					ActionState.SmoothedAnimCamQuat = FMath::QInterpTo(ActionState.SmoothedAnimCamQuat, TargetAnimCamQuat, DeltaTime, 14.0f);
				}

				const FVector FinalCamLoc = FMath::Lerp(BaseEyeWorldLoc, ActionState.SmoothedAnimCamLoc, BlendAlpha);
				const FQuat FinalCamQuat = FQuat::Slerp(BaseEyeWorldRot.Quaternion(), ActionState.SmoothedAnimCamQuat, BlendAlpha).GetNormalized();

				ActionState.bDrivingFPCamera = true;
				Camera->bUsePawnControlRotation = false;
				Camera->SetUsingAbsoluteLocation(true);
				Camera->SetUsingAbsoluteRotation(true);
				Camera->SetWorldLocationAndRotation(FinalCamLoc, FinalCamQuat.Rotator());
			}
		}

		if (FarmerOneShotRemaining > 0.0f)
		{
			return;
		}

		CurrentFarmerAnim = nullptr;
		EndPlantThirdPerson(this, ActionState, bIsThirdPersonView, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation);
		RestoreFarmerActionCamera(this, ActionState, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation, bIsThirdPersonView, bIsSleepingInBed);
	}
	else if (ActionState.bIsPlanting || ActionState.bDrivingFPCamera)
	{
		EndPlantThirdPerson(this, ActionState, bIsThirdPersonView, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation);
		RestoreFarmerActionCamera(this, ActionState, DefaultCameraRelLocation, DefaultCameraRelRotation, bSavedUsePawnControlRotation, bIsThirdPersonView, bIsSleepingInBed);
	}

	const float GroundSpeed = GetVelocity().Size2D();
	UAnimSequence* DesiredAnim = nullptr;
	float PlayRate = 1.0f;

	if (GroundSpeed > 380.0f && FarmerAnim_Run)
	{
		DesiredAnim = FarmerAnim_Run;
		PlayRate = FMath::Clamp(GroundSpeed / 450.0f, 0.85f, 1.35f);
	}
	else if (GroundSpeed > 25.0f && FarmerAnim_Walk)
	{
		DesiredAnim = FarmerAnim_Walk;
		PlayRate = FMath::Clamp(GroundSpeed / 260.0f, 0.8f, 1.45f);
	}
	else
	{
		DesiredAnim = (CurrentDimension == EGameDimension::Combat && FarmerAnim_IdleCombat)
			? FarmerAnim_IdleCombat
			: FarmerAnim_Idle;
	}

	UZombieAnimInstance* FAnim = EnsureFarmerAnimInstance(BodyMesh);
	if (DesiredAnim && (CurrentFarmerAnim != DesiredAnim || !FAnim))
	{
		PlayFarmerAnimSafe(BodyMesh, DesiredAnim, true, PlayRate);
		CurrentFarmerAnim = DesiredAnim;
	}
	else if (DesiredAnim && FAnim)
	{
		FAnim->SetZombiePlayRate(PlayRate);
	}
}

void AShooterCharacter::ToggleThirdPersonView()
{
	if (!bUsingFarmerMesh || bIsSleepingInBed || GetFarmerActionState(this).bIsPlanting || IsDead())
	{
		return;
	}

	bIsThirdPersonView = !bIsThirdPersonView;
	ApplyFarmerViewVisibility(this, bIsThirdPersonView);

	// Also toggle third-person gun visibility in Combat when pressing [V]
	if (CurrentWeapon && CurrentDimension == EGameDimension::Combat)
	{
		if (UStaticMeshComponent* TPGun = CurrentWeapon->GetThirdPersonGunMesh())
		{
			TPGun->SetFirstPersonPrimitiveType(
				bIsThirdPersonView
					? EFirstPersonPrimitiveType::None
					: EFirstPersonPrimitiveType::WorldSpaceRepresentation);
			TPGun->SetOwnerNoSee(!bIsThirdPersonView);
			TPGun->MarkRenderStateDirty();
		}
		if (USkeletalMeshComponent* FPWep = CurrentWeapon->GetFirstPersonMesh())
		{
			FPWep->SetVisibility(!bIsThirdPersonView, true);
		}
	}

	if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		if (bIsThirdPersonView)
		{
			Camera->bUsePawnControlRotation = false;
			Camera->SetUsingAbsoluteLocation(true);
			Camera->SetUsingAbsoluteRotation(true);
		}
		else
		{
			Camera->SetUsingAbsoluteLocation(false);
			Camera->SetUsingAbsoluteRotation(false);
			Camera->SetRelativeLocationAndRotation(DefaultCameraRelLocation, DefaultCameraRelRotation);
			Camera->bUsePawnControlRotation = bSavedUsePawnControlRotation;
		}
	}
}
