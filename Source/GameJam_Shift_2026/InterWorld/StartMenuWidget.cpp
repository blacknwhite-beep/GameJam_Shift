// Copyright GameJam Shift 2026. All Rights Reserved.

#include "InterWorld/StartMenuWidget.h"
#include "DimensionShiftSubsystem.h"
#include "Farming/FarmPlotBase.h"
#include "ShooterPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/Slider.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

void UStartMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StartMenuRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// =========================================================================
	// 1. TOP HALF (0.0 -> 0.50 Y): Live Sunlit Farm Dimension Preview
	// =========================================================================
	TopFarmViewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TopFarmViewImage"));
	if (UCanvasPanelSlot* TopSlot = RootCanvas->AddChildToCanvas(TopFarmViewImage))
	{
		TopSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.50f));
		TopSlot->SetOffsets(FMargin(0.0f));
	}

	// Top-Left Farm Dimension Badge
	UBorder* FarmBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FarmBadge"));
	FarmBadge->SetBrushColor(FLinearColor(0.04f, 0.14f, 0.07f, 0.88f));
	FarmBadge->SetPadding(FMargin(16.0f, 8.0f));
	if (UCanvasPanelSlot* FBSlot = RootCanvas->AddChildToCanvas(FarmBadge))
	{
		FBSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		FBSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		FBSlot->SetPosition(FVector2D(28.0f, 24.0f));
		FBSlot->SetAutoSize(true);
	}

	UTextBlock* FarmBadgeTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FarmBadgeTxt"));
	FarmBadgeTxt->SetText(FText::FromString(TEXT("SANCTUARY FARM DIMENSION  (Z = -20,000)\nGrow 9mm, 12G & 5.56mm Ammo Crops  |  Barn Arsenal & Seed Store")));
	FarmBadgeTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.98f, 0.72f, 1.0f)));
	{
		FSlateFontInfo F = FarmBadgeTxt->GetFont();
		F.Size = 10;
		FarmBadgeTxt->SetFont(F);
	}
	FarmBadge->SetContent(FarmBadgeTxt);

	// =========================================================================
	// 2. BOTTOM HALF (0.50 -> 1.0 Y): Live Zombie Quarantine Compound Preview
	// =========================================================================
	BottomCombatViewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BottomCombatViewImage"));
	if (UCanvasPanelSlot* BotSlot = RootCanvas->AddChildToCanvas(BottomCombatViewImage))
	{
		BotSlot->SetAnchors(FAnchors(0.0f, 0.50f, 1.0f, 1.0f));
		BotSlot->SetOffsets(FMargin(0.0f));
	}

	// Bottom-Left Combat Dimension Badge
	UBorder* CombatBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CombatBadge"));
	CombatBadge->SetBrushColor(FLinearColor(0.14f, 0.04f, 0.04f, 0.88f));
	CombatBadge->SetPadding(FMargin(16.0f, 8.0f));
	if (UCanvasPanelSlot* CBSlot = RootCanvas->AddChildToCanvas(CombatBadge))
	{
		CBSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		CBSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		CBSlot->SetPosition(FVector2D(28.0f, -24.0f));
		CBSlot->SetAutoSize(true);
	}

	UTextBlock* CombatBadgeTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CombatBadgeTxt"));
	CombatBadgeTxt->SetText(FText::FromString(TEXT("QUARANTINE COMPOUND — COMBAT DIMENSION  (Z = 0)\nEliminate Roaming Zombies (+100G)  |  7-Gun Radial Wheel [Hold TAB]")));
	CombatBadgeTxt->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.65f, 0.58f, 1.0f)));
	{
		FSlateFontInfo F = CombatBadgeTxt->GetFont();
		F.Size = 10;
		CombatBadgeTxt->SetFont(F);
	}
	CombatBadge->SetContent(CombatBadgeTxt);

	// =========================================================================
	// 3. HORIZONTAL RIFT DIVIDER BAR AT Y = 0.50
	// =========================================================================
	UBorder* RiftOuterGlow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RiftOuterGlow"));
	RiftOuterGlow->SetBrushColor(FLinearColor(0.02f, 0.05f, 0.08f, 0.95f));
	if (UCanvasPanelSlot* RiftGlowSlot = RootCanvas->AddChildToCanvas(RiftOuterGlow))
	{
		RiftGlowSlot->SetAnchors(FAnchors(0.0f, 0.50f, 1.0f, 0.50f));
		RiftGlowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		RiftGlowSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	UBorder* RiftCoreLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RiftCoreLine"));
	RiftCoreLine->SetBrushColor(FLinearColor(0.25f, 0.92f, 0.78f, 0.95f));
	if (UCanvasPanelSlot* RiftLineSlot = RootCanvas->AddChildToCanvas(RiftCoreLine))
	{
		RiftLineSlot->SetAnchors(FAnchors(0.0f, 0.50f, 1.0f, 0.50f));
		RiftLineSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		RiftLineSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	// =========================================================================
	// 4. CENTER TITLE & START MENU CARD STRADDLING THE HORIZONTAL SPLIT
	// =========================================================================
	UBorder* CenterMenuFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CenterMenuFrame"));
	CenterMenuFrame->SetBrushColor(FLinearColor(0.03f, 0.045f, 0.065f, 0.94f));
	CenterMenuFrame->SetPadding(FMargin(32.0f, 22.0f));
	if (UCanvasPanelSlot* MenuSlot = RootCanvas->AddChildToCanvas(CenterMenuFrame))
	{
		MenuSlot->SetAnchors(FAnchors(0.50f, 0.50f));
		MenuSlot->SetAlignment(FVector2D(0.50f, 0.50f));
		MenuSlot->SetPosition(FVector2D(0.0f, 0.0f));
		MenuSlot->SetAutoSize(true);
	}

	UVerticalBox* MenuVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuVBox"));
	CenterMenuFrame->SetContent(MenuVBox);

	// Title
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameTitleText"));
	TitleText->SetText(FText::FromString(TEXT("DIMENSION SHIFT: HARVEST & HORDE")));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.90f, 0.42f, 1.0f)));
	{
		FSlateFontInfo F = TitleText->GetFont();
		F.Size = 22;
		TitleText->SetFont(F);
	}
	if (UVerticalBoxSlot* TSlot = MenuVBox->AddChildToVerticalBox(TitleText))
	{
		TSlot->SetHorizontalAlignment(HAlign_Center);
		TSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	// Tagline
	UTextBlock* SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameSubtitleText"));
	SubtitleText->SetText(FText::FromString(TEXT("TOP: CULTIVATE AMMO IN THE FARM VALLEY   |   BOTTOM: SURVIVE THE ZOMBIE QUARANTINE")));
	SubtitleText->SetJustification(ETextJustify::Center);
	SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.92f, 0.88f, 1.0f)));
	{
		FSlateFontInfo F = SubtitleText->GetFont();
		F.Size = 10;
		SubtitleText->SetFont(F);
	}
	if (UVerticalBoxSlot* SubSlot = MenuVBox->AddChildToVerticalBox(SubtitleText))
	{
		SubSlot->SetHorizontalAlignment(HAlign_Center);
		SubSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	// =========================================================================
	// TAB 0: MAIN MENU (Controls + START GAME / SETTINGS / QUIT)
	// =========================================================================
	PlayTabBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayTabBox"));
	MenuVBox->AddChildToVerticalBox(PlayTabBox);

	// Controls Quick-Reference Strip
	UBorder* ControlsBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ControlsBox"));
	ControlsBox->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.11f, 0.92f));
	ControlsBox->SetPadding(FMargin(18.0f, 8.0f));
	if (UVerticalBoxSlot* CSlot = PlayTabBox->AddChildToVerticalBox(ControlsBox))
	{
		CSlot->SetHorizontalAlignment(HAlign_Center);
		CSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	UTextBlock* ControlsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ControlsText"));
	ControlsText->SetText(FText::FromString(TEXT("[LEFT SHIFT] Teleport   |   [HOLD TAB] 7-Weapon Wheel   |   [I] Backpack   |   [F] Flashlight   |   [B] Barn Store   |   [P] Settings")));
	ControlsText->SetJustification(ETextJustify::Center);
	ControlsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.88f, 0.94f, 1.0f)));
	{
		FSlateFontInfo F = ControlsText->GetFont();
		F.Size = 9;
		ControlsText->SetFont(F);
	}
	ControlsBox->SetContent(ControlsText);

	// Action Buttons Row: [START GAME] + [SETTINGS] + [QUIT]
	UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MenuBtnRow"));
	if (UVerticalBoxSlot* BRowSlot = PlayTabBox->AddChildToVerticalBox(BtnRow))
	{
		BRowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	USizeBox* StartSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StartBtnSize"));
	StartSize->SetWidthOverride(240.0f);
	StartSize->SetHeightOverride(46.0f);
	if (UHorizontalBoxSlot* SSlot = BtnRow->AddChildToHorizontalBox(StartSize))
	{
		SSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
	}

	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartGameButton"));
	StartButton->SetBackgroundColor(FLinearColor(0.16f, 0.64f, 0.32f, 1.0f));
	StartButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::OnStartGameClicked);
	StartSize->SetContent(StartButton);

	StartButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartGameLabel"));
	StartButtonLabel->SetText(FText::FromString(TEXT("START GAME  [ENTER]")));
	StartButtonLabel->SetJustification(ETextJustify::Center);
	StartButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.98f, 0.82f, 1.0f)));
	{
		FSlateFontInfo F = StartButtonLabel->GetFont();
		F.Size = 12;
		StartButtonLabel->SetFont(F);
	}
	StartButton->SetContent(StartButtonLabel);

	USizeBox* SettingsBtnSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsTabSize"));
	SettingsBtnSize->SetWidthOverride(155.0f);
	SettingsBtnSize->SetHeightOverride(46.0f);
	if (UHorizontalBoxSlot* STSlot = BtnRow->AddChildToHorizontalBox(SettingsBtnSize))
	{
		STSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
	}

	TabSettingsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TabSettingsButton"));
	TabSettingsButton->SetBackgroundColor(FLinearColor(0.18f, 0.38f, 0.58f, 1.0f));
	TabSettingsButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::OnTabSettingsClicked);
	SettingsBtnSize->SetContent(TabSettingsButton);

	UTextBlock* SettingsTabLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingsTabLbl"));
	SettingsTabLbl->SetText(FText::FromString(TEXT("SETTINGS")));
	SettingsTabLbl->SetJustification(ETextJustify::Center);
	SettingsTabLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.94f, 0.72f, 1.0f)));
	{
		FSlateFontInfo F = SettingsTabLbl->GetFont();
		F.Size = 12;
		SettingsTabLbl->SetFont(F);
	}
	TabSettingsButton->SetContent(SettingsTabLbl);

	USizeBox* QuitSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("QuitBtnSize"));
	QuitSize->SetWidthOverride(130.0f);
	QuitSize->SetHeightOverride(46.0f);
	BtnRow->AddChildToHorizontalBox(QuitSize);

	QuitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuitGameButton"));
	QuitButton->SetBackgroundColor(FLinearColor(0.46f, 0.14f, 0.14f, 1.0f));
	QuitButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::OnQuitGameClicked);
	QuitSize->SetContent(QuitButton);

	UTextBlock* QuitLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuitGameLabel"));
	QuitLabel->SetText(FText::FromString(TEXT("QUIT")));
	QuitLabel->SetJustification(ETextJustify::Center);
	QuitLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo F = QuitLabel->GetFont();
		F.Size = 12;
		QuitLabel->SetFont(F);
	}
	QuitButton->SetContent(QuitLabel);

	// =========================================================================
	// TAB 1: SETTINGS TAB (Master Volume, Mouse Sensitivity, Developer Mode)
	// =========================================================================
	SettingsTabBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsTabBox"));
	SettingsTabBox->SetVisibility(ESlateVisibility::Collapsed);
	MenuVBox->AddChildToVerticalBox(SettingsTabBox);

	// Helper container width so sliders align cleanly
	USizeBox* SettingsWidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsWidthBox"));
	SettingsWidthBox->SetWidthOverride(560.0f);
	if (UVerticalBoxSlot* SWSlot = SettingsTabBox->AddChildToVerticalBox(SettingsWidthBox))
	{
		SWSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UVerticalBox* SettingsInner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsInner"));
	SettingsWidthBox->SetContent(SettingsInner);

	// Row 1: Master Volume Slider
	UBorder* VolRowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VolRowBg"));
	VolRowBg->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.11f, 0.94f));
	VolRowBg->SetPadding(FMargin(16.0f, 10.0f));
	if (UVerticalBoxSlot* VSlot = SettingsInner->AddChildToVerticalBox(VolRowBg))
	{
		VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UHorizontalBox* VolRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VolRow"));
	VolRowBg->SetContent(VolRow);

	UTextBlock* VolLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VolLabel"));
	VolLabel->SetText(FText::FromString(TEXT("MASTER VOLUME")));
	VolLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.94f, 0.98f, 1.0f)));
	{
		FSlateFontInfo F = VolLabel->GetFont();
		F.Size = 11;
		VolLabel->SetFont(F);
	}
	if (UHorizontalBoxSlot* VLSlot = VolRow->AddChildToHorizontalBox(VolLabel))
	{
		VLSlot->SetVerticalAlignment(VAlign_Center);
		VLSlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
	}

	VolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("VolumeSlider"));
	VolumeSlider->SetMinValue(0.0f);
	VolumeSlider->SetMaxValue(1.0f);
	VolumeSlider->SetStepSize(0.05f);
	VolumeSlider->SetValue(1.0f);
	VolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UStartMenuWidget::OnVolumeSliderChanged);
	if (UHorizontalBoxSlot* VSSlot = VolRow->AddChildToHorizontalBox(VolumeSlider))
	{
		VSSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		VSSlot->SetVerticalAlignment(VAlign_Center);
	}

	VolumeValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VolumeValueText"));
	VolumeValueText->SetText(FText::FromString(TEXT("100%")));
	VolumeValueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.98f, 0.62f, 1.0f)));
	{
		FSlateFontInfo F = VolumeValueText->GetFont();
		F.Size = 11;
		VolumeValueText->SetFont(F);
	}
	if (UHorizontalBoxSlot* VVSlot = VolRow->AddChildToHorizontalBox(VolumeValueText))
	{
		VVSlot->SetVerticalAlignment(VAlign_Center);
		VVSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	// Row 2: Mouse Sensitivity Slider
	UBorder* SensRowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SensRowBg"));
	SensRowBg->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.11f, 0.94f));
	SensRowBg->SetPadding(FMargin(16.0f, 10.0f));
	if (UVerticalBoxSlot* VSlot = SettingsInner->AddChildToVerticalBox(SensRowBg))
	{
		VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UHorizontalBox* SensRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SensRow"));
	SensRowBg->SetContent(SensRow);

	UTextBlock* SensLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SensLabel"));
	SensLabel->SetText(FText::FromString(TEXT("MOUSE SENSITIVITY")));
	SensLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.94f, 0.98f, 1.0f)));
	{
		FSlateFontInfo F = SensLabel->GetFont();
		F.Size = 11;
		SensLabel->SetFont(F);
	}
	if (UHorizontalBoxSlot* SLSlot = SensRow->AddChildToHorizontalBox(SensLabel))
	{
		SLSlot->SetVerticalAlignment(VAlign_Center);
		SLSlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
	}

	SensitivitySlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("SensitivitySlider"));
	SensitivitySlider->SetMinValue(0.2f);
	SensitivitySlider->SetMaxValue(3.0f);
	SensitivitySlider->SetStepSize(0.1f);
	SensitivitySlider->SetValue(1.0f);
	SensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &UStartMenuWidget::OnSensitivitySliderChanged);
	if (UHorizontalBoxSlot* SSSlot = SensRow->AddChildToHorizontalBox(SensitivitySlider))
	{
		SSSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SSSlot->SetVerticalAlignment(VAlign_Center);
	}

	SensitivityValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SensitivityValueText"));
	SensitivityValueText->SetText(FText::FromString(TEXT("1.0x")));
	SensitivityValueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.92f, 1.0f, 1.0f)));
	{
		FSlateFontInfo F = SensitivityValueText->GetFont();
		F.Size = 11;
		SensitivityValueText->SetFont(F);
	}
	if (UHorizontalBoxSlot* SVSlot = SensRow->AddChildToHorizontalBox(SensitivityValueText))
	{
		SVSlot->SetVerticalAlignment(VAlign_Center);
		SVSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	// Row 3: Developer Mode Toggle (Unlocks All 7 Weapons + Infinite Money)
	UBorder* DevRowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DevRowBg"));
	DevRowBg->SetBrushColor(FLinearColor(0.08f, 0.07f, 0.11f, 0.95f));
	DevRowBg->SetPadding(FMargin(16.0f, 10.0f));
	if (UVerticalBoxSlot* VSlot = SettingsInner->AddChildToVerticalBox(DevRowBg))
	{
		VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	UHorizontalBox* DevRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DevRow"));
	DevRowBg->SetContent(DevRow);

	UVerticalBox* DevInfoCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DevInfoCol"));
	if (UHorizontalBoxSlot* DISlot = DevRow->AddChildToHorizontalBox(DevInfoCol))
	{
		DISlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DISlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* DevTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DevTitle"));
	DevTitle->SetText(FText::FromString(TEXT("DEVELOPER MODE")));
	DevTitle->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.35f, 1.0f)));
	{
		FSlateFontInfo F = DevTitle->GetFont();
		F.Size = 11;
		DevTitle->SetFont(F);
	}
	DevInfoCol->AddChildToVerticalBox(DevTitle);

	UTextBlock* DevDesc = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DevDesc"));
	DevDesc->SetText(FText::FromString(TEXT("Unlocks all 7 weapons, Infinite Gold, Infinite Ammo & [+] (+1 min)")));
	DevDesc->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.78f, 0.86f, 1.0f)));
	{
		FSlateFontInfo F = DevDesc->GetFont();
		F.Size = 9;
		DevDesc->SetFont(F);
	}
	DevInfoCol->AddChildToVerticalBox(DevDesc);

	USizeBox* DevBtnSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DevBtnSize"));
	DevBtnSize->SetWidthOverride(130.0f);
	DevBtnSize->SetHeightOverride(36.0f);
	if (UHorizontalBoxSlot* DBSlot = DevRow->AddChildToHorizontalBox(DevBtnSize))
	{
		DBSlot->SetVerticalAlignment(VAlign_Center);
		DBSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	DevModeToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DevModeToggleButton"));
	DevModeToggleButton->SetBackgroundColor(FLinearColor(0.32f, 0.14f, 0.14f, 1.0f));
	DevModeToggleButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::OnToggleDeveloperModeClicked);
	DevBtnSize->SetContent(DevModeToggleButton);

	DevModeToggleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DevModeToggleLabel"));
	DevModeToggleLabel->SetText(FText::FromString(TEXT("OFF")));
	DevModeToggleLabel->SetJustification(ETextJustify::Center);
	DevModeToggleLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo F = DevModeToggleLabel->GetFont();
		F.Size = 11;
		DevModeToggleLabel->SetFont(F);
	}
	DevModeToggleButton->SetContent(DevModeToggleLabel);

	// Bottom Row inside Settings Tab: [BACK] + [APPLY & PLAY]
	UHorizontalBox* SettingsBtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsBtnRow"));
	if (UVerticalBoxSlot* SRSlot = SettingsInner->AddChildToVerticalBox(SettingsBtnRow))
	{
		SRSlot->SetHorizontalAlignment(HAlign_Center);
	}

	USizeBox* SettingsBackSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsBackSize"));
	SettingsBackSize->SetWidthOverride(150.0f);
	SettingsBackSize->SetHeightOverride(40.0f);
	if (UHorizontalBoxSlot* SBSlot = SettingsBtnRow->AddChildToHorizontalBox(SettingsBackSize))
	{
		SBSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
	}

	TabPlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SettingsBackBtn"));
	TabPlayButton->SetBackgroundColor(FLinearColor(0.20f, 0.24f, 0.30f, 1.0f));
	TabPlayButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::OnTabPlayClicked);
	SettingsBackSize->SetContent(TabPlayButton);

	UTextBlock* SettingsBackLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingsBackLbl"));
	SettingsBackLbl->SetText(FText::FromString(TEXT("BACK")));
	SettingsBackLbl->SetJustification(ETextJustify::Center);
	SettingsBackLbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo F = SettingsBackLbl->GetFont();
		F.Size = 11;
		SettingsBackLbl->SetFont(F);
	}
	TabPlayButton->SetContent(SettingsBackLbl);

	USizeBox* SettingsResumeSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsResumeSize"));
	SettingsResumeSize->SetWidthOverride(220.0f);
	SettingsResumeSize->SetHeightOverride(40.0f);
	SettingsBtnRow->AddChildToHorizontalBox(SettingsResumeSize);

	UButton* SettingsResumeBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SettingsResumeBtn"));
	SettingsResumeBtn->SetBackgroundColor(FLinearColor(0.16f, 0.64f, 0.32f, 1.0f));
	SettingsResumeBtn->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::OnStartGameClicked);
	SettingsResumeSize->SetContent(SettingsResumeBtn);

	UTextBlock* SettingsResumeLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingsResumeLbl"));
	SettingsResumeLbl->SetText(FText::FromString(TEXT("APPLY & PLAY")));
	SettingsResumeLbl->SetJustification(ETextJustify::Center);
	SettingsResumeLbl->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.98f, 0.82f, 1.0f)));
	{
		FSlateFontInfo F = SettingsResumeLbl->GetFont();
		F.Size = 11;
		SettingsResumeLbl->SetFont(F);
	}
	SettingsResumeBtn->SetContent(SettingsResumeLbl);
}

void UStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SpawnSplitScreenCameras();
	RefreshSettingsUI();

	if (StartButtonLabel && GetWorld() && GetWorld()->GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GetWorld()->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>())
		{
			StartButtonLabel->SetText(FText::FromString(
				Sub->bHasDismissedStartMenu
				? TEXT("RESUME GAME  [ESC / P]")
				: TEXT("START GAME  [ENTER / SPACE]")
			));
		}
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(Mode);
	}
}

void UStartMenuWidget::NativeDestruct()
{
	DestroySplitScreenCameras();
	Super::NativeDestruct();
}

void UStartMenuWidget::SpawnSplitScreenCameras()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Ensure Lvl_Farm is loaded at Z = -20,000 so the top camera sees the Barn, Silo, Crops & Trees
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Sub->EnsureFarmLevelLoaded(World);
		}
	}

	// 1. Create Render Targets (1280 x 360 widescreen 32:9 per half)
	if (!FarmRenderTarget)
	{
		FarmRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_StartMenu_Farm"));
		FarmRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		FarmRenderTarget->ClearColor = FLinearColor(0.42f, 0.65f, 0.90f, 1.0f);
		FarmRenderTarget->bAutoGenerateMips = false;
		FarmRenderTarget->InitAutoFormat(1280, 360);
		FarmRenderTarget->UpdateResourceImmediate(true);
	}

	if (!CombatRenderTarget)
	{
		CombatRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_StartMenu_Combat"));
		CombatRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		CombatRenderTarget->ClearColor = FLinearColor(0.02f, 0.03f, 0.04f, 1.0f);
		CombatRenderTarget->bAutoGenerateMips = false;
		CombatRenderTarget->InitAutoFormat(1280, 360);
		CombatRenderTarget->UpdateResourceImmediate(true);
	}

	if (TopFarmViewImage && FarmRenderTarget)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(FarmRenderTarget);
		Brush.ImageSize = FVector2D(1280.0f, 360.0f);
		TopFarmViewImage->SetBrush(Brush);
	}

	if (BottomCombatViewImage && CombatRenderTarget)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(CombatRenderTarget);
		Brush.ImageSize = FVector2D(1280.0f, 360.0f);
		BottomCombatViewImage->SetBrush(Brush);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 2. Spawn Top Half Farm SceneCapture Actor + Warm Golden Fill Lights so the Farm is bright even while Combat lighting is active
	if (!FarmCaptureActor)
	{
		FVector FarmOrigin(500000.0f, 0.0f, -20000.0f);
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				FarmOrigin = Sub->FarmWorldOffset;
			}
		}

		const FVector InitialFarmCamLoc = FarmOrigin + FVector(550.0f, 820.0f, 450.0f);
		const FVector FarmLookAt = FarmOrigin + FVector(-150.0f, -1200.0f, 180.0f);
		const FRotator InitialFarmRot = UKismetMathLibrary::FindLookAtRotation(InitialFarmCamLoc, FarmLookAt);

		FarmCaptureActor = World->SpawnActor<AActor>(AActor::StaticClass(), InitialFarmCamLoc, InitialFarmRot, Params);
		if (FarmCaptureActor)
		{
			USceneComponent* Root = NewObject<USceneComponent>(FarmCaptureActor, TEXT("FarmCamRoot"));
			Root->RegisterComponent();
			FarmCaptureActor->SetRootComponent(Root);

			USceneCaptureComponent2D* Cap = NewObject<USceneCaptureComponent2D>(FarmCaptureActor, TEXT("FarmCaptureComp"));
			Cap->SetupAttachment(Root);
			Cap->FOVAngle = 72.0f;
			Cap->TextureTarget = FarmRenderTarget;
			Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Cap->bCaptureEveryFrame = true;
			Cap->bCaptureOnMovement = true;
			Cap->bAlwaysPersistRenderingState = true;
			// CRITICAL: Disable Atmosphere and ExponentialHeightFog on the Farm SceneCapture!
			// The Combat level's ExponentialHeightFog is at Z=0 with density 0.065, which becomes 100% opaque grey at Z=-20,000!
			Cap->ShowFlags.SetAtmosphere(false);
			Cap->ShowFlags.SetFog(false);
			Cap->ShowFlags.SetVolumetricFog(false);
			Cap->ShowFlags.SetCloud(false);
			Cap->PostProcessBlendWeight = 1.0f;
			Cap->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
			Cap->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
			Cap->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
			Cap->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
			Cap->PostProcessSettings.bOverride_AutoExposureBias = true;
			Cap->PostProcessSettings.AutoExposureBias = 0.65f;
			Cap->PostProcessSettings.bOverride_ColorSaturation = true;
			Cap->PostProcessSettings.ColorSaturation = FVector4(1.18f, 1.18f, 1.12f, 1.0f);
			Cap->RegisterComponent();

			// Dedicated warm golden sun-fill point lights over the Farm Field, Barn, Silo, Pond & Trees (at Z = -20,000)
			const FVector LightOffsets[5] = {
				FVector(650.0f, 0.0f, 900.0f),        // Directly over the 4x4 Farm Crop Plots (X=325..1075, Y=-375..375)
				FVector(-700.0f, -2700.0f, 1100.0f),  // Over the Red Barn & Barn Shop entrance
				FVector(350.0f, -3200.0f, 1200.0f),   // Over the Silo & Hay Bales
				FVector(0.0f, -1300.0f, 1000.0f),     // Mid-valley pathway between Field and Barn
				FVector(-1600.0f, 800.0f, 1100.0f)    // West trees & pond approach
			};
			for (int32 i = 0; i < 5; ++i)
			{
				UPointLightComponent* WarmLight = NewObject<UPointLightComponent>(FarmCaptureActor, *FString::Printf(TEXT("MenuFarmFillLight_%d"), i));
				WarmLight->SetupAttachment(Root);
				WarmLight->SetIntensityUnits(ELightUnits::Candelas);
				WarmLight->SetIntensity(3200.0f);
				WarmLight->SetAttenuationRadius(9500.0f);
				WarmLight->SetLightColor(FColor(255, 224, 170));
				WarmLight->SetCastShadows(false);
				WarmLight->RegisterComponent();
				WarmLight->SetWorldLocation(FarmOrigin + LightOffsets[i]);
			}

			// Add a warm blue sky-dome fill light high above the Farm valley so grass and hills are brightly lit
			UPointLightComponent* SkyDomeFill = NewObject<UPointLightComponent>(FarmCaptureActor, TEXT("MenuFarmSkyDomeFill"));
			SkyDomeFill->SetupAttachment(Root);
			SkyDomeFill->SetIntensityUnits(ELightUnits::Candelas);
			SkyDomeFill->SetIntensity(6500.0f);
			SkyDomeFill->SetAttenuationRadius(16000.0f);
			SkyDomeFill->SetLightColor(FColor(195, 225, 255));
			SkyDomeFill->SetCastShadows(false);
			SkyDomeFill->RegisterComponent();
			SkyDomeFill->SetWorldLocation(FarmOrigin + FVector(0.0f, -1000.0f, 2800.0f));

			// Add a distant sky backdrop plane behind the Barn & Hills so with Atmosphere disabled the sky above the hills is warm golden-blue
			if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
			{
				UStaticMeshComponent* SkyBackdrop = NewObject<UStaticMeshComponent>(FarmCaptureActor, TEXT("MenuFarmSkyBackdrop"));
				SkyBackdrop->SetupAttachment(Root);
				SkyBackdrop->SetStaticMesh(CubeMesh);
				SkyBackdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SkyBackdrop->SetCastShadow(false);
				if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase")))
				{
					if (UMaterialInstanceDynamic* SkyMID = UMaterialInstanceDynamic::Create(BaseMat, FarmCaptureActor))
					{
						SkyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.32f, 0.58f, 0.88f, 1.0f));
						SkyMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.28f, 0.50f, 0.78f, 1.0f));
						SkyBackdrop->SetMaterial(0, SkyMID);
					}
				}
				SkyBackdrop->RegisterComponent();
				SkyBackdrop->SetWorldLocationAndRotation(FarmOrigin + FVector(-1200.0f, -8500.0f, 2500.0f), FRotator(0.0f, 15.0f, 0.0f));
				SkyBackdrop->SetWorldScale3D(FVector(240.0f, 1.0f, 80.0f));
			}
		}
	}

	// 3. Spawn Bottom Half Combat SceneCapture Actor + Eerie Moonlight Rim Fill looking across the Zombie Compound
	if (!CombatCaptureActor)
	{
		const FVector InitialCombatCamLoc(-980.0f, -980.0f, 340.0f);
		const FVector CombatLookAt(120.0f, 180.0f, 95.0f);
		const FRotator InitialCombatRot = UKismetMathLibrary::FindLookAtRotation(InitialCombatCamLoc, CombatLookAt);

		CombatCaptureActor = World->SpawnActor<AActor>(AActor::StaticClass(), InitialCombatCamLoc, InitialCombatRot, Params);
		if (CombatCaptureActor)
		{
			USceneComponent* Root = NewObject<USceneComponent>(CombatCaptureActor, TEXT("CombatCamRoot"));
			Root->RegisterComponent();
			CombatCaptureActor->SetRootComponent(Root);

			USceneCaptureComponent2D* Cap = NewObject<USceneCaptureComponent2D>(CombatCaptureActor, TEXT("CombatCaptureComp"));
			Cap->SetupAttachment(Root);
			Cap->FOVAngle = 76.0f;
			Cap->TextureTarget = CombatRenderTarget;
			Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Cap->bCaptureEveryFrame = true;
			Cap->bCaptureOnMovement = true;
			Cap->bAlwaysPersistRenderingState = true;
			Cap->PostProcessBlendWeight = 1.0f;
			Cap->PostProcessSettings.bOverride_AutoExposureBias = true;
			Cap->PostProcessSettings.AutoExposureBias = 0.85f;
			Cap->RegisterComponent();

			// Subtle cold moonlight fill so roaming zombies and ruined barricades read clearly in the bottom split view
			UPointLightComponent* MoonFill = NewObject<UPointLightComponent>(CombatCaptureActor, TEXT("MenuCombatMoonFill"));
			MoonFill->SetupAttachment(Root);
			MoonFill->SetRelativeLocation(FVector(150.0f, 0.0f, 180.0f));
			MoonFill->SetIntensityUnits(ELightUnits::Candelas);
			MoonFill->SetIntensity(120.0f);
			MoonFill->SetAttenuationRadius(3800.0f);
			MoonFill->SetLightColor(FColor(155, 190, 255));
			MoonFill->SetCastShadows(false);
			MoonFill->RegisterComponent();
		}
	}
}

