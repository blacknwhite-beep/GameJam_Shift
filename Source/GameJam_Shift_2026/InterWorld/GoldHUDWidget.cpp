// Copyright GameJam Shift 2026. All Rights Reserved.

#include "InterWorld/GoldHUDWidget.h"
#include "DimensionShiftSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UGoldHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GoldRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	// =========================================================================
	// 1. TOP-RIGHT: GOLD BALANCE BADGE
	// =========================================================================
	UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GoldBadge"));
	Badge->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.90f));
	Badge->SetPadding(FMargin(18.0f, 9.0f));
	if (UCanvasPanelSlot* BadgeSlot = RootCanvas->AddChildToCanvas(Badge))
	{
		BadgeSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		BadgeSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		BadgeSlot->SetPosition(FVector2D(-28.0f, 22.0f));
		BadgeSlot->SetAutoSize(true);
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoldRow"));
	Badge->SetContent(Row);

	GoldAmountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldAmountText"));
	FSlateFontInfo MainFont = GoldAmountText->GetFont();
	MainFont.Size = 18;
	GoldAmountText->SetFont(MainFont);
	GoldAmountText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.25f, 1.0f)));
	GoldAmountText->SetText(FText::FromString(TEXT("GOLD: 2000")));
	UHorizontalBoxSlot* MainSlot = Row->AddChildToHorizontalBox(GoldAmountText);
	MainSlot->SetVerticalAlignment(VAlign_Center);

	GoldDeltaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldDeltaText"));
	FSlateFontInfo DeltaFont = GoldDeltaText->GetFont();
	DeltaFont.Size = 15;
	GoldDeltaText->SetFont(DeltaFont);
	GoldDeltaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 1.0f, 0.45f, 1.0f)));
	GoldDeltaText->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* DeltaSlot = Row->AddChildToHorizontalBox(GoldDeltaText);
	DeltaSlot->SetVerticalAlignment(VAlign_Center);
	DeltaSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));

	// =========================================================================
	// 2. MIDDLE-RIGHT: 24-HOUR CLOCK & DIMENSION SCHEDULE WIDGET
	// =========================================================================
	ClockCardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ClockCardBorder"));
	ClockCardBorder->SetBrushColor(FLinearColor(0.04f, 0.07f, 0.09f, 0.88f));
	ClockCardBorder->SetPadding(FMargin(18.0f, 12.0f));
	if (UCanvasPanelSlot* ClockSlot = RootCanvas->AddChildToCanvas(ClockCardBorder))
	{
		ClockSlot->SetAnchors(FAnchors(1.0f, 0.50f));
		ClockSlot->SetAlignment(FVector2D(1.0f, 0.50f));
		ClockSlot->SetPosition(FVector2D(-26.0f, 16.0f));
		ClockSlot->SetAutoSize(true);
	}

	UVerticalBox* ClockVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ClockVBox"));
	ClockCardBorder->SetContent(ClockVBox);

	ClockTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClockTimeText"));
	ClockTimeText->SetText(FText::FromString(TEXT("06:00  (6:00 AM)")));
	ClockTimeText->SetJustification(ETextJustify::Right);
	ClockTimeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.94f, 0.72f, 1.0f)));
	{
		FSlateFontInfo F = ClockTimeText->GetFont();
		F.Size = 18;
		ClockTimeText->SetFont(F);
	}
	if (UVerticalBoxSlot* VSlot = ClockVBox->AddChildToVerticalBox(ClockTimeText))
	{
		VSlot->SetHorizontalAlignment(HAlign_Right);
		VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	ClockPhaseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClockPhaseText"));
	ClockPhaseText->SetText(FText::FromString(TEXT("DAY 1  |  FARM HOURS (06:00 - 00:00)")));
	ClockPhaseText->SetJustification(ETextJustify::Right);
	ClockPhaseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.96f, 0.68f, 1.0f)));
	{
		FSlateFontInfo F = ClockPhaseText->GetFont();
		F.Size = 10;
		ClockPhaseText->SetFont(F);
	}
	if (UVerticalBoxSlot* VSlot = ClockVBox->AddChildToVerticalBox(ClockPhaseText))
	{
		VSlot->SetHorizontalAlignment(HAlign_Right);
		VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	ClockShiftStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClockShiftStatusText"));
	ClockShiftStatusText->SetText(FText::FromString(TEXT("COMBAT SHIFT UNLOCKS AT 00:00")));
	ClockShiftStatusText->SetJustification(ETextJustify::Right);
	ClockShiftStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.72f, 0.42f, 1.0f)));
	{
		FSlateFontInfo F = ClockShiftStatusText->GetFont();
		F.Size = 9;
		ClockShiftStatusText->SetFont(F);
	}
	if (UVerticalBoxSlot* VSlot = ClockVBox->AddChildToVerticalBox(ClockShiftStatusText))
	{
		VSlot->SetHorizontalAlignment(HAlign_Right);
	}
}

void UGoldHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Subsystem->OnGoldChanged.AddUniqueDynamic(this, &UGoldHUDWidget::HandleGoldChanged);
				if (GoldAmountText)
				{
					GoldAmountText->SetText(FText::FromString(
						Subsystem->IsDeveloperModeEnabled()
						? TEXT("GOLD: INF (DEV)")
						: FString::Printf(TEXT("GOLD: %d"), Subsystem->GetGold())
					));
				}
			}
		}
	}

	RefreshClockDisplay();
}

void UGoldHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Subsystem->OnGoldChanged.RemoveDynamic(this, &UGoldHUDWidget::HandleGoldChanged);
			}
		}
	}
	Super::NativeDestruct();
}

void UGoldHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DeltaDisplayRemaining > 0.0f && GoldDeltaText)
	{
		DeltaDisplayRemaining -= InDeltaTime;
		if (DeltaDisplayRemaining <= 0.0f)
		{
			GoldDeltaText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			GoldDeltaText->SetRenderOpacity(FMath::Clamp(DeltaDisplayRemaining / 1.4f, 0.0f, 1.0f));
		}
	}

	RefreshClockDisplay();
}

void UGoldHUDWidget::RefreshClockDisplay()
{
	UWorld* World = GetWorld();
	UDimensionShiftSubsystem* Sub = (World && World->GetGameInstance())
		? World->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>()
		: nullptr;
	if (!Sub || !ClockTimeText || !ClockPhaseText || !ClockShiftStatusText)
	{
		return;
	}

	const bool bIsNight = Sub->IsNightWindow();
	const bool bInCombat = (Sub->GetCurrentDimension() == EGameDimension::Combat);
	const bool bDev = Sub->IsDeveloperModeEnabled();

	ClockTimeText->SetText(FText::FromString(
		FString::Printf(TEXT("%s  (%s)"), *Sub->GetFormattedTime24H(), *Sub->GetFormattedTime12H())
	));

	if (bIsNight)
	{
		ClockTimeText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.45f, 0.38f, 1.0f)));
		ClockPhaseText->SetText(FText::FromString(FString::Printf(TEXT("DAY %d  |  ZOMBIE NIGHT (00:00 - 06:00)"), Sub->CurrentDay)));
		ClockPhaseText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.65f, 0.58f, 1.0f)));

		if (ClockCardBorder)
		{
			ClockCardBorder->SetBrushColor(FLinearColor(0.11f, 0.035f, 0.04f, 0.90f));
		}

		if (bInCombat)
		{
			ClockShiftStatusText->SetText(FText::FromString(
				bDev ? TEXT("FARM SHIFT UNLOCKS AT 06:00  |  [+] +1m") : TEXT("FARM SHIFT UNLOCKS AT 06:00")
			));
			ClockShiftStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.72f, 0.42f, 1.0f)));
		}
		else
		{
			ClockShiftStatusText->SetText(FText::FromString(
				bDev ? TEXT("[LEFT SHIFT] ENTER COMBAT UNLOCKED!  |  [+] +1m") : TEXT("[LEFT SHIFT] ENTER COMBAT UNLOCKED!")
			));
			ClockShiftStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.40f, 1.0f, 0.55f, 1.0f)));
		}
	}
	else
	{
		ClockTimeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.94f, 0.72f, 1.0f)));
		ClockPhaseText->SetText(FText::FromString(FString::Printf(TEXT("DAY %d  |  FARM HOURS (06:00 - 00:00)"), Sub->CurrentDay)));
		ClockPhaseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.96f, 0.68f, 1.0f)));

		if (ClockCardBorder)
		{
			ClockCardBorder->SetBrushColor(FLinearColor(0.04f, 0.08f, 0.06f, 0.90f));
		}

		if (!bInCombat)
		{
			ClockShiftStatusText->SetText(FText::FromString(
				bDev ? TEXT("COMBAT SHIFT UNLOCKS AT 00:00  |  [+] +1m") : TEXT("COMBAT SHIFT UNLOCKS AT 00:00")
			));
			ClockShiftStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.78f, 0.45f, 1.0f)));
		}
		else
		{
			ClockShiftStatusText->SetText(FText::FromString(
				bDev ? TEXT("[LEFT SHIFT] RETURN TO FARM UNLOCKED!  |  [+] +1m") : TEXT("[LEFT SHIFT] RETURN TO FARM UNLOCKED!")
			));
			ClockShiftStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.40f, 1.0f, 0.55f, 1.0f)));
		}
	}
}

void UGoldHUDWidget::HandleGoldChanged(int32 NewGold, int32 Delta)
{
	bool bDevMode = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bDevMode = Sub->IsDeveloperModeEnabled();
			}
		}
	}

	if (GoldAmountText)
	{
		GoldAmountText->SetText(FText::FromString(
			bDevMode
			? TEXT("GOLD: INF (DEV)")
			: FString::Printf(TEXT("GOLD: %d"), NewGold)
		));
	}

	if (GoldDeltaText && Delta != 0)
	{
		const bool bPositive = (Delta > 0);
		GoldDeltaText->SetColorAndOpacity(FSlateColor(bPositive
			? FLinearColor(0.35f, 1.0f, 0.45f, 1.0f)
			: FLinearColor(1.0f, 0.35f, 0.30f, 1.0f)));
		GoldDeltaText->SetText(FText::FromString(bPositive
			? FString::Printf(TEXT("+%d"), Delta)
			: FString::Printf(TEXT("%d"), Delta)));
		GoldDeltaText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		GoldDeltaText->SetRenderOpacity(1.0f);
		DeltaDisplayRemaining = 1.4f;
	}
}
