// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Farming/BarnShopWidget.h"
#include "Farming/FarmingComponent.h"
#include "DimensionShiftSubsystem.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "ShooterCharacter.h"
#include "Weapons/ShooterWeapon.h"
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
		UButton*& OutButton,
		UTextBlock*& OutBtnLabel)
	{
		UBorder* RowBg = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("RowBg_%s"), *RowId));
		RowBg->SetBrushColor(FLinearColor(0.08f, 0.10f, 0.12f, 0.96f));
		RowBg->SetPadding(FMargin(14.0f, 8.0f));
		if (UVerticalBoxSlot* VSlot = ParentBox->AddChildToVerticalBox(RowBg))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
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
		TitleFont.Size = 12;
		TitleTxt->SetFont(TitleFont);
		InfoCol->AddChildToVerticalBox(TitleTxt);

		UTextBlock* DescTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("RowDesc_%s"), *RowId));
		DescTxt->SetText(FText::FromString(Desc));
		DescTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.78f, 0.72f, 1.0f)));
		FSlateFontInfo DescFont = DescTxt->GetFont();
		DescFont.Size = 9;
		DescTxt->SetFont(DescFont);
		InfoCol->AddChildToVerticalBox(DescTxt);

		USizeBox* BtnSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("RowBtnSize_%s"), *RowId));
		BtnSize->SetWidthOverride(148.0f);
		BtnSize->SetHeightOverride(36.0f);
		if (UHorizontalBoxSlot* BtnSlot = HBox->AddChildToHorizontalBox(BtnSize))
		{
			BtnSlot->SetVerticalAlignment(VAlign_Center);
			BtnSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
		}

		OutButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("BuyBtn_%s"), *RowId));
		OutButton->SetBackgroundColor(FLinearColor(0.20f, 0.58f, 0.28f, 1.0f));
		BtnSize->SetContent(OutButton);

		OutBtnLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("BtnTxt_%s"), *RowId));
		OutBtnLabel->SetText(FText::FromString(Price > 0 ? FString::Printf(TEXT("BUY (%d G)"), Price) : TEXT("OWNED (START)")));
		OutBtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.65f, 1.0f)));
		FSlateFontInfo BtnFont = OutBtnLabel->GetFont();
		BtnFont.Size = 10;
		OutBtnLabel->SetFont(BtnFont);
		OutButton->SetContent(OutBtnLabel);
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
	Panel->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.06f, 0.97f));
	Panel->SetPadding(FMargin(24.0f, 18.0f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D(0.0f, 0.0f));
		PanelSlot->SetSize(FVector2D(650.0f, 595.0f));
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopVBox"));
	Panel->SetContent(VBox);

	// 1. Header Row (Title + Gold Balance)
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopHeader"));
	if (UVerticalBoxSlot* HSlot = VBox->AddChildToVerticalBox(Header))
	{
		HSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTitle"));
	Title->SetText(FText::FromString(TEXT("BARN SUPPLY & ARSENAL STORE")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.45f, 1.0f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 17;
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

	// 2. Navigation Tab Bar: [SEEDS & LAND] vs [GUNS ARSENAL]
	UHorizontalBox* TabBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopTabBar"));
	if (UVerticalBoxSlot* TBSlot = VBox->AddChildToVerticalBox(TabBar))
	{
		TBSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	TabSeedsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TabSeedsButton"));
	TabSeedsButton->SetBackgroundColor(FLinearColor(0.20f, 0.55f, 0.30f, 1.0f));
	TabSeedsButton->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnTabSeedsClicked);
	if (UHorizontalBoxSlot* Tab1Slot = TabBar->AddChildToHorizontalBox(TabSeedsButton))
	{
		Tab1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Tab1Slot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}

	UTextBlock* TabSeedsLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TabSeedsLabel"));
	TabSeedsLabel->SetText(FText::FromString(TEXT("SEEDS & FARM PLOTS")));
	TabSeedsLabel->SetJustification(ETextJustify::Center);
	TabSeedsLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo F = TabSeedsLabel->GetFont();
		F.Size = 12;
		TabSeedsLabel->SetFont(F);
	}
	TabSeedsButton->SetContent(TabSeedsLabel);

	TabGunsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TabGunsButton"));
	TabGunsButton->SetBackgroundColor(FLinearColor(0.12f, 0.15f, 0.20f, 1.0f));
	TabGunsButton->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnTabGunsClicked);
	if (UHorizontalBoxSlot* Tab2Slot = TabBar->AddChildToHorizontalBox(TabGunsButton))
	{
		Tab2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Tab2Slot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	}

	UTextBlock* TabGunsLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TabGunsLabel"));
	TabGunsLabel->SetText(FText::FromString(TEXT("GUNS ARSENAL (7 WEAPONS)")));
	TabGunsLabel->SetJustification(ETextJustify::Center);
	TabGunsLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.90f, 0.45f, 1.0f)));
	{
		FSlateFontInfo F = TabGunsLabel->GetFont();
		F.Size = 12;
		TabGunsLabel->SetFont(F);
	}
	TabGunsButton->SetContent(TabGunsLabel);

	// 3A. SEEDS & LAND Container
	SeedsTabBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SeedsTabBox"));
	if (UVerticalBoxSlot* SeedsSlot = VBox->AddChildToVerticalBox(SeedsTabBox))
	{
		SeedsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UButton* BtnShotgun = nullptr;
	UButton* BtnRifle = nullptr;
	UButton* BtnPistol = nullptr;
	UButton* BtnMedkit = nullptr;
	UButton* BtnPlot = nullptr;
	UTextBlock* DummyLabel = nullptr;

	AddShopRow(WidgetTree, SeedsTabBox, TEXT("Pistol"), TEXT("Pistol Berry Seeds"), TEXT("Harvests +25x 9mm Pistol Ammo (cycles with [T])"), 100, BtnPistol, DummyLabel);
	AddShopRow(WidgetTree, SeedsTabBox, TEXT("Shotgun"), TEXT("Shotgun Corn Seeds"), TEXT("Harvests +16x 12G Shotgun Shells (cycles with [T])"), 150, BtnShotgun, DummyLabel);
	AddShopRow(WidgetTree, SeedsTabBox, TEXT("Rifle"), TEXT("Rifle Wheat Seeds"), TEXT("Harvests +30x 5.56mm Rifle Ammo (cycles with [T])"), 200, BtnRifle, DummyLabel);
	AddShopRow(WidgetTree, SeedsTabBox, TEXT("Medkit"), TEXT("Medicinal Herb Seeds"), TEXT("Harvests +1x Combat Medkit (+50 HP on [H])"), 250, BtnMedkit, DummyLabel);
	AddShopRow(WidgetTree, SeedsTabBox, TEXT("Plot"), TEXT("Expandable Farm Plot"), TEXT("Adds 1x1 Farm Plot to Backpack -> click in Inventory to place!"), 300, BtnPlot, DummyLabel);

	if (BtnPistol)  { BtnPistol->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyPistolSeeds); }
	if (BtnShotgun) { BtnShotgun->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyShotgunSeeds); }
	if (BtnRifle)   { BtnRifle->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyRifleSeeds); }
	if (BtnMedkit)  { BtnMedkit->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyMedkitSeeds); }
	if (BtnPlot)    { BtnPlot->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyFarmPlot); }

	// 3B. GUNS ARSENAL Container (7 Weapons, Slot 0 owned by default, Slots 1..6 increasingly expensive from 1000G)
	GunsTabBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GunsTabBox"));
	GunsTabBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* GunsSlot = VBox->AddChildToVerticalBox(GunsTabBox))
	{
		GunsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	struct FGunShopEntry
	{
		const TCHAR* Id;
		const TCHAR* Title;
		const TCHAR* Desc;
		int32 Price;
	};

	const FGunShopEntry GunEntries[7] = {
		{ TEXT("Gun0"), TEXT("[1] M1911 Classic Pistol"),     TEXT("12-Round Semi-Auto Sidearm  |  Uses 9mm Pistol Ammo"), 0 },
		{ TEXT("Gun1"), TEXT("[2] M1911 Tactical Pistol"),    TEXT("15-Round Match Trigger Sidearm  |  Uses 9mm Pistol Ammo"), 1000 },
		{ TEXT("Gun2"), TEXT("[3] MP5 Tactical SMG"),         TEXT("30-Round Full-Auto Submachine Gun  |  Uses 9mm Pistol Ammo"), 1800 },
		{ TEXT("Gun3"), TEXT("[4] MP7 Personal Defense SMG"), TEXT("40-Round High-ROF Compact PDW  |  Uses 9mm Pistol Ammo"), 2600 },
		{ TEXT("Gun4"), TEXT("[5] M4A1 Assault Carbine"),     TEXT("30-Round Full-Auto Carbine  |  Uses 5.56mm Rifle Ammo"), 3500 },
		{ TEXT("Gun5"), TEXT("[6] M16A4 Service Rifle"),      TEXT("30-Round Precision Assault Rifle  |  Uses 5.56mm Rifle Ammo"), 4500 },
		{ TEXT("Gun6"), TEXT("[7] FN SCAR-H Battle Rifle"),   TEXT("20-Round Heavy Full-Auto Battle Rifle  |  Uses 5.56mm Rifle Ammo"), 6000 }
	};

	GunBuyButtons.Reset();
	GunBuyButtonLabels.Reset();

	for (int32 i = 0; i < 7; ++i)
	{
		UButton* GunBtn = nullptr;
		UTextBlock* GunLbl = nullptr;
		AddShopRow(WidgetTree, GunsTabBox, GunEntries[i].Id, GunEntries[i].Title, GunEntries[i].Desc, GunEntries[i].Price, GunBtn, GunLbl);
		GunBuyButtons.Add(GunBtn);
		GunBuyButtonLabels.Add(GunLbl);
	}

	if (GunBuyButtons.IsValidIndex(1) && GunBuyButtons[1]) { GunBuyButtons[1]->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyGunSlot1); }
	if (GunBuyButtons.IsValidIndex(2) && GunBuyButtons[2]) { GunBuyButtons[2]->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyGunSlot2); }
	if (GunBuyButtons.IsValidIndex(3) && GunBuyButtons[3]) { GunBuyButtons[3]->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyGunSlot3); }
	if (GunBuyButtons.IsValidIndex(4) && GunBuyButtons[4]) { GunBuyButtons[4]->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyGunSlot4); }
	if (GunBuyButtons.IsValidIndex(5) && GunBuyButtons[5]) { GunBuyButtons[5]->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyGunSlot5); }
	if (GunBuyButtons.IsValidIndex(6) && GunBuyButtons[6]) { GunBuyButtons[6]->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnBuyGunSlot6); }

	// 4. Footer Status Text & Close Button
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopStatusText"));
	StatusText->SetText(FText::FromString(TEXT("Switch between SEEDS & FARM PLOTS and GUNS ARSENAL tabs above.")));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.88f, 0.80f, 1.0f)));
	FSlateFontInfo StatFont = StatusText->GetFont();
	StatFont.Size = 10;
	StatusText->SetFont(StatFont);
	if (UVerticalBoxSlot* SSlot = VBox->AddChildToVerticalBox(StatusText))
	{
		SSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 8.0f));
	}

	UButton* CloseBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopCloseBtn"));
	CloseBtn->SetBackgroundColor(FLinearColor(0.45f, 0.16f, 0.16f, 1.0f));
	CloseBtn->OnClicked.AddUniqueDynamic(this, &UBarnShopWidget::OnCloseClicked);
	VBox->AddChildToVerticalBox(CloseBtn);

	UTextBlock* CloseTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopCloseTxt"));
	CloseTxt->SetText(FText::FromString(TEXT("CLOSE SHOP [ESC / B]")));
	CloseTxt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo CloseFont = CloseTxt->GetFont();
	CloseFont.Size = 11;
	CloseTxt->SetFont(CloseFont);
	CloseBtn->SetContent(CloseTxt);
}

void UBarnShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshGoldText();
	RefreshGunsTabUI();
}

void UBarnShopWidget::OnTabSeedsClicked()
{
	SetActiveTab(0);
}

void UBarnShopWidget::OnTabGunsClicked()
{
	SetActiveTab(1);
}

void UBarnShopWidget::SetActiveTab(int32 TabIndex)
{
	const bool bSeedsActive = (TabIndex == 0);
	if (SeedsTabBox)
	{
		SeedsTabBox->SetVisibility(bSeedsActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (GunsTabBox)
	{
		GunsTabBox->SetVisibility(bSeedsActive ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (TabSeedsButton)
	{
		TabSeedsButton->SetBackgroundColor(bSeedsActive ? FLinearColor(0.20f, 0.55f, 0.30f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.20f, 1.0f));
	}
	if (TabGunsButton)
	{
		TabGunsButton->SetBackgroundColor(!bSeedsActive ? FLinearColor(0.20f, 0.48f, 0.68f, 1.0f) : FLinearColor(0.12f, 0.15f, 0.20f, 1.0f));
	}

	RefreshGoldText();
	RefreshGunsTabUI();

	if (bSeedsActive)
	{
		SetStatus(TEXT("Purchase seeds to add them to your [T] pouch rotation."), FLinearColor(0.75f, 0.88f, 0.80f, 1.0f));
	}
	else
	{
		SetStatus(TEXT("Unlock weapons to equip them in the [Hold TAB] Radial Weapon Wheel in Combat!"), FLinearColor(0.75f, 0.88f, 1.0f, 1.0f));
	}
}

void UBarnShopWidget::RefreshGunsTabUI()
{
	UWorld* World = GetWorld();
	UDimensionShiftSubsystem* Sub = (World && World->GetGameInstance())
		? World->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>()
		: nullptr;

	for (int32 i = 0; i < 7; ++i)
	{
		const bool bUnlocked = (i == 0) || (Sub && Sub->IsWeaponUnlocked(i));
		const int32 Price = AShooterCharacter::GetWeaponSlotPrice(i);

		if (GunBuyButtons.IsValidIndex(i) && GunBuyButtons[i])
		{
			GunBuyButtons[i]->SetBackgroundColor(
				bUnlocked
				? FLinearColor(0.16f, 0.36f, 0.48f, 1.0f)
				: FLinearColor(0.20f, 0.58f, 0.28f, 1.0f)
			);
		}
		if (GunBuyButtonLabels.IsValidIndex(i) && GunBuyButtonLabels[i])
		{
			GunBuyButtonLabels[i]->SetText(FText::FromString(
				bUnlocked
				? TEXT("OWNED")
				: FString::Printf(TEXT("BUY (%d G)"), Price)
			));
			GunBuyButtonLabels[i]->SetColorAndOpacity(FSlateColor(
				bUnlocked
				? FLinearColor(0.55f, 0.95f, 1.0f, 1.0f)
				: FLinearColor(1.0f, 0.95f, 0.65f, 1.0f)
			));
		}
	}
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

void UBarnShopWidget::TryBuyOrEquipGun(int32 WeaponSlotIndex, const FString& DisplayName, int32 Price)
{
	UWorld* World = GetWorld();
	UDimensionShiftSubsystem* Sub = (World && World->GetGameInstance())
		? World->GetGameInstance()->GetSubsystem<UDimensionShiftSubsystem>()
		: nullptr;
	AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(GetOwningPlayerPawn());

	if (!Sub)
	{
		return;
	}

	if (Sub->IsWeaponUnlocked(WeaponSlotIndex))
	{
		SetStatus(
			FString::Printf(TEXT("%s is already unlocked! Hold [TAB] or press [%d] in Combat to equip it."), *DisplayName, WeaponSlotIndex + 1),
			FLinearColor(0.55f, 0.92f, 1.0f, 1.0f)
		);
		return;
	}

	if (!Sub->SpendGold(Price))
	{
		SetStatus(
			FString::Printf(TEXT("Need %d Gold to buy %s! Eliminate zombies in Combat (+100G each)."), Price, *DisplayName),
			FLinearColor(1.0f, 0.35f, 0.30f, 1.0f)
		);
		return;
	}

	Sub->UnlockWeapon(WeaponSlotIndex);
	RefreshGoldText();
	RefreshGunsTabUI();

	if (ShooterChar)
	{
		// Update Tarkov Inventory EquippedWeaponItem so when player returns to Combat, their newly purchased gun is ready
		const TArray<AShooterWeapon*>& Weapons = ShooterChar->GetOwnedWeapons();
		if (Weapons.IsValidIndex(WeaponSlotIndex) && Weapons[WeaponSlotIndex])
		{
			if (UTarkovInventoryComponent* Inv = ShooterChar->FindComponentByClass<UTarkovInventoryComponent>())
			{
				Inv->bHasEquippedWeapon = true;
				Inv->EquippedWeaponItem.ItemName = Weapons[WeaponSlotIndex]->GetWeaponDisplayName();
				Inv->EquippedWeaponItem.WeaponClass = Weapons[WeaponSlotIndex]->GetClass();
				Inv->EquippedWeaponItem.AmmoCount = Weapons[WeaponSlotIndex]->GetBulletCount();
				Inv->OnInventoryUpdated.Broadcast();
			}
		}
	}

	SetStatus(
		FString::Printf(TEXT("Unlocked %s! Hold [TAB] or press [%d] in Combat to use it!"), *DisplayName, WeaponSlotIndex + 1),
		FLinearColor(0.35f, 1.0f, 0.45f, 1.0f)
	);
}

void UBarnShopWidget::OnBuyShotgunSeeds() { TryBuySeed(FName("Seed_ShotgunAmmo"), TEXT("Shotgun Corn Seed"), 150); }
void UBarnShopWidget::OnBuyRifleSeeds()   { TryBuySeed(FName("Seed_RifleAmmo"), TEXT("Rifle Wheat Seed"), 200); }
void UBarnShopWidget::OnBuyPistolSeeds()  { TryBuySeed(FName("Seed_PistolAmmo"), TEXT("Pistol Berry Seed"), 100); }
void UBarnShopWidget::OnBuyMedkitSeeds()  { TryBuySeed(FName("Seed_Medkit"), TEXT("Medicinal Herb Seed"), 250); }

void UBarnShopWidget::OnBuyGunSlot1() { TryBuyOrEquipGun(1, TEXT("M1911 Tactical Pistol"), 1000); }
void UBarnShopWidget::OnBuyGunSlot2() { TryBuyOrEquipGun(2, TEXT("MP5 Tactical SMG"), 1800); }
void UBarnShopWidget::OnBuyGunSlot3() { TryBuyOrEquipGun(3, TEXT("MP7 Personal Defense SMG"), 2600); }
void UBarnShopWidget::OnBuyGunSlot4() { TryBuyOrEquipGun(4, TEXT("M4A1 Assault Carbine"), 3500); }
void UBarnShopWidget::OnBuyGunSlot5() { TryBuyOrEquipGun(5, TEXT("M16A4 Service Rifle"), 4500); }
void UBarnShopWidget::OnBuyGunSlot6() { TryBuyOrEquipGun(6, TEXT("FN SCAR-H Battle Rifle"), 6000); }

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
