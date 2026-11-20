// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FarmBedActor.generated.h"

class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Interactive bed inside the Farmhouse. Press [E] while looking at it at night (12:00 AM - 06:00 AM)
 * to lie down Minecraft-style; sleeping in it is the only way to unlock the Combat shift.
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API AFarmBedActor : public AActor
{
	GENERATED_BODY()

public:
	AFarmBedActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Visibility-blocking trigger so camera line traces reliably hit the bed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> PromptText;

	/** World position on top of the mattress where the sleeping player lies */
	UFUNCTION(BlueprintPure, Category = "Farm Bed")
	FVector GetSleepAnchorLocation() const;

	/** World position standing on the floor beside the bed, clear of the mattress */
	UFUNCTION(BlueprintPure, Category = "Farm Bed")
	FVector GetWakeUpLocation() const;

	/** World yaw from the pillow toward the footboard */
	UFUNCTION(BlueprintPure, Category = "Farm Bed")
	FRotator GetSleepFacingRotation() const;

protected:
	virtual void BeginPlay() override;

private:
	void AddPiece(const FName Name, const FVector& Location, const FVector& Size, const FLinearColor& Color);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BedPieces;

	TArray<FLinearColor> BedPieceColors;
};
