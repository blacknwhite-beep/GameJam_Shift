// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Farming/BarnShopWidget.h"
#include "Farming/FarmingComponent.h"
#include "DimensionShiftSubsystem.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	void AddShopRow(
		UWidgetTree* Tree,
		UVerticalBox* ParentBox,
		const FString& RowId,
		const FString& Title,
		const FString& Desc,
		int32 Price,
		UButton*& OutButton)
	{
		UBorder* RowBg = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("RowBg_%s"), *RowId));
		RowBg->SetBrushColor(FLinearColor(0.09f, 0.11f, 0.12f, 0.95f));
		RowBg->SetPadding(FMargin(14.0f, 10.0f));
		if (UVerticalBoxSlot* VSlot = ParentBox->AddChildToVerticalBox(RowBg))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}

		UHorizontalBox* HBox = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("RowH_%s"), *RowId));
		RowBg->SetContent(HBox);

		UVerticalBox* InfoCol = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("RowInfo_%s"), *RowId));
		if (UHorizontalBoxSlot* InfoSlot = HBox->AddChildToHorizontalBox(InfoCol))
		{
			InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			InfoSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* TitleTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("RowTitle_%s"), *RowId));
		TitleTxt->SetText(FText::FromString(Title));
		TitleTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 0.92f, 1.0f)));
		FSlateFontInfo TitleFont = TitleTxt->GetFont();
		TitleFont.Size = 13;
		TitleTxt->SetFont(TitleFont);
		InfoCol->AddChildToVerticalBox(TitleTxt);

		UTextBlock* DescTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("RowDesc_%s"), *RowId));
		DescTxt->SetText(FText::FromString(Desc));
		DescTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.78f, 0.70f, 1.0f)));
		FSlateFontInfo DescFont = DescTxt->GetFont();
		DescFont.Size = 10;
		DescTxt->SetFont(DescFont);
		InfoCol->AddChildToVerticalBox(DescTxt);

		USizeBox* BtnSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("RowBtnSize_%s"), *RowId));
		BtnSize->SetWidthOverride(130.0f);
		BtnSize->SetHeightOverride(40.0f);
		if (UHorizontalBoxSlot* BtnSlot = HBox->AddChildToHorizontalBox(BtnSize))
		{
			BtnSlot->SetVerticalAlignment(VAlign_Center);
			BtnSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
		}

		OutButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("BuyBtn_%s"), *RowId));
		OutButton->SetBackgroundColor(FLinearColor(0.20f, 0.58f, 0.28f, 1.0f));
		BtnSize->SetContent(OutButton);

		UTextBlock* BtnLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("BtnTxt_%s"), *RowId));
		BtnLabel->SetText(FText::FromString(FString::Printf(TEXT("BUY (%d G)"), Price)));
		BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.65f, 1.0f)));
		FSlateFontInfo BtnFont = BtnLabel->GetFont();
		BtnFont.Size = 11;
		BtnLabel->SetFont(BtnFont);
		OutButton->SetContent(BtnLabel);
	}
}

