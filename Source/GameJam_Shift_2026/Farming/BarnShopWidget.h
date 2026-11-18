// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BarnShopWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * Barn Seed & Farm Plot Shop UI opened when interacting with the Barn in Lvl_Farm.
 * Sells the 4 crop seed types (which enter the T rotation once purchased) and
 * placeable Farm Plots (deposited into the Tarkov Backpack).
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UBarnShopWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void OnBuyShotgunSeeds();
	UFUNCTION() void OnBuyRifleSeeds();
	UFUNCTION() void OnBuyPistolSeeds();
	UFUNCTION() void OnBuyMedkitSeeds();
	UFUNCTION() void OnBuyFarmPlot();
	UFUNCTION() void OnCloseClicked();

	void TryBuySeed(FName SeedID, const FString& DisplayName, int32 Price);
	void RefreshGoldText();
	void SetStatus(const FString& Message, const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
};