void UStartMenuWidget::EnsurePreviewCropsInFarm()
{
	if (bSeededPreviewCrops)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find streamed Farm plots at Z < -10,000 and showcase mature 3D crops on the first few empty plots if none are visible yet
	TArray<AFarmPlotBase*> FarmPlots;
	for (TActorIterator<AFarmPlotBase> It(World); It; ++It)
	{
		if (It->GetActorLocation().Z < -10000.0f)
		{
			FarmPlots.Add(*It);
		}
	}

	if (FarmPlots.Num() > 0)
	{
		// Plant and advance the first 4 empty plots to Mature so the top Farm half of the Start Menu showcases 3D crops
		const FName ShowcaseSeeds[4] = {
			FName("Seed_ShotgunAmmo"),
			FName("Seed_RifleAmmo"),
			FName("Seed_PistolAmmo"),
			FName("Seed_Medkit")
		};

		int32 SeededCount = 0;
		for (AFarmPlotBase* Plot : FarmPlots)
		{
			if (Plot && Plot->CanPlant() && SeededCount < 4)
			{
				if (Plot->PlantSeed(ShowcaseSeeds[SeededCount]))
				{
					Plot->AdvanceGrowthStage(); // Sprout
					Plot->AdvanceGrowthStage(); // Growing
					Plot->AdvanceGrowthStage(); // Mature
					++SeededCount;
				}
			}
		}

		bSeededPreviewCrops = true;
	}
}

void UStartMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	MenuElapsedSeconds += InDeltaTime;
	EnsurePreviewCropsInFarm();

	// 1. Gently pan the Top Half Farm Camera across the Crop Plots, Red Barn, Silo & Trees
	if (FarmCaptureActor)
	{
		FVector FarmOrigin(500000.0f, 0.0f, -20000.0f);
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
				{
					FarmOrigin = Sub->FarmWorldOffset;
				}
			}
		}

		const float Angle = MenuElapsedSeconds * 0.22f;
		const FVector CamPos = FarmOrigin + FVector(
			420.0f + FMath::Sin(Angle) * 380.0f,
			640.0f + FMath::Cos(Angle * 0.7f) * 260.0f,
			420.0f + FMath::Sin(Angle * 0.5f) * 55.0f
		);
		const FVector TargetLook = FarmOrigin + FVector(-220.0f, -1350.0f, 180.0f);
		FarmCaptureActor->SetActorLocationAndRotation(CamPos, UKismetMathLibrary::FindLookAtRotation(CamPos, TargetLook));

		// Re-anchor fill lights in world space above the Farm Valley so they don't drift with the panning camera
		const FVector LightOffsets[5] = {
			FVector(650.0f, 0.0f, 900.0f),
			FVector(-700.0f, -2700.0f, 1100.0f),
			FVector(350.0f, -3200.0f, 1200.0f),
			FVector(0.0f, -1300.0f, 1000.0f),
			FVector(-1600.0f, 800.0f, 1100.0f)
		};
		TArray<UPointLightComponent*> Lights;
		FarmCaptureActor->GetComponents<UPointLightComponent>(Lights);
		for (int32 i = 0; i < Lights.Num(); ++i)
		{
			if (i < 5)
			{
				Lights[i]->SetWorldLocation(FarmOrigin + LightOffsets[i]);
			}
			else
			{
				Lights[i]->SetWorldLocation(FarmOrigin + FVector(0.0f, -1000.0f, 2800.0f));
			}
		}

		if (UStaticMeshComponent* SkyBackdrop = FarmCaptureActor->FindComponentByClass<UStaticMeshComponent>())
		{
			SkyBackdrop->SetWorldLocationAndRotation(FarmOrigin + FVector(-1200.0f, -8500.0f, 2500.0f), FRotator(0.0f, 15.0f, 0.0f));
		}
	}

	// 2. Orbit the Bottom Half Combat Camera around the Zombie Quarantine Compound while zombies roam below
	if (CombatCaptureActor)
	{
		const float OrbitAngle = MenuElapsedSeconds * 0.20f + 0.65f;
		const float Radius = 1180.0f;
		const FVector CamPos(
			FMath::Cos(OrbitAngle) * Radius,
			FMath::Sin(OrbitAngle) * Radius,
			325.0f + FMath::Sin(OrbitAngle * 1.5f) * 40.0f
		);
		const FVector TargetLook(0.0f, 0.0f, 85.0f);
		CombatCaptureActor->SetActorLocationAndRotation(CamPos, UKismetMathLibrary::FindLookAtRotation(CamPos, TargetLook));
	}
}

