// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TarkovInventoryComponent.generated.h"

class AShooterWeapon;
class UUserWidget;
class UTarkovEquippedSlotWidget;
class UTarkovHUDWidget;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryToggled, bool, bIsOpen);

/**
 * An item represented in the Tarkov grid inventory.
 * In a 120x120 pixel backpack with 20x20 pixel cells (6x6 grid),
 * a 1x1 tile item takes exactly 20x20 pixels.
 */
USTRUCT(BlueprintType)
struct FInventoryGridItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FGuid ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FString ItemName = TEXT("Pistol");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 GridX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 GridY = 0;

	/** Number of grid cells wide (each cell = 20 pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 Width = 1;

	/** Number of grid cells high (each cell = 20 pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 Height = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	bool bIsRotated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	TSubclassOf<AShooterWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 AmmoCount = 10;

	int32 GetCurrentAmmo() const { return AmmoCount; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	TObjectPtr<class UTexture2D> Icon;
};

/**
 * Tarkov-style Grid Inventory Component
 * Backpack dimensions: 120x120 pixels (6x6 cells @ 20x20 px/cell).
 * Gun dimensions: 20x20 pixels (1x1 cell).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEJAM_SHIFT_2026_API UTarkovInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UTarkovInventoryComponent();

	/** Number of columns in backpack grid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Grid")
	int32 GridColumns = 6;

	/** Number of rows in backpack grid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Grid")
	int32 GridRows = 6;

	/** Visual pixel size of each cell (20x20 pixels -> 6*20 = 120x120 px total backpack) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Grid")
	float CellPixelSize = 20.0f;

	/** Getter alias for CellPixelSize (20.0f) */
	UFUNCTION(BlueprintPure, Category="Inventory|Grid")
	float GetCellSize() const { return CellPixelSize; }

	/** Total pixel width of backpack (120 pixels) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Grid")
	float BackpackPixelWidth = 120.0f;

	/** Total pixel height of backpack (120 pixels) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Grid")
	float BackpackPixelHeight = 120.0f;

	/** Items currently placed in the backpack grid */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FInventoryGridItem> BackpackItems;

	/** Weapon item currently equipped in the active weapon slot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	FInventoryGridItem EquippedWeaponItem;

	/** True if a weapon is equipped in the active weapon slot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bHasEquippedWeapon = false;

	/** UI Widget class to spawn for Tarkov inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|UI")
	TSubclassOf<UUserWidget> InventoryWidgetClass;

	/** Audio effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Audio")
	TObjectPtr<USoundBase> SoundInventoryOpen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Audio")
	TObjectPtr<USoundBase> SoundInventoryClose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Audio")
	TObjectPtr<USoundBase> SoundItemPickup;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Audio")
	TObjectPtr<USoundBase> SoundItemDrop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Audio")
	TObjectPtr<USoundBase> SoundItemRotate;

	UPROPERTY(BlueprintAssignable, Category="Inventory|Events")
	FOnInventoryUpdated OnInventoryUpdated;

	UPROPERTY(BlueprintAssignable, Category="Inventory|Events")
	FOnInventoryToggled OnInventoryToggled;

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveInventoryWidget;

	bool bIsInventoryOpen = false;

public:
	/** Checks if an item of dimension (W x H) can fit at grid position (X, Y) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool CanPlaceItemAt(int32 X, int32 Y, int32 W, int32 H, FGuid IgnoreItemId = FGuid()) const;

	/** Alias for CanPlaceItemAt */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool CanPlaceAt(int32 X, int32 Y, int32 W, int32 H, FGuid IgnoreItemId = FGuid()) const { return CanPlaceItemAt(X, Y, W, H, IgnoreItemId); }

	/** Adds an item to the backpack at the specified cell */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool AddItemAt(UPARAM(ref) FInventoryGridItem& Item, int32 X, int32 Y);

	/** Moves an existing item to a new cell */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool MoveItem(FGuid ItemId, int32 NewX, int32 NewY);

	/** Rotates an item (swaps W and H) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RotateItem(FGuid ItemId);

	/** Removes an item from the backpack */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveItem(FGuid ItemId);

	/** Equips an item from the backpack into the active weapon slot */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool EquipItemFromBackpack(FGuid ItemId);

	/** Alias for EquipItemFromBackpack */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool EquipWeaponFromInventory(FGuid ItemId) { return EquipItemFromBackpack(ItemId); }

	/** Unequips the currently equipped weapon into the backpack at cell (TargetX, TargetY) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool UnequipWeaponToBackpack(int32 TargetX, int32 TargetY);

	/** Alias for UnequipWeaponToBackpack matching SPEC name */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool UnequipToBackpack(int32 TargetX = -1, int32 TargetY = -1) { return UnequipWeaponToBackpack(TargetX, TargetY); }

	/** Finds the first available cell in the backpack that can fit an item of (W, H) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool FindFreeSpace(int32 W, int32 H, int32& OutX, int32& OutY) const;

	/** Toggles inventory open/closed */
	/** Creates the bottom-right equipped weapon slot shown during play */
	void CreateHUD();

	UPROPERTY()
	TObjectPtr<UTarkovHUDWidget> HUDRoot;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool IsInventoryOpen() const { return bIsInventoryOpen; }

	UFUNCTION(BlueprintCallable, Category="Inventory")
	const TArray<FInventoryGridItem>& GetBackpackItems() const { return BackpackItems; }

	UFUNCTION(BlueprintCallable, Category="Inventory")
	const FInventoryGridItem& GetEquippedWeaponItem() const { return EquippedWeaponItem; }

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool HasEquippedWeapon() const { return bHasEquippedWeapon; }
};
