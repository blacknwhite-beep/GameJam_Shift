// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InterWorldBridge.generated.h"

class UTarkovInventoryComponent;
struct FInventoryGridItem;

/**
 * Economy Bridge Library connecting harvested crops in the Farming world
 * to the player's Tarkov-style grid inventory in the Combat world.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UInterWorldBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Deposits a harvested crop/item directly into the target actor's UTarkovInventoryComponent.
	 * Automatically stacks into existing slots where possible or finds free grid space (1x1).
	 * Supports:
	 *   - "Item_Ammo_Pistol" / "Ammo_Pistol_9mm" -> 9mm Pistol Ammo (stacks up to 50)
	 *   - "Item_Ammo_Shotgun" / "Ammo_Shotgun_12G" -> 12G Shotgun Shells (stacks up to 20)
	 *   - "Item_Ammo_Rifle" / "Ammo_Rifle_556" -> 5.56mm Rifle Ammo (stacks up to 60)
	 *   - "Item_Medkit" / "Medkit" -> Medkit consumable
	 * 
	 * @param Player The player actor receiving the harvest (must have UTarkovInventoryComponent).
	 * @param ItemID Identifier of the harvested item.
	 * @param Quantity Number of units yielded.
	 * @return True if all or part of the harvest was successfully deposited.
	 */
	UFUNCTION(BlueprintCallable, Category = "InterWorld|Economy")
	static bool DepositHarvestToInventory(AActor* Player, FName ItemID, int32 Quantity);

	/**
	 * Consumes a specific quantity of an item from the player's Tarkov inventory.
	 * Decrements stack counts and removes empty item tiles from the grid.
	 *
	 * @param Player The player actor.
	 * @param ItemID Identifier or name of the item to consume.
	 * @param Quantity Amount to consume.
	 * @return True if the requested quantity was available and consumed.
	 */
	UFUNCTION(BlueprintCallable, Category = "InterWorld|Economy")
	static bool ConsumeItemFromInventory(AActor* Player, FName ItemID, int32 Quantity = 1);

	/**
	 * Calculates the total count of an item type currently stored in the player's backpack.
	 *
	 * @param Player The player actor.
	 * @param ItemID Identifier or name of the item.
	 * @return Total quantity stored across all grid stacks.
	 */
	UFUNCTION(BlueprintPure, Category = "InterWorld|Economy")
	static int32 GetTotalItemCountInInventory(AActor* Player, FName ItemID);

	/**
	 * Checks whether the player has at least the required quantity of an item.
	 *
	 * @param Player The player actor.
	 * @param ItemID Identifier or name of the item.
	 * @param RequiredQuantity Minimum count required.
	 * @return True if inventory contains at least RequiredQuantity.
	 */
	UFUNCTION(BlueprintPure, Category = "InterWorld|Economy")
	static bool HasItemInInventory(AActor* Player, FName ItemID, int32 RequiredQuantity = 1);

	/**
	 * Uses a Medkit from the inventory to heal the player character.
	 *
	 * @param Player The player actor.
	 * @param HealthToRestore Amount of HP restored (default 50.0).
	 * @return True if a medkit was consumed and health was restored.
	 */
	UFUNCTION(BlueprintCallable, Category = "InterWorld|Economy")
	static bool UseMedkitFromInventory(AActor* Player, float HealthToRestore = 50.0f);

	/**
	 * Helper function resolving ItemID to canonical display name, max stack size, and grid dimensions.
	 */
	static void ResolveItemMetadata(FName ItemID, FString& OutDisplayName, int32& OutMaxStack, int32& OutWidth, int32& OutHeight);
};