void UStartMenuWidget::DestroySplitScreenCameras()
{
	if (FarmCaptureActor)
	{
		FarmCaptureActor->Destroy();
		FarmCaptureActor = nullptr;
	}
	if (CombatCaptureActor)
	{
		CombatCaptureActor->Destroy();
		CombatCaptureActor = nullptr;
	}
}

FReply UStartMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Escape || Key == EKeys::P)
	{
		OnStartGameClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UStartMenuWidget::OnTabPlayClicked()
{
	SetActiveTab(0);
}

void UStartMenuWidget::OnTabSettingsClicked()
{
	SetActiveTab(1);
}

void UStartMenuWidget::OpenOnSettingsTab()
{
	SetActiveTab(1);
}

void UStartMenuWidget::SetActiveTab(int32 TabIndex)
{
	ActiveTabIndex = TabIndex;
	const bool bPlayActive = (TabIndex == 0);

	if (PlayTabBox)
	{
		PlayTabBox->SetVisibility(bPlayActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SettingsTabBox)
	{
		SettingsTabBox->SetVisibility(bPlayActive ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	RefreshSettingsUI();
}

void UStartMenuWidget::RefreshSettingsUI()
{
	UWorld* World = GetWorld();
	UDimensionShiftSubsystem* Sub = (World && World->GetGameInstance())
		? World->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>()
		: nullptr;
	if (!Sub)
	{
		return;
	}

	if (VolumeSlider)
	{
		VolumeSlider->SetValue(Sub->MasterVolume);
	}
	if (VolumeValueText)
	{
		VolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Sub->MasterVolume * 100.0f))));
	}
	if (SensitivitySlider)
	{
		SensitivitySlider->SetValue(Sub->MouseSensitivity);
	}
	if (SensitivityValueText)
	{
		SensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), Sub->MouseSensitivity)));
	}
	if (DevModeToggleButton && DevModeToggleLabel)
	{
		const bool bDev = Sub->IsDeveloperModeEnabled();
		DevModeToggleButton->SetBackgroundColor(
			bDev
			? FLinearColor(0.18f, 0.62f, 0.30f, 1.0f)
			: FLinearColor(0.32f, 0.14f, 0.14f, 1.0f)
		);
		DevModeToggleLabel->SetText(FText::FromString(bDev ? TEXT("ON (ACTIVE)") : TEXT("OFF")));
	}
}

