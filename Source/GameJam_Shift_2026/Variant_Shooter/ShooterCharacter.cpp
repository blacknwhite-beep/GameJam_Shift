// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "DimensionShiftSubsystem.h"
#include "DimensionShiftCameraShake.h"
#include "FarmingComponent.h"
#include "Farming/BarnShopActor.h"
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
	TacticalFlashlight->SetRelativeLocation(FVector(15.0f, 8.0f, -6.0f));
	TacticalFlashlight->SetIntensityUnits(ELightUnits::Candelas);
	TacticalFlashlight->SetIntensity(45.0f);
	TacticalFlashlight->SetAttenuationRadius(2600.0f);
	TacticalFlashlight->SetInnerConeAngle(14.0f);
	TacticalFlashlight->SetOuterConeAngle(32.0f);
	TacticalFlashlight->SetLightColor(FColor(235, 244, 255));
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

	// update the HUD
	OnDamaged.Broadcast(1.0f);

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

		// Switch weapon
		EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoSwitchWeapon);

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
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AShooterCharacter::CycleToolNext);
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

}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// ignore if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
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
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoAim(Yaw, Pitch);
	}
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoMove(Right, Forward);
	}
}

void AShooterCharacter::DoJumpStart()
{
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
			FarmingComponent->UseActiveTool();
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

	// ensure we have at least two weapons two switch between
	if (OwnedWeapons.Num() > 1 && !IsDead())
	{
		// deactivate the old weapon
		CurrentWeapon->DeactivateWeapon();

		// find the index of the current weapon in the owned list
		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		// is this the last weapon?
		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			// loop back to the beginning of the array
			WeaponIndex = 0;
		}
		else {
			// select the next weapon index
			++WeaponIndex;
		}

		// set the new weapon as current
		CurrentWeapon = OwnedWeapons[WeaponIndex];

		// activate the new weapon
		CurrentWeapon->ActivateWeapon();
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
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->SetVisibility(false, true);
			GetFirstPersonMesh()->SetAnimInstanceClass(nullptr);
		}
		if (GetMesh())
		{
			GetMesh()->SetAnimInstanceClass(nullptr);
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

		// Restore first person mesh visibility
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->SetVisibility(true, true);
		}

		// Unholster and reactivate weapon when returning to combat
		if (CurrentWeapon)
		{
			CurrentWeapon->ActivateWeapon();

			// Restore character mesh AnimInstances
			if (GetFirstPersonMesh())
			{
				GetFirstPersonMesh()->SetAnimInstanceClass(CurrentWeapon->GetFirstPersonAnimInstanceClass());
			}
			if (GetMesh())
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

	// attach the weapon meshes
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, FirstPersonWeaponSocket);
	
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

	// set the character mesh AnimInstances
	GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
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
		GetMesh()->SetAnimInstanceClass(nullptr);
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

	USoundBase* FireSnd = LoadObject<USoundBase>(nullptr, TEXT("/Game/Weapons/Pistol/Audio/S_Pistol_Fire.S_Pistol_Fire"));
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
		// 1. M1911 Classic
		{
			TEXT("M1911 Classic"),
			TEXT("Semi-Auto Sidearm"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M1911.SM_Gun_M1911"),
			TEXT("9mm Pistol Ammo"),
			12, false, 0.20f, 1.35f, -0.05f, 10.0f,
			FVector(0.0f, 4.0f, 2.0f), FRotator(0.0f, 0.0f, 0.0f), 0.052f, true
		},
		// 2. M1911 Tactical
		{
			TEXT("M1911 Tactical"),
			TEXT("Custom Match Pistol"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M1911Tactical.SM_Gun_M1911Tactical"),
			TEXT("9mm Pistol Ammo"),
			15, false, 0.15f, 1.25f, -0.04f, 8.0f,
			FVector(0.0f, 4.0f, 2.0f), FRotator(0.0f, 0.0f, 0.0f), 0.052f, true
		},
		// 3. MP5 Submachine Gun
		{
			TEXT("MP5 SMG"),
			TEXT("9mm Tactical SMG"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_MP5.SM_Gun_MP5"),
			TEXT("9mm Pistol Ammo"),
			30, true, 0.085f, 1.55f, -0.06f, 16.0f,
			FVector(0.0f, 8.0f, 3.0f), FRotator(0.0f, 0.0f, 0.0f), 0.058f, false
		},
		// 4. MP7 PDW
		{
			TEXT("MP7 Personal Defense"),
			TEXT("High-ROF Compact SMG"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_MP7.SM_Gun_MP7"),
			TEXT("9mm Pistol Ammo"),
			40, true, 0.068f, 1.45f, -0.055f, 18.0f,
			FVector(0.0f, 9.0f, 4.0f), FRotator(0.0f, 0.0f, 0.0f), 0.065f, false
		},
		// 5. M4A1 Carbine
		{
			TEXT("M4A1 Carbine"),
			TEXT("Full-Auto Assault Carbine"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M4A1.SM_Gun_M4A1"),
			TEXT("5.56mm Rifle Ammo"),
			30, true, 0.095f, 1.65f, -0.085f, 14.0f,
			FVector(0.0f, 11.0f, 2.5f), FRotator(0.0f, 0.0f, 0.0f), 0.056f, false
		},
		// 6. M16A4 Assault Rifle
		{
			TEXT("M16A4 Rifle"),
			TEXT("Precision Service Rifle"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_M16.SM_Gun_M16"),
			TEXT("5.56mm Rifle Ammo"),
			30, true, 0.11f, 1.70f, -0.075f, 9.0f,
			FVector(0.0f, 14.0f, 3.0f), FRotator(0.0f, 0.0f, 0.0f), 0.048f, false
		},
		// 7. FN SCAR-H
		{
			TEXT("SCAR-H Heavy"),
			TEXT("Heavy Battle Rifle"),
			TEXT("/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_SCAR.SM_Gun_SCAR"),
			TEXT("5.56mm Rifle Ammo"),
			20, true, 0.135f, 1.80f, -0.11f, 7.0f,
			FVector(0.0f, 14.0f, 3.0f), FRotator(0.0f, 0.0f, 0.0f), 0.048f, false
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
			FireSnd,
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
