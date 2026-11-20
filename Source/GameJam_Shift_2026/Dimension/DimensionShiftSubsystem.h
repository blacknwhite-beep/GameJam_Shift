// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DimensionTypes.h"
#include "DimensionShiftSubsystem.generated.h"

class ACharacter;
class AStaticMeshActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoldChanged, int32, NewGold, int32, Delta);

/**
 * Subsystem responsible for managing dimension shifting between the Combat and Farming worlds.
 * Persists across level transitions via UGameInstance.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UDimensionShiftSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UDimensionShiftSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** World offset applied when shifting from Combat into the Farming dimension. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift")
	FVector FarmWorldOffset;

	/** Minimum time in seconds between consecutive dimension shifts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift", meta = (ClampMin = "0.0"))
	float ShiftCooldown;

	/** If true, verifies solid ground exists below destination to prevent falling into the void. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift")
	bool bRequireGroundCheck;

	/** The currently active dimension. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dimension Shift")
	EGameDimension CurrentDimension;

	/** Broadcasted whenever a dimension shift successfully occurs. */
	UPROPERTY(BlueprintAssignable, Category = "Dimension Shift")
	FOnDimensionShifted OnDimensionShifted;

	/**
	 * Requests a dimension shift for the given character.
	 * Performs safety checks, applies cooldown, teleports character, and broadcasts event.
	 * @param PlayerCharacter The character attempting to shift.
	 * @return True if the shift succeeded, false if on cooldown or obstructed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dimension Shift")
	bool RequestShift(ACharacter* PlayerCharacter);

	/**
	 * Checks whether shifting from SourceLocation to the alternate dimension is safe.
	 * Performs ground raycast and capsule collision sweep.
	 * @param SourceLocation Current world location of the character.
	 * @param OutTargetLocation Calculated destination world location if safe.
	 * @param World World context for collision queries.
	 * @return True if target ground exists and capsule clearance is valid.
	 */
	UFUNCTION(BlueprintPure, Category = "Dimension Shift")
	bool CanShiftSafely(const FVector& SourceLocation, FVector& OutTargetLocation, const UWorld* World) const;

	/** Returns the currently active dimension. */
	UFUNCTION(BlueprintPure, Category = "Dimension Shift")
	EGameDimension GetCurrentDimension() const { return CurrentDimension; }

	/** Forces the active dimension state without teleporting (useful for level startup). */
	UFUNCTION(BlueprintCallable, Category = "Dimension Shift")
	void SetCurrentDimension(EGameDimension NewDimension);

	/** Spawns a 1km x 1km solid prototype platform at Z = -20000 if no farm ground exists. */
	void EnsurePrototypeFarmPlatform(UWorld* World) const;

	/** Level built by the Blueprint Designer for the Farming dimension (streamed in at FarmWorldOffset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift|Farm Level")
	TSoftObjectPtr<UWorld> FarmLevel;

	/** Actor tag that marks sun/sky/fog actors belonging to the Farming dimension. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift|Farm Level")
	FName FarmWorldTag;

	/** Streams FarmLevel into the given world at FarmWorldOffset (idempotent). */
	UFUNCTION(BlueprintCallable, Category = "Dimension Shift|Farm Level")
	void EnsureFarmLevelLoaded(UWorld* World);

	/**
	 * Shows the sun/sky/fog of the active dimension and hides the other one.
	 * Actors tagged FarmWorldTag are visible only in Farming; untagged ones only in Combat.
	 * A combat light type is only hidden if the farm actually provides a replacement of that type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dimension Shift|Farm Level")
	void ApplyDimensionLighting(UWorld* World, EGameDimension Dimension) const;

	// -------------------------------------------------------------------------
	// Gold / Money Economy
	// -------------------------------------------------------------------------

	/** Initial gold balance granted when a new game session starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int32 StartingGold = 2000;

	/** Current gold balance carried across Combat and Farming dimensions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy")
	int32 CurrentGold = 2000;

	/** Broadcast whenever gold changes (NewGold, Delta). */
	UPROPERTY(BlueprintAssignable, Category = "Economy")
	FOnGoldChanged OnGoldChanged;

	/** Returns the player's current gold balance. */
	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetGold() const { return CurrentGold; }

	/** Adds (or subtracts if negative) gold and broadcasts OnGoldChanged. */
	UFUNCTION(BlueprintCallable, Category = "Economy")
	void AddGold(int32 Amount);

	/** Attempts to spend Amount gold; returns true and broadcasts if affordable. */
	UFUNCTION(BlueprintCallable, Category = "Economy")
	bool SpendGold(int32 Amount);

	// -------------------------------------------------------------------------
	// Weapon Arsenal Unlocks (Slots 0..6)
	// -------------------------------------------------------------------------

	/** Returns true if the weapon slot (0..6) is unlocked. Slot 0 (M1911 Classic) is always unlocked. */
	UFUNCTION(BlueprintPure, Category = "Economy|Weapons")
	bool IsWeaponUnlocked(int32 SlotIndex) const;

	/** Unlocks the given weapon slot (0..6). */
	UFUNCTION(BlueprintCallable, Category = "Economy|Weapons")
	void UnlockWeapon(int32 SlotIndex);

	/** True while the Start Menu split-screen overlay is active (before the player clicks Start Game). */
	bool bIsStartMenuActive = true;

	/** True once the player has dismissed the Start Menu in this session. */
	bool bHasDismissedStartMenu = false;

	// -------------------------------------------------------------------------
	// User Settings & Developer Mode
	// -------------------------------------------------------------------------

	/** Master audio volume multiplier (0.0 to 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.0f;

	/** Mouse look sensitivity multiplier (0.2 to 3.0, default 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0.2", ClampMax = "3.0"))
	float MouseSensitivity = 1.0f;

	/** Developer Mode: unlocks all 7 weapons and grants infinite Gold */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings")
	bool bDeveloperMode = false;

	/** Sets master volume (0.0..1.0) and applies it to the audio device */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetMasterVolume(float NewVolume, UWorld* WorldContext = nullptr);

	/** Sets mouse look sensitivity multiplier (0.2..3.0) */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetMouseSensitivity(float NewSensitivity);

	/** Toggles Developer Mode (all 7 weapons unlocked + infinite gold) */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetDeveloperMode(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Settings")
	bool IsDeveloperModeEnabled() const { return bDeveloperMode; }

	// -------------------------------------------------------------------------
	// 24-Hour Day/Night Clock & Farm Sun Control
	//   - 06:00 -> 24:00 (18 in-game hrs) = 5 real-time minutes (300s) -> Farm Phase
	//   - 00:00 -> 06:00 (6 in-game hrs)  = 3 real-time minutes (180s) -> Combat/Zombie Phase
	// -------------------------------------------------------------------------

	/** Current 24-hour clock time in hours [0.0, 24.0). Starts at 06:00 (6.0f). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time")
	float CurrentTimeOfDay = 6.0f;

	/** Current day counter (starts at Day 1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time")
	int32 CurrentDay = 1;

	/** Advances the 24-hour clock by DeltaTime seconds and rotates ONLY the Farm sun (never Combat lighting). */
	void TickDayNightClock(UWorld* World, float DeltaTime);

	/** Advances the clock by the equivalent of 1 real-time minute (Developer Mode [+] hotkey). */
	UFUNCTION(BlueprintCallable, Category = "Time")
	void AdvanceTimeByRealMinute(UWorld* WorldContext = nullptr);

	/** Returns true if the clock is currently in the Night/Zombie window [00:00, 06:00). */
	UFUNCTION(BlueprintPure, Category = "Time")
	bool IsNightWindow() const { return CurrentTimeOfDay < 6.0f; }

	/** Returns true if the player is currently allowed to Shift to TargetDimension based on the 24h clock and bed sleep. */
	UFUNCTION(BlueprintPure, Category = "Time")
	bool CanShiftToDimension(EGameDimension TargetDimension) const;

	/** Records whether the player is lying in the farmhouse bed (required to shift into Combat at night). */
	UFUNCTION(BlueprintCallable, Category = "Time")
	void SetPlayerSleepingInBed(bool bSleeping) { bIsSleepingInBed = bSleeping; }

	/** Returns true while the player is lying in the farmhouse bed. */
	UFUNCTION(BlueprintPure, Category = "Time")
	bool IsPlayerSleepingInBed() const { return bIsSleepingInBed; }

	/** Shows a centered on-screen notification via the HUD, falling back to on-screen debug text. */
	void ShowCenterMessage(const FString& Message, float DurationSeconds = 2.0f, FLinearColor TextColor = FLinearColor(1.0f, 0.85f, 0.35f, 1.0f)) const;

	/** Updates the Farm DirectionalLight rotation, intensity, and color to match CurrentTimeOfDay (never touches Combat). */
	void UpdateFarmSunForTimeOfDay(UWorld* World) const;

	/** Returns formatted 24-hour string like "06:00" or "23:45". */
	FString GetFormattedTime24H() const;

	/** Returns formatted 12-hour string like "6:00 AM" or "12:00 AM". */
	FString GetFormattedTime12H() const;

private:
	/** Set of unlocked weapon slot indices (Slot 0 = M1911 Classic is unlocked by default). */
	TSet<int32> UnlockedWeaponSlots;
	/** Called once the streamed farm level becomes visible. */
	UFUNCTION()
	void HandleFarmLevelShown();

	/** Streaming handle for the loaded farm level instance. */
	TWeakObjectPtr<class ULevelStreamingDynamic> FarmLevelStreaming;

	/** Timestamp in world seconds of the last successful shift. */
	float LastShiftTime;

	/** True while the player is lying in the farmhouse bed. */
	bool bIsSleepingInBed = false;

	/** Reference to the spawned prototype platform so we don't recreate it every shift. */
	mutable TWeakObjectPtr<AStaticMeshActor> PrototypeFarmPlatform;
};