void UStartMenuWidget::OnVolumeSliderChanged(float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Sub->SetMasterVolume(Value, World);
			}
		}
	}
	if (VolumeValueText)
	{
		VolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f))));
	}
}

void UStartMenuWidget::OnSensitivitySliderChanged(float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Sub->SetMouseSensitivity(Value);
			}
		}
	}
	if (SensitivityValueText)
	{
		SensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), Value)));
	}
}

void UStartMenuWidget::OnToggleDeveloperModeClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Sub->SetDeveloperMode(!Sub->IsDeveloperModeEnabled());
				RefreshSettingsUI();
			}
		}
	}
}

void UStartMenuWidget::OnStartGameClicked()
{
	DestroySplitScreenCameras();

	bool bStartingFreshGame = false;
	UDimensionShiftSubsystem* ShiftSub = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bStartingFreshGame = !Sub->bHasDismissedStartMenu;
				Sub->bIsStartMenuActive = false;
				Sub->bHasDismissedStartMenu = true;
				ShiftSub = Sub;
			}
		}
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
		PC->ResetIgnoreLookInput();
		PC->ResetIgnoreMoveInput();
		PC->SetInputMode(FInputModeGameOnly());

		// Always start a new game in the sunlit Farm world (not the Zombie world)
		if (bStartingFreshGame && ShiftSub)
		{
			if (ACharacter* PlayerChar = PC->GetCharacter())
			{
				ShiftSub->TeleportToFarmStart(PlayerChar);
			}
		}
	}

	RemoveFromParent();
}

void UStartMenuWidget::OnQuitGameClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
