// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeployablePlacementComponent.generated.h"

class AActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Generic click-to-place flow for backpack deployables such as the Landmine. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAMEJAM_SHIFT_2026_API UDeployablePlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeployablePlacementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts placement for one inventory item and its spawn class. */
	void StartPlacement(FGuid ItemId, TSubclassOf<AActor> PlaceableClass);

	/** Spawns the deployable and consumes the inventory item on success. */
	bool ConfirmPlacement();

	/** Cancels placement without consuming the item. */
	void CancelPlacement();

	bool IsPlacing() const { return bIsPlacing; }
	bool IsPlacementValid() const { return bPlacementValid; }

protected:
	void UpdatePreviewTransform();

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PreviewMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Placement")
	TObjectPtr<UStaticMesh> PreviewCylinderMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Placement")
	TObjectPtr<UMaterialInterface> PreviewMaterial = nullptr;

	FGuid PendingItemId;
	TSubclassOf<AActor> PendingPlaceableClass;
	bool bIsPlacing = false;
	bool bPlacementValid = false;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "100.0", ClampMax = "2000.0", Units = "cm"))
	float MaxPlacementDistance = 600.0f;
};
