// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GoldHUDWidget.generated.h"

class UTextBlock;

/**
 * Persistent top-right HUD widget displaying the player's current Gold balance
 * and flashing a "+100" reward indicator whenever zombies are killed.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UGoldHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleGoldChanged(int32 NewGold, int32 Delta);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldAmountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldDeltaText;

private:
	float DeltaDisplayRemaining = 0.0f;
};
