// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartMenuWidget.generated.h"

class UImage;
class UButton;
class UTextBlock;
class UBorder;
class UTextureRenderTarget2D;
class AActor;

/**
 * Horizontally Split Dual-Dimension Start Menu Widget for GameJam_Shift_2026.
 * - Top Half: Live 3D view of the sunlit Farm Valley (Barn, Silo, Crops, Pond, Farm Plots) at Z = -20,000.
 * - Bottom Half: Live 3D view of the Zombie Quarantine Compound (Zombies roaming in Lvl_Combat) at Z = 0.
 * - Center Rift Divider: Glowing dimensional rift bar with game title, controls summary, and START / QUIT buttons.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API UStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Dismisses the Start Menu, cleans up the preview cameras, and begins gameplay */
	UFUNCTION()
	void OnStartGameClicked();

	/** Quits the game / exits PIE */
	UFUNCTION()
	void OnQuitGameClicked();

protected:
	void SpawnSplitScreenCameras();
	void DestroySplitScreenCameras();
	void EnsurePreviewCropsInFarm();

	UPROPERTY(Transient)
	TObjectPtr<UImage> TopFarmViewImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BottomCombatViewImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> FarmRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> CombatRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FarmCaptureActor;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CombatCaptureActor;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;

	float MenuElapsedSeconds = 0.0f;
	bool bSeededPreviewCrops = false;
};
