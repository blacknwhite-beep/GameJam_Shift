// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameJam_Shift_2026Character.h"
#include "ShooterWeaponHolder.h"
#include "DimensionTypes.h"
#include "ShooterCharacter.generated.h"

class AShooterWeapon;
class AFarmBedActor;
class UInputAction;
class UInputMappingContext;
class UInputComponent;
class UPawnNoiseEmitterComponent;
class UFarmingComponent;
class UCameraShakeBase;
class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class UShooterWeaponWheelWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletCountUpdatedDelegate, int32, MagazineSize, int32, Bullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamagedDelegate, float, LifePercent);

/**
 *  A player controllable first person shooter character
 *  Manages a weapon inventory through the IShooterWeaponHolder interface
 *  Manages health and death
 */
UCLASS(abstract)
class GAMEJAM_SHIFT_2026_API AShooterCharacter : public AGameJam_Shift_2026Character, public IShooterWeaponHolder
{
	GENERATED_BODY()
	
	/** AI Noise emitter component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPawnNoiseEmitterComponent* PawnNoiseEmitter;

	/** Tactical flashlight attached to the first-person camera (Combat/Zombie world only) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpotLightComponent> TacticalFlashlight;

	/** True while the tactical flashlight is turned on */
	bool bIsFlashlightOn = false;

protected:

	/** Fire weapon input action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* FireAction;

	/** Reload weapon input action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ReloadAction;

	/** Toggle inventory input action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ToggleInventoryAction;

	/** Switch weapon input action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SwitchWeaponAction;

	/** Dimension shift input action (Shift key) */
	UPROPERTY(EditAnywhere, Category = "Input|Dimension")
	UInputAction* DimensionShiftAction;

	/** Input mapping context applied when in the Farming dimension */
	UPROPERTY(EditAnywhere, Category = "Input|Dimension")
	UInputMappingContext* FarmingMappingContext;

	/** Input mapping context for weapons (removed in Farming, restored in Combat) */
	UPROPERTY(EditAnywhere, Category = "Input|Dimension")
	UInputMappingContext* WeaponMappingContext;

	/** Priority used when adding FarmingMappingContext */
	UPROPERTY(EditAnywhere, Category = "Input|Dimension")
	int32 FarmingMappingPriority = 1;

	/** The currently active dimension of the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dimension")
	EGameDimension CurrentDimension = EGameDimension::Combat;

	/** Initial weapon class to spawn and equip on spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapons")
	TSubclassOf<AShooterWeapon> InitialWeaponClass;

	/** Tarkov-style Grid Inventory Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UTarkovInventoryComponent> TarkovInventory;

	/** Farming Component managing tools, seeds, and soil plots */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UFarmingComponent> FarmingComponent;

