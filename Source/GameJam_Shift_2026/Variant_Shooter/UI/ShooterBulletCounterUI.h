// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterBulletCounterUI.generated.h"

class UImage;
class UTexture2D;

/**
 *  Simple bullet counter UI widget for a first person shooter game.
 *  Replaces the template orange damage outline with a full-screen crimson blood & gore splatter frame
 *  that pulses on every zombie hit and lingers stronger at low HP.
 */
UCLASS(abstract)
class GAMEJAM_SHIFT_2026_API UShooterBulletCounterUI : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Allows Blueprint to update sub-widgets with the new bullet count */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "UpdateBulletCounter"))
	void BP_UpdateBulletCounter(int32 MagazineSize, int32 BulletCount);

	/** Allows Blueprint to update sub-widgets with the new life total and play a damage effect on the HUD */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "Damaged"))
	void BP_Damaged(float LifePercent);

	/** Triggers the crimson blood & gore screen splatter frame on player damage */
	void TriggerBloodDamageEffect(float LifePercent);

protected:
	/** Replaces any default template orange hit border/vignette image with T_BloodDamageOverlay */
	void SetupBloodGoreOverlay();

	UPROPERTY(Transient)
	TObjectPtr<UImage> BloodGoreOverlayImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BloodOverlayTexture;

	float BloodPulseOpacity = 0.0f;
	float LastKnownLifePercent = 1.0f;
};
