// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GoldHUDWidget.generated.h"

class UTextBlock;
class UBorder;

/**
 * Persistent HUD widget displaying:
 *  - Top-Right: Gold balance & +100 kill delta popup.
 *  - Middle-Right: 24-Hour Day/Night Clock, Day counter, active phase (FARM / ZOMBIE NIGHT),
 *    and Shift Unlock status indicator (plus [+] Dev skip hint when Developer Mode is active).
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

	void RefreshClockDisplay();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldAmountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldDeltaText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ClockCardBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockTimeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockPhaseText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockShiftStatusText;

private:
	float DeltaDisplayRemaining = 0.0f;
};
