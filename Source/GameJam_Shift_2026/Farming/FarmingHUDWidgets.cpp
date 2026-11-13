// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmingHUDWidgets.h"
#include "FarmingComponent.h"
#include "FarmPlotBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "GameFramework/Pawn.h"

// ============================================================================
// UCropPromptWidget
// ============================================================================

void UCropPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	if (!WidgetTree->RootWidget)
	{
		UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PromptRoot"));
		WidgetTree->RootWidget = RootOverlay;

		PromptContainer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptContainer"));
		PromptContainer->SetBrushColor(FLinearColor(0.04f, 0.06f, 0.08f, 0.92f));
		PromptContainer->SetPadding(FMargin(18.0f, 10.0f, 18.0f, 10.0f));
		RootOverlay->AddChildToOverlay(PromptContainer);

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PromptVBox"));
		PromptContainer->SetContent(VBox);

		PlotHeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlotHeaderText"));
		PlotHeaderText->SetJustification(ETextJustify::Center);
		PlotHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.55f, 1.0f)));
		{
			FSlateFontInfo Font = PlotHeaderText->GetFont();
			Font.Size = 12;
			PlotHeaderText->SetFont(Font);
		}
		if (UVerticalBoxSlot* HeaderSlot = VBox->AddChildToVerticalBox(PlotHeaderText))
		{
			HeaderSlot->SetHorizontalAlignment(HAlign_Center);
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}

		PlotStageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlotStageText"));
		PlotStageText->SetJustification(ETextJustify::Center);
		PlotStageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.90f, 0.95f, 1.0f)));
		{
			FSlateFontInfo Font = PlotStageText->GetFont();
			Font.Size = 10;
			PlotStageText->SetFont(Font);
		}
		if (UVerticalBoxSlot* StageSlot = VBox->AddChildToVerticalBox(PlotStageText))
		{
			StageSlot->SetHorizontalAlignment(HAlign_Center);
			StageSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}

		ActionHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionHintText"));
		ActionHintText->SetJustification(ETextJustify::Center);
		ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.95f, 0.55f, 1.0f)));
		{
			FSlateFontInfo Font = ActionHintText->GetFont();
			Font.Size = 11;
			ActionHintText->SetFont(Font);
		}
		if (UVerticalBoxSlot* HintSlot = VBox->AddChildToVerticalBox(ActionHintText))
		{
			HintSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	SetVisibility(ESlateVisibility::Hidden);
}

void UCropPromptWidget::UpdateFromPlot(AFarmPlotBase* Plot, EFarmTool ActiveTool, FName SelectedSeedID)
{
	if (!Plot || !PlotHeaderText || !PlotStageText || !ActionHintText)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	FString StageName;
	switch (Plot->GetCurrentStage())
	{
	case ECropStage::Empty:    StageName = TEXT("Empty / Tilled"); break;
	case ECropStage::Planted:  StageName = TEXT("Planted Seed"); break;
	case ECropStage::Sprout:   StageName = TEXT("Sprout"); break;
	case ECropStage::Growing:  StageName = TEXT("Growing Stalk"); break;
	case ECropStage::Mature:   StageName = TEXT("MATURE (Ready)"); break;
	case ECropStage::Withered: StageName = TEXT("Withered"); break;
	}

	if (Plot->CanPlant())
	{
		PlotHeaderText->SetText(FText::FromString(TEXT("SOIL PLOT — READY TO PLANT")));
		PlotHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.92f, 0.65f, 1.0f)));

		PlotStageText->SetText(FText::FromString(FString::Printf(TEXT("Selected Seed: %s"), *SelectedSeedID.ToString())));

		if (ActiveTool == EFarmTool::None)
		{
			ActionHintText->SetText(FText::FromString(TEXT("[LEFT CLICK] Plant Selected Seed")));
			ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.95f, 0.50f, 1.0f)));
		}
		else
		{
			ActionHintText->SetText(FText::FromString(TEXT("Press [1] (Hands) & Left Click to Plant Seed")));
			ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.35f, 1.0f)));
		}
	}
	else if (Plot->CanHarvest())
	{
		PlotHeaderText->SetText(FText::FromString(FString::Printf(TEXT("CROP READY: %s"), *Plot->PlantedSeedID.ToString().ToUpper())));
		PlotHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.25f, 1.0f)));

		PlotStageText->SetText(FText::FromString(FString::Printf(TEXT("Yield: +%d %s"), Plot->HarvestYield.Quantity, *Plot->HarvestYield.ItemID.ToString())));

		if (ActiveTool == EFarmTool::Harvester || ActiveTool == EFarmTool::None)
		{
			ActionHintText->SetText(FText::FromString(TEXT("[LEFT CLICK] Harvest Crop to Tarkov Backpack!")));
			ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 1.0f, 0.45f, 1.0f)));
		}
		else
		{
			ActionHintText->SetText(FText::FromString(TEXT("Press [4] (Harvester) or [1] & Left Click to Harvest!")));
			ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.25f, 1.0f)));
		}
	}
	else if (Plot->CanWater())
	{
		PlotHeaderText->SetText(FText::FromString(FString::Printf(TEXT("CROP: %s"), *Plot->PlantedSeedID.ToString().ToUpper())));
		PlotHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.82f, 1.0f, 1.0f)));

		PlotStageText->SetText(FText::FromString(FString::Printf(TEXT("Stage: %s   |   SOIL DRY — NEEDS WATER"), *StageName)));

		if (ActiveTool == EFarmTool::WateringCan)
		{
			ActionHintText->SetText(FText::FromString(TEXT("[LEFT CLICK] Water Plot to Advance Growth")));
			ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.90f, 1.0f, 1.0f)));
		}
		else
		{
			ActionHintText->SetText(FText::FromString(TEXT("Press [3] (Watering Can) & Left Click to Water")));
			ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.85f, 1.0f, 1.0f)));
		}
	}
	else
	{
		const int32 ProgressPct = FMath::RoundToInt(Plot->GetStageProgress() * 100.0f);
		PlotHeaderText->SetText(FText::FromString(FString::Printf(TEXT("CROP GROWING: %s"), *Plot->PlantedSeedID.ToString().ToUpper())));
		PlotHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.95f, 0.70f, 1.0f)));

		PlotStageText->SetText(FText::FromString(FString::Printf(TEXT("Stage: %s   |   Watered & Growing (%d%%)"), *StageName, ProgressPct)));
		ActionHintText->SetText(FText::FromString(TEXT("Growing... Wait for stage completion")));
		ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.82f, 0.88f, 1.0f)));
	}
}

