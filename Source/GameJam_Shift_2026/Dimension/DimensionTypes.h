// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DimensionTypes.generated.h"

/**
 * Represents the active dimension / world state.
 */
UENUM(BlueprintType)
enum class EGameDimension : uint8
{
	Combat  UMETA(DisplayName = "Combat / Real World"),
	Farming UMETA(DisplayName = "Farming / Dream World")
};

/**
 * Delegate broadcasted when the player shifts between dimensions.
 * @param NewDimension The dimension transitioned into.
 * @param TargetLocation The player's new world location.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDimensionShifted, EGameDimension, NewDimension, FVector, TargetLocation);
