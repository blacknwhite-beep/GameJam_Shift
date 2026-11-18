// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterWeaponWheelWidget.generated.h"

class AShooterCharacter;
class UCanvasPanel;
class UBorder;
class UTextBlock;

/**
 * 7-Slot Radial Weapon Wheel UI Widget for the Combat Dimension.
 * - Opened while holding [TAB] in Combat.
 * - Moving the mouse radially around the center highlights slots 1..7.
 * - Pressing [1]..[7] while holding [TAB] immediately selects and equips that weapon.
 * - Releasing [TAB] equips the currently highlighted weapon and closes the wheel.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UShooterWeaponWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Opens the radial wheel and centers the mouse cursor */
	void OpenWheel(AShooterCharacter* InOwnerCharacter);

	/** Closes the radial wheel and equips the highlighted weapon if valid */
	void CloseWheelAndCommit();

	/** Directly selects a weapon slot (0..6 corresponding to keys 1..7) */
	void SelectSlotByIndex(int32 SlotIndex);

	/** Returns the currently highlighted slot index (0..6, or -1 if none) */
	int32 GetHoveredSlotIndex() const { return HoveredSlotIndex; }

	/** Refreshes the 7 slot cards with live weapon names, subtitles, and magazine/reserve ammo counts */
	void RefreshWheelSlots();

protected:
	UPROPERTY()
	TObjectPtr<AShooterCharacter> OwnerCharacter;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotCardBorders;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotKeyBadges;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotNameTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotSubtitleTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotAmmoTexts;

	UPROPERTY()
	TObjectPtr<UTextBlock> CenterWeaponTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CenterWeaponSubtitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CenterWeaponAmmoText;

	int32 HoveredSlotIndex = 0;

	/** Screen-space cursor position when the wheel was opened (used as the radial center) */
	FVector2D WheelCenterMousePos = FVector2D::ZeroVector;
	bool bHasCapturedCenter = false;
};
