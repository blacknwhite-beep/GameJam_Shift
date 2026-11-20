// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FarmHouseActor.generated.h"

class AFarmBedActor;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Cozy farmhouse built from primitive cubes beside the Barn in Lvl_Farm.
 * Has a wide walkable doorway, a warm interior light, and a bed inside that spawns with it.
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API AFarmHouseActor : public AActor
{
	GENERATED_BODY()

public:
	AFarmHouseActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> InteriorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> SignText;

	/** The bed spawned inside this house */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<AFarmBedActor> Bed;

protected:
	virtual void BeginPlay() override;

private:
	void AddPiece(const FName Name, const FVector& Location, const FVector& Size, const FRotator& Rotation, const FLinearColor& Color);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> HousePieces;

	TArray<FLinearColor> HousePieceColors;
};
