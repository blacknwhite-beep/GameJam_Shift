// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FarmingTypes.generated.h"

/**
 * Active farm tool equipped by the player in the farming dimension.
 */
UENUM(BlueprintType)
enum class EFarmTool : uint8
{
	None         UMETA(DisplayName = "None / Hands"),
	Hoe          UMETA(DisplayName = "Hoe"),
	WateringCan  UMETA(DisplayName = "Watering Can"),
	Harvester    UMETA(DisplayName = "Harvester")
};

/**
 * Growth stage of a crop planted on a soil plot.
 */
UENUM(BlueprintType)
enum class ECropStage : uint8
{
	Planted   UMETA(DisplayName = "Planted / Seed"),
	Sprout    UMETA(DisplayName = "Sprout"),
	Growing   UMETA(DisplayName = "Growing"),
	Mature    UMETA(DisplayName = "Mature / Harvestable"),
	Withered  UMETA(DisplayName = "Withered")
};
