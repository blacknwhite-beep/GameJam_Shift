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
	Empty     UMETA(DisplayName = "Empty / Tilled"),
	Planted   UMETA(DisplayName = "Planted / Seed"),
	Sprout    UMETA(DisplayName = "Sprout"),
	Growing   UMETA(DisplayName = "Growing"),
	Mature    UMETA(DisplayName = "Mature / Harvestable"),
	Withered  UMETA(DisplayName = "Withered")
};

/**
 * Data describing the yield of a harvested crop.
 */
USTRUCT(BlueprintType)
struct FCropHarvestData
{
	GENERATED_BODY()

	/** Item ID deposited into player inventory (e.g. Ammo_Shotgun_12G, Ammo_Rifle_556) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
	FName ItemID = NAME_None;

	/** Number of items yielded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	/** Optional bonus seed returned upon harvest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
	FName BonusSeedID = NAME_None;

	/** Quantity of bonus seeds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest", meta = (ClampMin = "0"))
	int32 BonusSeedQuantity = 0;
};
