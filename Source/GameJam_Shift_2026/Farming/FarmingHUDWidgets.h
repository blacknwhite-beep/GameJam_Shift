// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FarmingTypes.h"
#include "FarmingHUDWidgets.generated.h"

class AFarmPlotBase;
class UFarmingComponent;
class UCanvasPanel;
class UOverlay;
class UBorder;
class UHorizontalBox;
class UVerticalBox;
class UTextBlock;
class USizeBox;

/**
 * WBP_CropPrompt native base class:
 * Displays contextual status, growth progress, water state, and interaction prompts
 * when the player aims at an AFarmPlotBase in the Farming dimension.
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEJAM_SHIFT_2026_API UCropPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates the prompt display based on the currently targeted plot and active tool */
	UFUNCTION(BlueprintCallable, Category = "Farming|UI")
	void UpdateFromPlot(AFarmPlotBase* Plot, EFarmTool ActiveTool, FName SelectedSeedID);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY()
	TObjectPtr<UBorder> PromptContainer;

	UPROPERTY()
	TObjectPtr<UTextBlock> PlotHeaderText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PlotStageText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ActionHintText;
};

/**
 * WBP_FarmingHUD native base class:
 * Full-screen Farming overlay containing:
 *   - 4-Slot Toolbelt Hotbar ([1] Hands, [2] Hoe, [3] Watering Can, [4] Harvester)
 *   - Seed Pouch Counter & Yield Info ([T] Cycle Seed)
 *   - Dynamic Center Reticle & Crop Hover Prompt (WBP_CropPrompt)
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEJAM_SHIFT_2026_API UFarmingHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Refreshes the HUD state from the owning player's UFarmingComponent */
	UFUNCTION(BlueprintCallable, Category = "Farming|UI")
	void RefreshHUD();

	/** Embedded crop hover prompt widget */
	UPROPERTY(BlueprintReadOnly, Category = "Farming|UI")
	TObjectPtr<UCropPromptWidget> CropPromptWidget;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Center reticle dot that changes color when aiming at a farm plot */
	UPROPERTY()
	TObjectPtr<UBorder> CenterReticle;

	/** Tool slot borders (0: Hands, 1: Hoe, 2: Watering Can, 3: Harvester) */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> ToolSlotBorders;

	/** Tool slot title labels */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ToolSlotLabels;

	/** Seed pouch active seed name text */
	UPROPERTY()
	TObjectPtr<UTextBlock> ActiveSeedText;

	/** Seed pouch yield description text */
	UPROPERTY()
	TObjectPtr<UTextBlock> SeedYieldText;

private:
	/** Helper to format friendly seed name and yield description */
	static void DescribeSeed(FName SeedID, FString& OutDisplayName, FString& OutYieldDesc);
};
