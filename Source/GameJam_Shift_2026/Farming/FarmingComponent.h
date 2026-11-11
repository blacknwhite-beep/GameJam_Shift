// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FarmingTypes.h"
#include "FarmingComponent.generated.h"

/**
 * Player component managing equipped farming tools, seed selection, and plot interactions.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEJAM_SHIFT_2026_API UFarmingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFarmingComponent();

	virtual void BeginPlay() override;

	/** Currently equipped farming tool */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Tools")
	EFarmTool ActiveTool;

	/** Currently selected seed type / crop ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Seeds")
	FName SelectedSeedID;

	/** Max reach distance when interacting with soil plots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Interaction", meta = (ClampMin = "50.0", ClampMax = "1000.0"))
	float InteractionDistance;

	/** Uses the currently equipped tool (interacts with the targeted plot) */
	UFUNCTION(BlueprintCallable, Category = "Farming|Tools")
	void UseActiveTool();

	/** Cycles through available tools */
	UFUNCTION(BlueprintCallable, Category = "Farming|Tools")
	void CycleTool(bool bNext = true);

	/** Equips a specific tool */
	UFUNCTION(BlueprintCallable, Category = "Farming|Tools")
	void SetActiveTool(EFarmTool NewTool);

	/** Gets the active tool */
	UFUNCTION(BlueprintPure, Category = "Farming|Tools")
	EFarmTool GetActiveTool() const { return ActiveTool; }
};
