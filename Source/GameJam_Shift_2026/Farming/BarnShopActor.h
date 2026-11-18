// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BarnShopActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UBarnShopWidget;

/**
 * Interactive Barn Shop counter placed at the Barn entrance in Lvl_Farm.
 * Lets the player buy Crop Seeds and Farm Plots using Gold earned in Combat.
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API ABarnShopActor : public AActor
{
	GENERATED_BODY()

public:
	ABarnShopActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CounterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> SignText;

	UFUNCTION(BlueprintCallable, Category = "Barn Shop")
	void OpenBarnShop(APlayerController* PC);

	UFUNCTION(BlueprintPure, Category = "Barn Shop")
	bool IsPlayerInRange() const { return bPlayerInRange; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(Transient)
	TObjectPtr<UBarnShopWidget> ActiveShopWidget;

	bool bPlayerInRange = false;
};