// ============================================================================
// UFarmingHUDWidget
// ============================================================================

void UFarmingHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FarmingHUDCanvas"));
		WidgetTree->RootWidget = RootCanvas;

		// 1. Top-Center Dimension Banner
		UBorder* TopBanner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopDimensionBanner"));
		TopBanner->SetBrushColor(FLinearColor(0.05f, 0.12f, 0.08f, 0.88f));
		TopBanner->SetPadding(FMargin(20.0f, 7.0f, 20.0f, 7.0f));
		if (UCanvasPanelSlot* TopSlot = RootCanvas->AddChildToCanvas(TopBanner))
		{
			TopSlot->SetAnchors(FAnchors(0.5f, 0.0f));
			TopSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			TopSlot->SetPosition(FVector2D(0.0f, 18.0f));
			TopSlot->SetAutoSize(true);
		}

		UTextBlock* TopBannerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TopBannerText"));
		TopBannerText->SetText(FText::FromString(TEXT("FARMING DIMENSION   |   [SHIFT] Return to Combat   |   [TAB] Inventory   |   [H] Heal (Medkit)")));
		TopBannerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.95f, 0.75f, 1.0f)));
		{
			FSlateFontInfo Font = TopBannerText->GetFont();
			Font.Size = 10;
			TopBannerText->SetFont(Font);
		}
		TopBanner->SetContent(TopBannerText);

		// 2. Center Reticle Dot
		CenterReticle = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CenterReticle"));
		CenterReticle->SetBrushColor(FLinearColor(0.90f, 0.95f, 0.90f, 0.85f));
		if (UCanvasPanelSlot* ReticleSlot = RootCanvas->AddChildToCanvas(CenterReticle))
		{
			ReticleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ReticleSlot->SetPosition(FVector2D(0.0f, 0.0f));
			ReticleSlot->SetSize(FVector2D(6.0f, 6.0f));
		}

		// 3. Center-Bottom Crop Hover Prompt (WBP_CropPrompt)
		CropPromptWidget = WidgetTree->ConstructWidget<UCropPromptWidget>(UCropPromptWidget::StaticClass(), TEXT("CropHoverPrompt"));
		CropPromptWidget->Initialize();
		if (UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(CropPromptWidget))
		{
			PromptSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PromptSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			PromptSlot->SetPosition(FVector2D(0.0f, 42.0f));
			PromptSlot->SetAutoSize(true);
		}

		// 4. Bottom-Center 4-Slot Toolbelt Hotbar
		UBorder* HotbarFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HotbarFrame"));
		HotbarFrame->SetBrushColor(FLinearColor(0.03f, 0.05f, 0.06f, 0.90f));
		HotbarFrame->SetPadding(FMargin(8.0f, 8.0f, 8.0f, 8.0f));
		if (UCanvasPanelSlot* HotbarSlot = RootCanvas->AddChildToCanvas(HotbarFrame))
		{
			HotbarSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			HotbarSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			HotbarSlot->SetPosition(FVector2D(0.0f, -24.0f));
			HotbarSlot->SetAutoSize(true);
		}

		UHorizontalBox* ToolbeltRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ToolbeltRow"));
		HotbarFrame->SetContent(ToolbeltRow);

		const TCHAR* ToolKeys[4] = { TEXT("[1]"), TEXT("[2]"), TEXT("[3]"), TEXT("[4]") };
		const TCHAR* ToolNames[4] = { TEXT("HANDS / SEED"), TEXT("TACTICAL HOE"), TEXT("WATERING CAN"), TEXT("HARVESTER") };

		ToolSlotBorders.Reset();
		ToolSlotLabels.Reset();

		for (int32 i = 0; i < 4; ++i)
		{
			USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ToolSlotSize_%d"), i));
			SlotSize->SetWidthOverride(135.0f);
			SlotSize->SetHeightOverride(56.0f);

			if (UHorizontalBoxSlot* HSlot = ToolbeltRow->AddChildToHorizontalBox(SlotSize))
			{
				HSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 0.0f));
			}

			UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("ToolSlotBorder_%d"), i));
			SlotBorder->SetBrushColor(FLinearColor(0.10f, 0.12f, 0.14f, 0.95f));
			SlotBorder->SetPadding(FMargin(8.0f, 6.0f, 8.0f, 6.0f));
			SlotSize->SetContent(SlotBorder);

			UVerticalBox* SlotVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("ToolSlotVBox_%d"), i));
			SlotBorder->SetContent(SlotVBox);

			UTextBlock* KeyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ToolKeyLabel_%d"), i));
			KeyLabel->SetText(FText::FromString(ToolKeys[i]));
			KeyLabel->SetJustification(ETextJustify::Center);
			KeyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.72f, 0.78f, 1.0f)));
			{
				FSlateFontInfo Font = KeyLabel->GetFont();
				Font.Size = 9;
				KeyLabel->SetFont(Font);
			}
			if (UVerticalBoxSlot* KeyVSlot = SlotVBox->AddChildToVerticalBox(KeyLabel))
			{
				KeyVSlot->SetHorizontalAlignment(HAlign_Center);
				KeyVSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
			}

			UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ToolNameLabel_%d"), i));
			NameLabel->SetText(FText::FromString(ToolNames[i]));
			NameLabel->SetJustification(ETextJustify::Center);
			NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 0.92f, 1.0f)));
			{
				FSlateFontInfo Font = NameLabel->GetFont();
				Font.Size = 10;
				NameLabel->SetFont(Font);
			}
			if (UVerticalBoxSlot* NameVSlot = SlotVBox->AddChildToVerticalBox(NameLabel))
			{
				NameVSlot->SetHorizontalAlignment(HAlign_Center);
			}

			ToolSlotBorders.Add(SlotBorder);
			ToolSlotLabels.Add(NameLabel);
		}

		// 5. Bottom-Right Seed Pouch Panel
		UBorder* SeedPouchCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SeedPouchCard"));
		SeedPouchCard->SetBrushColor(FLinearColor(0.04f, 0.09f, 0.06f, 0.92f));
		SeedPouchCard->SetPadding(FMargin(16.0f, 10.0f, 16.0f, 10.0f));
		if (UCanvasPanelSlot* PouchSlot = RootCanvas->AddChildToCanvas(SeedPouchCard))
		{
			PouchSlot->SetAnchors(FAnchors(1.0f, 1.0f));
			PouchSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			PouchSlot->SetPosition(FVector2D(-24.0f, -24.0f));
			PouchSlot->SetAutoSize(true);
		}

		UVerticalBox* PouchVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PouchVBox"));
		SeedPouchCard->SetContent(PouchVBox);

		UTextBlock* PouchHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PouchHeader"));
		PouchHeader->SetText(FText::FromString(TEXT("SEED POUCH  [Press T to Cycle]")));
		PouchHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.88f, 0.70f, 1.0f)));
		{
			FSlateFontInfo Font = PouchHeader->GetFont();
			Font.Size = 9;
			PouchHeader->SetFont(Font);
		}
		if (UVerticalBoxSlot* PHSlot = PouchVBox->AddChildToVerticalBox(PouchHeader))
		{
			PHSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		ActiveSeedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActiveSeedText"));
		ActiveSeedText->SetText(FText::FromString(TEXT("SHOTGUN CORN (12G)")));
		ActiveSeedText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.92f, 0.45f, 1.0f)));
		{
			FSlateFontInfo Font = ActiveSeedText->GetFont();
			Font.Size = 12;
			ActiveSeedText->SetFont(Font);
		}
		if (UVerticalBoxSlot* ASTSlot = PouchVBox->AddChildToVerticalBox(ActiveSeedText))
		{
			ASTSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}

		SeedYieldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SeedYieldText"));
		SeedYieldText->SetText(FText::FromString(TEXT("Harvest Yield: +8x 12G Shotgun Shells")));
		SeedYieldText->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.92f, 0.82f, 1.0f)));
		{
			FSlateFontInfo Font = SeedYieldText->GetFont();
			Font.Size = 9;
			SeedYieldText->SetFont(Font);
		}
		PouchVBox->AddChildToVerticalBox(SeedYieldText);
	}
}

void UFarmingHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshHUD();
}

void UFarmingHUDWidget::DescribeSeed(FName SeedID, FString& OutDisplayName, FString& OutYieldDesc)
{
	const FString SeedStr = SeedID.ToString();
	if (SeedStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("SHOTGUN CORN (12G Shells)");
		OutYieldDesc = TEXT("Harvest Yield: +8x 12G Shotgun Shells (1x1 Grid)");
	}
	else if (SeedStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("RIFLE WHEAT (5.56mm Ammo)");
		OutYieldDesc = TEXT("Harvest Yield: +30x 5.56mm Rifle Ammo (1x1 Grid)");
	}
	else if (SeedStr.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("PISTOL VINE (9mm Ammo)");
		OutYieldDesc = TEXT("Harvest Yield: +25x 9mm Pistol Ammo (1x1 Grid)");
	}
	else if (SeedStr.Contains(TEXT("Medkit"), ESearchCase::IgnoreCase) || SeedStr.Contains(TEXT("Health"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("MEDICINAL HERB (Medkit)");
		OutYieldDesc = TEXT("Harvest Yield: +1x Medkit (+50 HP on [H])");
	}
	else
	{
		OutDisplayName = SeedStr.ToUpper();
		OutYieldDesc = TEXT("Harvest Yield: +5x Resource");
	}
}

void UFarmingHUDWidget::RefreshHUD()
{
	APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		return;
	}

	UFarmingComponent* FarmingComp = OwningPawn->FindComponentByClass<UFarmingComponent>();
	if (!FarmingComp)
	{
		return;
	}

	const EFarmTool ActiveTool = FarmingComp->GetActiveTool();
	const int32 ActiveIdx = static_cast<int32>(ActiveTool);

	// 1. Update Toolbelt Slot Highlights
	for (int32 i = 0; i < ToolSlotBorders.Num(); ++i)
	{
		const bool bIsSelected = (i == ActiveIdx);
		if (ToolSlotBorders[i])
		{
			ToolSlotBorders[i]->SetBrushColor(
				bIsSelected
				? FLinearColor(0.18f, 0.52f, 0.28f, 1.0f)
				: FLinearColor(0.09f, 0.11f, 0.13f, 0.92f)
			);
		}
		if (ToolSlotLabels.IsValidIndex(i) && ToolSlotLabels[i])
		{
			ToolSlotLabels[i]->SetColorAndOpacity(FSlateColor(
				bIsSelected
				? FLinearColor(1.0f, 0.95f, 0.45f, 1.0f)
				: FLinearColor(0.82f, 0.86f, 0.88f, 1.0f)
			));
		}
	}

	// 2. Update Seed Pouch Display
	const FName SelectedSeed = FarmingComp->GetSelectedSeedID();
	if (ActiveSeedText && SeedYieldText)
	{
		FString SeedDisplay;
		FString YieldDisplay;
		DescribeSeed(SelectedSeed, SeedDisplay, YieldDisplay);
		ActiveSeedText->SetText(FText::FromString(SeedDisplay));
		SeedYieldText->SetText(FText::FromString(YieldDisplay));
	}

	// 3. Update Targeted Plot Prompt & Reticle
	AFarmPlotBase* TargetedPlot = FarmingComp->GetTargetedPlot();
	if (CenterReticle)
	{
		CenterReticle->SetBrushColor(
			TargetedPlot
			? FLinearColor(0.25f, 1.0f, 0.45f, 1.0f)
			: FLinearColor(0.90f, 0.95f, 0.90f, 0.80f)
		);
	}

	if (CropPromptWidget)
	{
		CropPromptWidget->UpdateFromPlot(TargetedPlot, ActiveTool, SelectedSeed);
	}
}