void UBarnShopWidget::NativeOnInitialized()
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

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPanel"));
	Panel->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.06f, 0.95f));
	Panel->SetPadding(FMargin(24.0f, 20.0f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D(0.0f, 0.0f));
		PanelSlot->SetSize(FVector2D(580.0f, 490.0f));
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopVBox"));
	Panel->SetContent(VBox);

	// Header row (Title + Gold)
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopHeader"));
	if (UVerticalBoxSlot* HSlot = VBox->AddChildToVerticalBox(Header))
	{
		HSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTitle"));
	Title->SetText(FText::FromString(TEXT("BARN SEED & LAND SHOP")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.45f, 1.0f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 18;
	Title->SetFont(TitleFont);
	if (UHorizontalBoxSlot* TSlot = Header->AddChildToHorizontalBox(Title))
	{
		TSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TSlot->SetVerticalAlignment(VAlign_Center);
	}

	GoldHeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopGoldText"));
	GoldHeaderText->SetText(FText::FromString(TEXT("GOLD: 2000")));
	GoldHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.25f, 1.0f)));
	FSlateFontInfo GoldFont = GoldHeaderText->GetFont();
	GoldFont.Size = 16;
	GoldHeaderText->SetFont(GoldFont);
	if (UHorizontalBoxSlot* GSlot = Header->AddChildToHorizontalBox(GoldHeaderText))
	{
		GSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 5 Shop items
	UButton* BtnShotgun = nullptr;
	UButton* BtnRifle = nullptr;
	UButton* BtnPistol = nullptr;
	UButton* BtnMedkit = nullptr;
	UButton* BtnPlot = nullptr;

	AddShopRow(WidgetTree, VBox, TEXT("Shotgun"), TEXT("Shotgun Corn Seeds"), TEXT("Harvests 12G Shotgun Shells (cycles with [T])"), 150, BtnShotgun);
	AddShopRow(WidgetTree, VBox, TEXT("Rifle"), TEXT("Rifle Wheat Seeds"), TEXT("Harvests 5.56mm Rifle Ammo (cycles with [T])"), 200, BtnRifle);
	AddShopRow(WidgetTree, VBox, TEXT("Pistol"), TEXT("Pistol Berry Seeds"), TEXT("Harvests 9mm Pistol Ammo (cycles with [T])"), 100, BtnPistol);
	AddShopRow(WidgetTree, VBox, TEXT("Medkit"), TEXT("Medicinal Herb Seeds"), TEXT("Harvests Combat Medkit (+50 HP on [H])"), 250, BtnMedkit);
	AddShopRow(WidgetTree, VBox, TEXT("Plot"), TEXT("Expandable Farm Plot"), TEXT("Adds 1x1 Farm Plot to Backpack -> click in Inventory to place!"), 300, BtnPlot);

	if (BtnShotgun) { BtnShotgun->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyShotgunSeeds); }
	if (BtnRifle) { BtnRifle->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyRifleSeeds); }
	if (BtnPistol) { BtnPistol->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyPistolSeeds); }
	if (BtnMedkit) { BtnMedkit->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyMedkitSeeds); }
	if (BtnPlot) { BtnPlot->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyFarmPlot); }

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopStatusText"));
	StatusText->SetText(FText::FromString(TEXT("Purchase seeds to add them to your [T] pouch rotation.")));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.85f, 0.78f, 1.0f)));
	FSlateFontInfo StatFont = StatusText->GetFont();
	StatFont.Size = 11;
	StatusText->SetFont(StatFont);
	if (UVerticalBoxSlot* SSlot = VBox->AddChildToVerticalBox(StatusText))
	{
		SSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 10.0f));
	}

	UButton* CloseBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopCloseBtn"));
	CloseBtn->SetBackgroundColor(FLinearColor(0.45f, 0.16f, 0.16f, 1.0f));
	CloseBtn->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnCloseClicked);
	VBox->AddChildToVerticalBox(CloseBtn);

	UTextBlock* CloseTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopCloseTxt"));
	CloseTxt->SetText(FText::FromString(TEXT("CLOSE SHOP [ESC / B]")));
	CloseTxt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo CloseFont = CloseTxt->GetFont();
	CloseFont.Size = 12;
	CloseTxt->SetFont(CloseFont);
	CloseBtn->SetContent(CloseTxt);
}

void UBarnShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshGoldText();
}

