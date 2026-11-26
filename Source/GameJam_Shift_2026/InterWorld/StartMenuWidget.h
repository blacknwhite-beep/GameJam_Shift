// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartMenuWidget.generated.h"

class UImage;
class UButton;
class UTextBlock;
class UBorder;
class UBackgroundBlur;
class USlider;
class UVerticalBox;
class UTextureRenderTarget2D;
class AActor;

/**
 * Horizontally Split Dual-Dimension Start Menu Widget for GameJam_Shift_2026.
 * - Top Half: Live 3D view of the sunlit Farm Valley (Barn, Silo, Crops, Pond, Farm Plots) at Z = -20,000.
 * - Bottom Half: Live 3D view of the Zombie Quarantine Compound (Zombies roaming in Lvl_Combat) at Z = 0.
 * - Center Rift Divider: Glowing dimensional rift bar with PLAY and SETTINGS tabs:
 *     - PLAY Tab: Game title, controls summary, START GAME / QUIT buttons.
 *     - SETTINGS Tab: Master Volume slider, Mouse Sensitivity slider, and Developer Mode toggle
 *       (unlocks all 7 weapons + infinite Gold).
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

	/** Switches center card to the PLAY tab */
	UFUNCTION()
	void OnTabPlayClicked();

	/** Switches center card to the SETTINGS tab */
	UFUNCTION()
	void OnTabSettingsClicked();

	/** Master Volume slider handler (0.0 -> 1.0) */
	UFUNCTION()
	void OnVolumeSliderChanged(float Value);

	/** Mouse Sensitivity slider handler (0.2 -> 3.0) */
	UFUNCTION()
	void OnSensitivitySliderChanged(float Value);

	/** Toggles Developer Mode (all 7 weapons unlocked + infinite Gold) */
	UFUNCTION()
	void OnToggleDeveloperModeClicked();

	/** Opens the menu directly on the Settings tab (used when pressing [P] or [O] in-game) */
	void OpenOnSettingsTab();

	/** Configures this same settings UI as the in-game pause menu. */
	void OpenPauseMenu();

	/** True while this widget is acting as the paused-game overlay. */
	bool IsPauseMenuMode() const { return bPauseMenuMode; }

protected:
	/** Resumes gameplay and closes the pause overlay. */
	void ClosePauseMenu();

	void SetActiveTab(int32 TabIndex);
	void RefreshSettingsUI();
	void SpawnSplitScreenCameras();
	void DestroySplitScreenCameras();
	void EnsurePreviewCropsInFarm();

	UPROPERTY(Transient)
	TObjectPtr<UImage> TopFarmViewImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BottomCombatViewImage;

	/** Full-screen live-game blur and dark shade used only by pause mode. */
	UPROPERTY(Transient)
	TObjectPtr<UBackgroundBlur> PauseBackgroundBlur;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PauseShade;

	/** Start-menu preview decorations hidden while the pause overlay is active. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> FarmBadge;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CombatBadge;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RiftOuterGlow;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RiftCoreLine;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> FarmRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> CombatRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FarmCaptureActor;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CombatCaptureActor;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TabPlayButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TabSettingsButton;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PlayTabBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> SettingsTabBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StartButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuitButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SettingsBackLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SettingsResumeLabel;

	UPROPERTY(Transient)
	TObjectPtr<USlider> VolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VolumeValueText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SensitivitySlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SensitivityValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DevModeToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DevModeToggleLabel;

	float MenuElapsedSeconds = 0.0f;
	bool bSeededPreviewCrops = false;
	bool bPauseMenuMode = false;
	int32 ActiveTabIndex = 0;
};
