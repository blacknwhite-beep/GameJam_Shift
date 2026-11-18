// Copyright GameJam Shift 2026. All Rights Reserved.

#include "InterWorld/GoldHUDWidget.h"
#include "DimensionShiftSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
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

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GoldRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GoldBadge"));
	Badge->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.85f));
	Badge->SetPadding(FMargin(16.0f, 8.0f));
	UOverlaySlot* BadgeSlot = Root->AddChildToOverlay(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Right);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(FMargin(0.0f, 24.0f, 28.0f, 0.0f));

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
					GoldAmountText->SetText(FText::FromString(FString::Printf(TEXT("GOLD: %d"), Subsystem->GetGold())));
				}
			}
		}
	}
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
}

void UGoldHUDWidget::HandleGoldChanged(int32 NewGold, int32 Delta)
{
	if (GoldAmountText)
	{
		GoldAmountText->SetText(FText::FromString(FString::Printf(TEXT("GOLD: %d"), NewGold)));
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
