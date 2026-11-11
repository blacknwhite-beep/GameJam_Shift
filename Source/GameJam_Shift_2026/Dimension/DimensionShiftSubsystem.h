// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DimensionTypes.h"
#include "DimensionShiftSubsystem.generated.h"

class ACharacter;
class AStaticMeshActor;

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

private:
	/** Timestamp in world seconds of the last successful shift. */
	float LastShiftTime;

	/** Reference to the spawned prototype platform so we don't recreate it every shift. */
	mutable TWeakObjectPtr<AStaticMeshActor> PrototypeFarmPlatform;
};
