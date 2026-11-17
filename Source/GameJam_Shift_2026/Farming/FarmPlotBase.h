// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FarmingTypes.h"
#include "FarmPlotBase.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UBoxComponent;
class UMaterialInterface;
class USoundBase;
class UNiagaraSystem;

/**
 * Base actor representing a cultivable soil plot in the Farming dimension.
 * Manages crop life cycle: Empty/Tilled -> Planted -> Sprout -> Growing -> Mature -> Harvest.
 * Fully exposed to Blueprints for visual/audio effects, mesh swapping, and designer tuning.
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API AFarmPlotBase : public AActor
{
	GENERATED_BODY()

public:
	AFarmPlotBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// -------------------------------------------------------------------------
	// Components
	// -------------------------------------------------------------------------

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Static mesh for the soil / dirt mound */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SoilMesh;

	/** Static mesh for the growing crop plant (fallback cylinder when Sprout/Mature meshes are unassigned) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CropMesh;

	/** Optional sprout mesh shown during Planted / Sprout / Growing stages */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh_Sprout;

	/** Optional mature crop mesh shown when crop reaches Mature stage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh_Mature;

	/** Interaction box volume to facilitate line-of-sight raycasts and player proximity */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// -------------------------------------------------------------------------
	// State & Configuration
	// -------------------------------------------------------------------------

	/** Current growth stage of this plot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming|State")
	ECropStage CurrentStage;

	/** True if the soil is watered and active growth is proceeding */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming|State")
	bool bIsWatered;

	/** Duration in seconds required for a watered crop to advance one stage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Configuration", meta = (ClampMin = "1.0"))
	float StageDuration;

	/** Identifier of the seed currently planted */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|State")
	FName PlantedSeedID;

	/** Data configured for the harvest yield of the current crop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Configuration")
	FCropHarvestData HarvestYield;

	/** If true, PlantSeed preserves HarvestYield & StageDuration configured on this Crop Blueprint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Configuration")
	bool bPreserveConfiguredYield = false;

	/** Dry tilled soil material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Visuals")
	TObjectPtr<UMaterialInterface> DrySoilMaterial;

	/** Darker wet soil material applied when watered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Visuals")
	TObjectPtr<UMaterialInterface> WetSoilMaterial;

	/** Sound played when harvesting this crop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Audio & VFX")
	TObjectPtr<USoundBase> HarvestPopSound;

	/** Optional Niagara system spawned when crop becomes Mature */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Audio & VFX")
	TObjectPtr<UNiagaraSystem> MatureSparkleVFX;

	/** Optional Niagara system spawned when plot is watered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Audio & VFX")
	TObjectPtr<UNiagaraSystem> WaterSplashVFX;

	/** Floating text format shown on harvest (e.g. "+16 Shotgun Shells") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Audio & VFX")
	FString HarvestFloatingText;

	// -------------------------------------------------------------------------
	// Player Interaction API
	// -------------------------------------------------------------------------

	/**
	 * Tills or clears an overgrown or harvested plot, preparing it for planting.
	 * @return True if plot was tilled.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Farming|Action")
	bool TillPlot();

	/**
	 * Plants a seed on this plot. Only succeeds if the plot is Empty.
	 * @param SeedID Identifier of the seed being planted.
	 * @param InStageDuration Optional override for growth stage duration.
	 * @return True if successfully planted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Farming|Action")
	bool PlantSeed(FName SeedID, float InStageDuration = 10.0f);

	/**
	 * Waters the plot. Required for crops to grow to the next stage.
	 * @return True if successfully watered, false if already watered or plot is empty/mature.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Farming|Action")
	bool WaterPlot();

	/**
	 * Advances the crop to the next growth stage immediately (can be called directly or via timer).
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Farming|Action")
	void AdvanceGrowthStage();

	/**
	 * Harvests the mature crop, returning yield items and resetting the plot to Empty.
	 * @param OutItemID ID of the harvested item (e.g. Ammo_Shotgun_12G).
	 * @param OutQuantity Amount of the harvested item yielded.
	 * @return True if harvest succeeded.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Farming|Action")
	bool Harvest(FName& OutItemID, int32& OutQuantity);

	/** Returns true if this plot is empty and ready for seeds */
	UFUNCTION(BlueprintPure, Category = "Farming|Query")
	bool CanPlant() const { return CurrentStage == ECropStage::Empty; }

	/** Returns true if this plot has a growing crop that needs water */
	UFUNCTION(BlueprintPure, Category = "Farming|Query")
	bool CanWater() const { return !bIsWatered && CurrentStage != ECropStage::Empty && CurrentStage != ECropStage::Mature && CurrentStage != ECropStage::Withered; }

	/** Returns true if the crop is fully mature and ready to harvest */
	UFUNCTION(BlueprintPure, Category = "Farming|Query")
	bool CanHarvest() const { return CurrentStage == ECropStage::Mature; }

	/** Returns progress from 0.0 to 1.0 of the current growth stage */
	UFUNCTION(BlueprintPure, Category = "Farming|Query")
	float GetStageProgress() const;

	/** Returns the currently active crop stage */
	UFUNCTION(BlueprintPure, Category = "Farming|Query")
	ECropStage GetCurrentStage() const { return CurrentStage; }

	/** Returns whether the soil is currently watered */
	UFUNCTION(BlueprintPure, Category = "Farming|Query")
	bool IsWatered() const { return bIsWatered; }

	// -------------------------------------------------------------------------
	// Blueprint Implementable Events (Friend's Domain)
	// -------------------------------------------------------------------------

	/** Fired when the crop advances to a new growth stage (use to swap meshes or spawn particles) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Farming|Events")
	void OnStageChanged(ECropStage NewStage);

	/** Fired when water is applied (use to play splash particles or darken dirt material) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Farming|Events")
	void OnWatered();

	/** Fired when the soil dries out after a growth cycle */
	UFUNCTION(BlueprintImplementableEvent, Category = "Farming|Events")
	void OnSoilDried();

	/** Fired when a seed is planted */
	UFUNCTION(BlueprintImplementableEvent, Category = "Farming|Events")
	void OnPlanted(FName SeedID);

	/** Fired when the plot is tilled with a hoe */
	UFUNCTION(BlueprintImplementableEvent, Category = "Farming|Events")
	void OnTilled();

	/** Fired when the crop is harvested (use to play audio, pop animations, or floating text) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Farming|Events")
	void OnHarvested(FName ItemID, int32 Quantity);

protected:
	/** Updates internal crop mesh scale and visibility for stage prototyping */
	virtual void UpdateCropMeshForStage(ECropStage Stage);

	/** Spawns temporary 3D world-space floating text above the plot */
	void SpawnFloatingHarvestText(const FString& Text, const FColor& Color);

	/** Timer handle managing the current growth cycle */
	FTimerHandle GrowthTimerHandle;

	/** Timestamp when current stage timer was started */
	float StageStartTime;
};