	/** Name of the first person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	/** Name of the third person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	/** Max distance to use for aim traces */
	UPROPERTY(EditAnywhere, Category ="Aim", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	/** Max HP this character can have */
	UPROPERTY(EditAnywhere, Category="Health")
	float MaxHP = 500.0f;

	/** Current HP remaining to this character */
	float CurrentHP = 0.0f;

	/** Team ID for this character*/
	UPROPERTY(EditAnywhere, Category="Team")
	uint8 TeamByte = 0;

	/** Actor tag to grant this character when it dies */
	UPROPERTY(EditAnywhere, Category="Team")
	FName DeathTag = FName("Dead");

	/** List of weapons picked up by the character */
	UPROPERTY(Transient)
	TArray<AShooterWeapon*> OwnedWeapons;

	/** Weapon currently equipped and ready to shoot with */
	UPROPERTY(Transient)
	TObjectPtr<AShooterWeapon> CurrentWeapon;

	/** 7-Slot Radial Weapon Wheel widget (shown while holding [TAB] in Combat) */
	UPROPERTY(Transient)
	TObjectPtr<UShooterWeaponWheelWidget> WeaponWheelWidget;

	/** True while the [TAB] radial weapon wheel is currently open */
	bool bIsWeaponWheelOpen = false;

	/** True while the player is lying in the farmhouse bed */
	bool bIsSleepingInBed = false;

	/** Position before lying down, kept for reference */
	FVector PreSleepLocation = FVector::ZeroVector;

	/** Camera offset and rotation mode saved before sleeping, restored on wake */
	FVector DefaultCameraRelLocation = FVector::ZeroVector;
	FRotator DefaultCameraRelRotation = FRotator::ZeroRotator;
	bool bSavedUsePawnControlRotation = true;

	/** The bed the player is currently lying in */
	TWeakObjectPtr<AFarmBedActor> ActiveBed;

	UPROPERTY(EditAnywhere, Category ="Destruction", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 5.0f;

	FTimerHandle RespawnTimer;

	/** Camera shake played on dimension shift (CS_DimensionShift) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift|Game Feel")
	TSubclassOf<UCameraShakeBase> ShiftCameraShakeClass;

	/** Audio stinger played on dimension shift (Cue_DimensionShift) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift|Game Feel")
	TObjectPtr<USoundBase> ShiftSoundCue;

	/** Portal sparkle Niagara system spawned at player feet on dimension shift */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift|Game Feel")
	TObjectPtr<UNiagaraSystem> ShiftPortalVFX;

	/** Post-process glitch material applied during dimension shift */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift|Game Feel")
	TObjectPtr<UMaterialInterface> ShiftPostProcessMaterial;

	/** Duration in seconds for the post-process glitch fade-out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift|Game Feel", meta = (ClampMin = "0.05"))
	float ShiftGlitchDuration = 0.2f;

	/** Dynamic instance of ShiftPostProcessMaterial attached to the camera */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShiftPostProcessMID;

	/** Remaining fade time for the post-process glitch effect */
	float ShiftGlitchRemaining = 0.0f;

	/** Index of ShiftPostProcessMID inside FirstPersonCameraComponent->PostProcessSettings.WeightedBlendables */
	int32 ShiftPostProcessBlendableIndex = INDEX_NONE;

public:

	/** Bullet count updated delegate */
	FBulletCountUpdatedDelegate OnBulletCountUpdated;

	/** Damaged delegate */
	FDamagedDelegate OnDamaged;

public:

	/** Constructor */
	AShooterCharacter();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Per-frame update (fades dimension shift post-process glitch) */
	virtual void Tick(float DeltaTime) override;

	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:

	/** Handle incoming damage */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	/** Handles aim inputs from either controls or UI interfaces */
	virtual void DoAim(float Yaw, float Pitch) override;

	/** Handles move inputs from either controls or UI interfaces */
	virtual void DoMove(float Right, float Forward)  override;

	/** Handles jump start inputs from either controls or UI interfaces */
	virtual void DoJumpStart()  override;

	/** Handles jump end inputs from either controls or UI interfaces */
	virtual void DoJumpEnd()  override;

	/** Handles start firing input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStartFiring();

	/** Handles stop firing input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStopFiring();

	/** Handles switch weapon input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSwitchWeapon();

	/** Handles reload input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoReload();

	/** Handles toggle inventory input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoToggleInventory();

	/** Direct key fallback for toggling inventory while in the Farming dimension */
	void DoToggleInventoryFromFarming();

	/** Toggles the tactical flashlight (only works in Combat / Zombie dimension) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Flashlight")
	void ToggleFlashlight();

	/** Opens the Barn Shop if nearby in the Farming dimension */
	void InteractBarnShop();

	/** Developer Mode hotkey (+ / =): advances the 24-hour clock by 1 real-time minute equivalent */
	void DevAdvanceTimeInput();

	/** Cancels active Farm Plot placement preview */
	void CancelPlotPlacementInput();

	/** Handles dimension shift input (Shift key) */
	UFUNCTION(BlueprintCallable, Category = "Input|Dimension")
	void HandleDimensionShift();

	/** Responds to dimension shift events (e.g. swapping input contexts and weapons) */
	UFUNCTION()
	void HandleOnDimensionShifted(EGameDimension NewDimension, FVector TargetLocation);

	/** Blueprint hook called whenever a dimension shift completes */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dimension Shift")
	void BP_OnDimensionShifted(EGameDimension NewDimension, FVector TargetLocation);

	/** Returns the farming component */
	UFUNCTION(BlueprintCallable, Category = "Farming")
	UFarmingComponent* GetFarmingComponent() const { return FarmingComponent; }

	/**Farming tool direct selection and cycling actions */
	UFUNCTION(BlueprintCallable, Category = "Farming|Input")
	void SelectTool1();

	UFUNCTION(BlueprintCallable, Category = "Farming|Input")
	void SelectTool2();

	UFUNCTION(BlueprintCallable, Category = "Farming|Input")
	void SelectTool3();

	UFUNCTION(BlueprintCallable, Category = "Farming|Input")
	void SelectTool4();

	UFUNCTION(BlueprintCallable, Category = "Weapons|Input")
	void SelectWeaponSlot5();

	UFUNCTION(BlueprintCallable, Category = "Weapons|Input")
	void SelectWeaponSlot6();

	UFUNCTION(BlueprintCallable, Category = "Weapons|Input")
	void SelectWeaponSlot7();

	/** Spawns all 7 Gun Pack weapons into OwnedWeapons at startup (no purchasing restriction) */
	void SpawnAllGunPackWeapons();

	/** Equips an owned weapon by index (0..6) */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void EquipWeaponByIndex(int32 WeaponIndex);

	/** Returns the character's owned weapons array (the 7 guns in the radial wheel) */
	const TArray<AShooterWeapon*>& GetOwnedWeapons() const { return OwnedWeapons; }

	/** Returns true if the weapon slot (0..6) has been unlocked/purchased in the Barn Shop */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	bool IsWeaponSlotUnlocked(int32 WeaponIndex) const;

	/** Returns the Gold store price for the weapon slot (0..6) */
	static int32 GetWeaponSlotPrice(int32 WeaponIndex);

	/** Opens the 7-slot radial weapon wheel when [TAB] is pressed/held in Combat */
	void OnTabPressed();

	/** Closes the 7-slot radial weapon wheel and equips the highlighted gun when [TAB] is released */
	void OnTabReleased();

	/** Returns true while the radial weapon wheel is open */
	bool IsWeaponWheelOpen() const { return bIsWeaponWheelOpen; }

	UFUNCTION(BlueprintCallable, Category = "Farming|Input")
	void CycleToolNext();

	UFUNCTION(BlueprintCallable, Category = "Farming|Input")
	void CycleToolPrevious();

	/** [E]: lies down in the farmhouse bed at night, shows the bed-hours notice before midnight, or cycles tools */
	void InteractOrCycleToolNext();

	/** Lies down in the given bed Minecraft-style (90-degree roll) and locks movement and aim */
	void EnterBedSleep(AFarmBedActor* Bed);

	/** Gets out of bed, restoring camera roll, movement, and (optionally) the wake-up position */
	void ExitBedSleep(bool bRestorePosition);

	/** True while the player is lying in the farmhouse bed */
	bool IsSleepingInBed() const { return bIsSleepingInBed; }

	UFUNCTION(BlueprintCallable, Category = "Farming|Input")
	void CycleSeedNext();

	/** Restores character health (capped at MaxHP) */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Gets current HP */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHP() const { return CurrentHP; }

	/** Gets maximum HP */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHP() const { return MaxHP; }

	/** Uses a medkit from Tarkov inventory to restore health */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
	void UseMedkit();

public:

	/** Unequips the currently equipped weapon */
	UFUNCTION(BlueprintCallable, Category="Weapons")
	void UnequipCurrentWeapon();

	/** Equips a weapon of the specified class */
	UFUNCTION(BlueprintCallable, Category="Weapons")
	void EquipWeaponClass(TSubclassOf<AShooterWeapon> InWeaponClass);

	/** Returns the initial weapon class */
	UFUNCTION(BlueprintCallable, Category="Weapons")
	TSubclassOf<AShooterWeapon> GetInitialWeaponClass() const { return InitialWeaponClass; }

	/** Returns the currently active weapon */
	UFUNCTION(BlueprintPure, Category="Weapons")
	AShooterWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	//~Begin IShooterWeaponHolder interface

	/** Attaches a weapon's meshes to the owner */
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	/** Plays the firing montage for the weapon */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** Plays the reload montage for the weapon */
	virtual void PlayReloadMontage(UAnimMontage* Montage) override;

	/** Applies weapon recoil to the owner */
	virtual void AddWeaponRecoil(float Recoil) override;

	/** Updates the weapon's HUD with the current ammo count */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	/** Calculates and returns the aim location for the weapon */
	virtual FVector GetWeaponTargetLocation() override;

	/** Gives a weapon of this class to the owner */
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	/** Activates the passed weapon */
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	/** Deactivates the passed weapon */
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	/** Notifies the owner that the weapon cooldown has expired and it's ready to shoot again */
	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

protected:

	/** Returns true if the character already owns a weapon of the given class */
	AShooterWeapon* FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const;

	/** Called when this character's HP is depleted */
	void Die();

	/** Called to allow Blueprint code to react to this character's death */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

	/** Called from the respawn timer to destroy this character and force the PC to respawn */
	void OnRespawn();

public:

	/** Returns true if the character is dead */
	bool IsDead() const;
};