FReply UBarnShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::B)
	{
		OnCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBarnShopWidget::RefreshGoldText()
{
	if (!GoldHeaderText || !GetWorld() || !GetWorld()->GetGameInstance())
	{
		return;
	}
	if (UDimensionShiftSubsystem* Sub = GetWorld()->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>())
	{
		GoldHeaderText->SetText(FText::FromString(FString::Printf(TEXT("GOLD: %d"), Sub->GetGold())));
	}
}

void UBarnShopWidget::SetStatus(const FString& Message, const FLinearColor& Color)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UBarnShopWidget::TryBuySeed(FName SeedID, const FString& DisplayName, int32 Price)
{
	UWorld* World = GetWorld();
	UDimensionShiftSubsystem* Sub = (World && World->GetGameInstance())
		? World->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>()
		: nullptr;
	APawn* Pawn = GetOwningPlayerPawn();
	UFarmingComponent* FarmComp = Pawn ? Pawn->FindComponentByClass<UFarmingComponent>() : nullptr;

	if (!Sub || !FarmComp)
	{
		return;
	}

	if (!Sub->SpendGold(Price))
	{
		SetStatus(TEXT("Not enough Gold! Eliminate zombies in Combat to earn +100G each."), FLinearColor(1.0f, 0.35f, 0.30f, 1.0f));
		return;
	}

	FarmComp->AddPurchasedSeed(SeedID, 1);
	RefreshGoldText();
	SetStatus(
		FString::Printf(TEXT("Purchased +1 %s! (Owned: x%d — press [T] to cycle)"), *DisplayName, FarmComp->GetSeedCount(SeedID)),
		FLinearColor(0.35f, 1.0f, 0.45f, 1.0f)
	);
}

void UBarnShopWidget::OnBuyShotgunSeeds() { TryBuySeed(FName("Seed_ShotgunAmmo"), TEXT("Shotgun Corn Seed"), 150); }
void UBarnShopWidget::OnBuyRifleSeeds()   { TryBuySeed(FName("Seed_RifleAmmo"), TEXT("Rifle Wheat Seed"), 200); }
void UBarnShopWidget::OnBuyPistolSeeds()  { TryBuySeed(FName("Seed_PistolAmmo"), TEXT("Pistol Berry Seed"), 100); }
void UBarnShopWidget::OnBuyMedkitSeeds()  { TryBuySeed(FName("Seed_Medkit"), TEXT("Medicinal Herb Seed"), 250); }

void UBarnShopWidget::OnBuyFarmPlot()
{
	const int32 Price = 300;
	UWorld* World = GetWorld();
	UDimensionShiftSubsystem* Sub = (World && World->GetGameInstance())
		? World->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>()
		: nullptr;
	APawn* Pawn = GetOwningPlayerPawn();
	UTarkovInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<UTarkovInventoryComponent>() : nullptr;

	if (!Sub || !Inv)
	{
		return;
	}

	int32 TargetX = -1;
	int32 TargetY = -1;
	if (!Inv->FindFreeSpace(1, 1, TargetX, TargetY))
	{
		SetStatus(TEXT("Tarkov Backpack is full! Make room before buying a Farm Plot."), FLinearColor(1.0f, 0.35f, 0.30f, 1.0f));
		return;
	}

	if (!Sub->SpendGold(Price))
	{
		SetStatus(TEXT("Not enough Gold! Farm Plots cost 300 Gold."), FLinearColor(1.0f, 0.35f, 0.30f, 1.0f));
		return;
	}

	FInventoryGridItem PlotItem;
	PlotItem.ItemId = FGuid::NewGuid();
	PlotItem.ItemName = TEXT("Farm Plot");
	PlotItem.Width = 1;
	PlotItem.Height = 1;
	PlotItem.AmmoCount = 1;
	PlotItem.GridX = TargetX;
	PlotItem.GridY = TargetY;
	Inv->AddItemAt(PlotItem, TargetX, TargetY);

	RefreshGoldText();
	SetStatus(TEXT("Purchased Farm Plot! Open Inventory [I/Tab] and click it to place!"), FLinearColor(0.35f, 1.0f, 0.45f, 1.0f));
}

void UBarnShopWidget::OnCloseClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
	RemoveFromParent();
}
