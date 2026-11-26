// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BarnShopWidget.generated.h"

class UTextBlock;
class UButton;
class UVerticalBox;

/**
 * Barn Seed, Land, Guns & Traps Shop UI opened when interacting with the Barn in Lvl_Farm.
 * Features three tabs:
 *  - Tab 1 (SEEDS & FARM PLOTS): Sells the 4 crop seed types and placeable Farm Plots.
 *  - Tab 2 (GUNS ARSENAL): Sells the 6 purchasable Gun Pack firearms at increasing prices.
 *  - Tab 3 (TRAPS): Sells placeable Landmines for Combat-world zombie defense.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UBarnShopWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Tab switching
	UFUNCTION() void OnTabSeedsClicked();
	UFUNCTION() void OnTabGunsClicked();
	UFUNCTION() void OnTabTrapsClicked();
	void SetActiveTab(int32 TabIndex);

	// Seeds & Land tab actions
	UFUNCTION() void OnBuyShotgunSeeds();
	UFUNCTION() void OnBuyRifleSeeds();
	UFUNCTION() void OnBuyPistolSeeds();
	UFUNCTION() void OnBuyMedkitSeeds();
	UFUNCTION() void OnBuyFarmPlot();

	// Traps tab actions
	UFUNCTION() void OnBuyLandmine();

	// Guns Arsenal tab actions (Slots 1..6; Slot 0 is M1911 Classic, owned at start)
	UFUNCTION() void OnBuyGunSlot1();
	UFUNCTION() void OnBuyGunSlot2();
	UFUNCTION() void OnBuyGunSlot3();
	UFUNCTION() void OnBuyGunSlot4();
	UFUNCTION() void OnBuyGunSlot5();
	UFUNCTION() void OnBuyGunSlot6();

	UFUNCTION() void OnCloseClicked();

	void TryBuySeed(FName SeedID, const FString& DisplayName, int32 Price);
	void TryBuyOrEquipGun(int32 WeaponSlotIndex, const FString& DisplayName, int32 Price);
	void RefreshGoldText();
	void RefreshGunsTabUI();
	void SetStatus(const FString& Message, const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TabSeedsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TabGunsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TabTrapsButton;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> SeedsTabBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> GunsTabBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> TrapsTabBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> GunBuyButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> GunBuyButtonLabels;
};
