// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FarmingTypes.h"
#include "FarmingComponent.generated.h"

class AFarmPlotBase;
class UFarmingHUDWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFarmToolChanged, EFarmTool, NewTool);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCropHarvested, FName, ItemID, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSeedPlanted, AFarmPlotBase*, Plot, FName, SeedID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlotWatered, AFarmPlotBase*, Plot);

/**
 * Player component managing equipped farming tools, seed selection, and plot interactions.
 * Raycasts forward from the player's view point to interact with AFarmPlotBase actors.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEJAM_SHIFT_2026_API UFarmingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFarmingComponent();

	virtual void BeginPlay() override;

	// -------------------------------------------------------------------------
	// Tool & Seed Properties
	// -------------------------------------------------------------------------

	/** Currently equipped farming tool */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Tools")
	EFarmTool ActiveTool;

	/** Currently selected seed type / crop ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Seeds")
	FName SelectedSeedID;

	/** Available seeds that the player can cycle through */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Seeds")
	TArray<FName> AvailableSeeds;

	/** Index of currently selected seed in AvailableSeeds */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Seeds")
	int32 SelectedSeedIndex;

	/** Max reach distance when interacting with soil plots (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Interaction", meta = (ClampMin = "50.0", ClampMax = "2000.0"))
	float InteractionDistance;

	/** Optional Widget Blueprint override for WBP_FarmingHUD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|UI")
	TSubclassOf<UFarmingHUDWidget> FarmingHUDClass;

	/** Active instance of the Farming HUD widget */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Farming|UI")
	TObjectPtr<UFarmingHUDWidget> ActiveFarmingHUD;

	/** Shows or hides the Farming HUD widget (creating it on demand) */
	UFUNCTION(BlueprintCallable, Category = "Farming|UI")
	void SetFarmingHUDVisible(bool bVisible);

	// -------------------------------------------------------------------------
	// Core Interaction API
	// -------------------------------------------------------------------------

	/** Uses the currently equipped tool (raycasts forward and interacts with the targeted plot) */
	UFUNCTION(BlueprintCallable, Category = "Farming|Tools")
	void UseActiveTool();

	/** Cycles through available tools */
	UFUNCTION(BlueprintCallable, Category = "Farming|Tools")
	void CycleTool(bool bNext = true);

	/** Equips a specific tool */
	UFUNCTION(BlueprintCallable, Category = "Farming|Tools")
	void SetActiveTool(EFarmTool NewTool);

	/** Cycles through available seed types */
	UFUNCTION(BlueprintCallable, Category = "Farming|Seeds")
	void CycleSeed(bool bNext = true);

	/** Selects a specific seed by name */
	UFUNCTION(BlueprintCallable, Category = "Farming|Seeds")
	void SetSelectedSeed(FName NewSeedID);

	/** Gets the active tool */
	UFUNCTION(BlueprintPure, Category = "Farming|Tools")
	EFarmTool GetActiveTool() const { return ActiveTool; }

	/** Gets the currently selected seed identifier */
	UFUNCTION(BlueprintPure, Category = "Farming|Seeds")
	FName GetSelectedSeedID() const { return SelectedSeedID; }

	/** Raycasts forward from the owner's viewpoint to detect the targeted farm plot */
	UFUNCTION(BlueprintCallable, Category = "Farming|Interaction")
	AFarmPlotBase* GetTargetedPlot() const;

	// -------------------------------------------------------------------------
	// Delegates / Blueprint Events
	// -------------------------------------------------------------------------

	/** Broadcast when the active tool is changed */
	UPROPERTY(BlueprintAssignable, Category = "Farming|Events")
	FOnFarmToolChanged OnToolChanged;

	/** Broadcast when a crop is harvested */
	UPROPERTY(BlueprintAssignable, Category = "Farming|Events")
	FOnCropHarvested OnCropHarvested;

	/** Broadcast when a seed is planted on a plot */
	UPROPERTY(BlueprintAssignable, Category = "Farming|Events")
	FOnSeedPlanted OnSeedPlanted;

	/** Broadcast when a plot is watered */
	UPROPERTY(BlueprintAssignable, Category = "Farming|Events")
	FOnPlotWatered OnPlotWatered;

protected:
	/** Internal execution of tool actions on a valid plot */
	void ExecuteToolOnPlot(AFarmPlotBase* Plot);
};
