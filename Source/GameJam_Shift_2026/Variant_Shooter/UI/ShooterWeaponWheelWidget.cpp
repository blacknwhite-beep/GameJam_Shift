// Copyright GameJam Shift 2026. All Rights Reserved.

#include "UI/ShooterWeaponWheelWidget.h"
#include "ShooterCharacter.h"
#include "Weapons/ShooterWeapon.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"

namespace
{
	constexpr int32 NUM_WHEEL_SLOTS = 7;
	constexpr float WHEEL_RADIUS_PX = 250.0f;
	constexpr float SLOT_CARD_WIDTH = 178.0f;
	constexpr float SLOT_CARD_HEIGHT = 82.0f;

	// Returns unit vector (Cos, Sin) for slot i (0 = top at -90 deg, moving clockwise)
	FVector2D GetSlotDirection(int32 SlotIdx)
	{
		const float AngleDeg = -90.0f + (360.0f / static_cast<float>(NUM_WHEEL_SLOTS)) * static_cast<float>(SlotIdx);
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		return FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
	}
}

void UShooterWeaponWheelWidget::NativeOnInitialized()
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

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WeaponWheelCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	// 1. Subtle Dark Screen Backdrop
	UBorder* ScreenBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WheelBackdrop"));
	ScreenBackdrop->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.025f, 0.62f));
	if (UCanvasPanelSlot* BackSlot = RootCanvas->AddChildToCanvas(ScreenBackdrop))
	{
		BackSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackSlot->SetOffsets(FMargin(0.0f));
	}

	// 2. Top Header Banner
	UBorder* TopHeader = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WheelTopHeader"));
	TopHeader->SetBrushColor(FLinearColor(0.04f, 0.06f, 0.09f, 0.94f));
	TopHeader->SetPadding(FMargin(24.0f, 10.0f));
	if (UCanvasPanelSlot* TopSlot = RootCanvas->AddChildToCanvas(TopHeader))
	{
		TopSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		TopSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		TopSlot->SetPosition(FVector2D(0.0f, 34.0f));
		TopSlot->SetAutoSize(true);
	}

	UTextBlock* TopBannerTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TopBannerTxt"));
	TopBannerTxt->SetText(FText::FromString(TEXT("TACTICAL ARSENAL WHEEL   |   Move Mouse or Press [1]-[7]   |   Release [TAB] to Equip")));
	TopBannerTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.92f, 1.0f, 1.0f)));
	{
		FSlateFontInfo Font = TopBannerTxt->GetFont();
		Font.Size = 11;
		TopBannerTxt->SetFont(Font);
	}
	TopHeader->SetContent(TopBannerTxt);

	// 3. Center Hub Card (Displays active / highlighted weapon details)
	USizeBox* CenterHubSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CenterHubSize"));
	CenterHubSize->SetWidthOverride(226.0f);
	CenterHubSize->SetHeightOverride(118.0f);
	if (UCanvasPanelSlot* HubSlot = RootCanvas->AddChildToCanvas(CenterHubSize))
	{
		HubSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		HubSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		HubSlot->SetPosition(FVector2D(0.0f, 0.0f));
		HubSlot->SetAutoSize(true);
	}

	UBorder* CenterHubBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CenterHubBorder"));
	CenterHubBorder->SetBrushColor(FLinearColor(0.04f, 0.07f, 0.10f, 0.96f));
	CenterHubBorder->SetPadding(FMargin(16.0f, 12.0f));
	CenterHubSize->SetContent(CenterHubBorder);

	UVerticalBox* CenterVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterVBox"));
	CenterHubBorder->SetContent(CenterVBox);

	CenterWeaponTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CenterWeaponTitleText"));
	CenterWeaponTitleText->SetText(FText::FromString(TEXT("M1911 CLASSIC")));
	CenterWeaponTitleText->SetJustification(ETextJustify::Center);
	CenterWeaponTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.90f, 0.35f, 1.0f)));
	{
		FSlateFontInfo Font = CenterWeaponTitleText->GetFont();
		Font.Size = 14;
		CenterWeaponTitleText->SetFont(Font);
	}
	if (UVerticalBoxSlot* VSlot = CenterVBox->AddChildToVerticalBox(CenterWeaponTitleText))
	{
		VSlot->SetHorizontalAlignment(HAlign_Center);
		VSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 4.0f));
	}

	CenterWeaponSubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CenterWeaponSubtitleText"));
	CenterWeaponSubtitleText->SetText(FText::FromString(TEXT("9mm Semi-Auto Pistol")));
	CenterWeaponSubtitleText->SetJustification(ETextJustify::Center);
	CenterWeaponSubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.84f, 0.94f, 1.0f)));
	{
		FSlateFontInfo Font = CenterWeaponSubtitleText->GetFont();
		Font.Size = 10;
		CenterWeaponSubtitleText->SetFont(Font);
	}
	if (UVerticalBoxSlot* VSlot = CenterVBox->AddChildToVerticalBox(CenterWeaponSubtitleText))
	{
		VSlot->SetHorizontalAlignment(HAlign_Center);
		VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	CenterWeaponAmmoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CenterWeaponAmmoText"));
	CenterWeaponAmmoText->SetText(FText::FromString(TEXT("MAG: 12 / 12   |   BAG: 0")));
	CenterWeaponAmmoText->SetJustification(ETextJustify::Center);
	CenterWeaponAmmoText->SetColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.98f, 0.58f, 1.0f)));
	{
		FSlateFontInfo Font = CenterWeaponAmmoText->GetFont();
		Font.Size = 11;
		CenterWeaponAmmoText->SetFont(Font);
	}
	if (UVerticalBoxSlot* VSlot = CenterVBox->AddChildToVerticalBox(CenterWeaponAmmoText))
	{
		VSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// 4. Create the 7 Radial Weapon Slot Cards arranged in a circle around (0.5, 0.5)
	SlotCardBorders.Reset();
	SlotKeyBadges.Reset();
	SlotNameTexts.Reset();
	SlotSubtitleTexts.Reset();
	SlotAmmoTexts.Reset();

	for (int32 i = 0; i < NUM_WHEEL_SLOTS; ++i)
	{
		const FVector2D Dir = GetSlotDirection(i);
		const FVector2D OffsetPos = Dir * WHEEL_RADIUS_PX;

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("WheelSlotSize_%d"), i));
		CardSize->SetWidthOverride(SLOT_CARD_WIDTH);
		CardSize->SetHeightOverride(SLOT_CARD_HEIGHT);
		if (UCanvasPanelSlot* CSlot = RootCanvas->AddChildToCanvas(CardSize))
		{
			CSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CSlot->SetPosition(OffsetPos);
			CSlot->SetAutoSize(true);
		}

		UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("WheelCardBorder_%d"), i));
		CardBorder->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.11f, 0.94f));
		CardBorder->SetPadding(FMargin(10.0f, 8.0f));
		CardSize->SetContent(CardBorder);

		UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("WheelCardVBox_%d"), i));
		CardBorder->SetContent(CardVBox);

		// Top Row: Key Badge [1..7] + Weapon Name
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("WheelHeaderRow_%d"), i));
		if (UVerticalBoxSlot* HRowSlot = CardVBox->AddChildToVerticalBox(HeaderRow))
		{
			HRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}

		UBorder* KeyBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("WheelKeyBadge_%d"), i));
		KeyBadge->SetBrushColor(FLinearColor(0.16f, 0.22f, 0.30f, 1.0f));
		KeyBadge->SetPadding(FMargin(6.0f, 2.0f));
		if (UHorizontalBoxSlot* KBSlot = HeaderRow->AddChildToHorizontalBox(KeyBadge))
		{
			KBSlot->SetVerticalAlignment(VAlign_Center);
			KBSlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 0.0f));
		}

		UTextBlock* KeyTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("WheelKeyTxt_%d"), i));
		KeyTxt->SetText(FText::FromString(FString::Printf(TEXT("%d"), i + 1)));
		KeyTxt->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.92f, 0.45f, 1.0f)));
		{
			FSlateFontInfo Font = KeyTxt->GetFont();
			Font.Size = 10;
			KeyTxt->SetFont(Font);
		}
		KeyBadge->SetContent(KeyTxt);

		UTextBlock* WepNameTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("WheelWepName_%d"), i));
		WepNameTxt->SetText(FText::FromString(FString::Printf(TEXT("WEAPON %d"), i + 1)));
		WepNameTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.97f, 1.0f, 1.0f)));
		{
			FSlateFontInfo Font = WepNameTxt->GetFont();
			Font.Size = 11;
			WepNameTxt->SetFont(Font);
		}
		if (UHorizontalBoxSlot* WNSlot = HeaderRow->AddChildToHorizontalBox(WepNameTxt))
		{
			WNSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Middle Row: Caliber / Category Subtitle
		UTextBlock* SubTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("WheelSubTxt_%d"), i));
		SubTxt->SetText(FText::FromString(TEXT("9mm Pistol Ammo")));
		SubTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.75f, 0.85f, 1.0f)));
		{
			FSlateFontInfo Font = SubTxt->GetFont();
			Font.Size = 9;
			SubTxt->SetFont(Font);
		}
		if (UVerticalBoxSlot* SubSlot = CardVBox->AddChildToVerticalBox(SubTxt))
		{
			SubSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		// Bottom Row: Magazine & Reserve Ammo Count
		UTextBlock* AmmoTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("WheelAmmoTxt_%d"), i));
		AmmoTxt->SetText(FText::FromString(TEXT("12 / 12  (Bag: 0)")));
		AmmoTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.92f, 0.60f, 1.0f)));
		{
			FSlateFontInfo Font = AmmoTxt->GetFont();
			Font.Size = 9;
			AmmoTxt->SetFont(Font);
		}
		CardVBox->AddChildToVerticalBox(AmmoTxt);

		SlotCardBorders.Add(CardBorder);
		SlotKeyBadges.Add(KeyBadge);
		SlotNameTexts.Add(WepNameTxt);
		SlotSubtitleTexts.Add(SubTxt);
		SlotAmmoTexts.Add(AmmoTxt);
	}
}

void UShooterWeaponWheelWidget::OpenWheel(AShooterCharacter* InOwnerCharacter)
{
	OwnerCharacter = InOwnerCharacter;
	SetVisibility(ESlateVisibility::Visible);

	// Initialize highlighted slot to the currently equipped weapon
	HoveredSlotIndex = 0;
	if (OwnerCharacter)
	{
		const TArray<AShooterWeapon*>& Weapons = OwnerCharacter->GetOwnedWeapons();
		const int32 FoundIdx = Weapons.Find(OwnerCharacter->GetCurrentWeapon());
		if (FoundIdx != INDEX_NONE)
		{
			HoveredSlotIndex = FoundIdx;
		}
	}

	bHasCapturedCenter = false;
	if (APlayerController* PC = GetOwningPlayer())
	{
		int32 ViewX = 0;
		int32 ViewY = 0;
		PC->GetViewportSize(ViewX, ViewY);
		if (ViewX > 0 && ViewY > 0)
		{
			PC->SetMouseLocation(ViewX / 2, ViewY / 2);
			WheelCenterMousePos = FVector2D(static_cast<float>(ViewX) * 0.5f, static_cast<float>(ViewY) * 0.5f);
			bHasCapturedCenter = true;
		}
		PC->SetShowMouseCursor(true);
		PC->SetIgnoreLookInput(true);
	}

	RefreshWheelSlots();
}

void UShooterWeaponWheelWidget::CloseWheelAndCommit()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
		PC->ResetIgnoreLookInput();
	}

	if (OwnerCharacter && HoveredSlotIndex >= 0)
	{
		OwnerCharacter->EquipWeaponByIndex(HoveredSlotIndex);
	}

	SetVisibility(ESlateVisibility::Hidden);
}

void UShooterWeaponWheelWidget::SelectSlotByIndex(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= NUM_WHEEL_SLOTS)
	{
		return;
	}

	HoveredSlotIndex = SlotIndex;
	if (OwnerCharacter)
	{
		OwnerCharacter->EquipWeaponByIndex(SlotIndex);
	}
	RefreshWheelSlots();
}

void UShooterWeaponWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetVisibility() != ESlateVisibility::Visible)
	{
		return;
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			if (!bHasCapturedCenter)
			{
				int32 ViewX = 0;
				int32 ViewY = 0;
				PC->GetViewportSize(ViewX, ViewY);
				WheelCenterMousePos = (ViewX > 0 && ViewY > 0)
					? FVector2D(ViewX * 0.5f, ViewY * 0.5f)
					: FVector2D(MouseX, MouseY);
				bHasCapturedCenter = true;
			}

			const FVector2D Delta(MouseX - WheelCenterMousePos.X, MouseY - WheelCenterMousePos.Y);
			const float DistSq = Delta.SizeSquared();

			// Once cursor moves outside the deadzone (28px), pick the closest radial sector (0..6)
			if (DistSq >= 28.0f * 28.0f)
			{
				const FVector2D NormDelta = Delta.GetSafeNormal();
				int32 BestSlot = HoveredSlotIndex;
				float BestDot = -2.0f;

				for (int32 i = 0; i < NUM_WHEEL_SLOTS; ++i)
				{
					const float Dot = FVector2D::DotProduct(NormDelta, GetSlotDirection(i));
					if (Dot > BestDot)
					{
						BestDot = Dot;
						BestSlot = i;
					}
				}

				if (BestSlot != HoveredSlotIndex)
				{
					HoveredSlotIndex = BestSlot;
				}
			}
		}
	}

	RefreshWheelSlots();
}

void UShooterWeaponWheelWidget::RefreshWheelSlots()
{
	if (!OwnerCharacter)
	{
		return;
	}

	const TArray<AShooterWeapon*>& Weapons = OwnerCharacter->GetOwnedWeapons();
	const AShooterWeapon* EquippedWep = OwnerCharacter->GetCurrentWeapon();

	for (int32 i = 0; i < NUM_WHEEL_SLOTS; ++i)
	{
		AShooterWeapon* Wep = Weapons.IsValidIndex(i) ? Weapons[i] : nullptr;
		const bool bIsHovered = (i == HoveredSlotIndex);
		const bool bIsEquipped = (Wep && Wep == EquippedWep);

		if (SlotCardBorders.IsValidIndex(i) && SlotCardBorders[i])
		{
			if (bIsHovered)
			{
				SlotCardBorders[i]->SetBrushColor(FLinearColor(0.14f, 0.36f, 0.56f, 0.98f));
			}
			else if (bIsEquipped)
			{
				SlotCardBorders[i]->SetBrushColor(FLinearColor(0.10f, 0.24f, 0.18f, 0.95f));
			}
			else
			{
				SlotCardBorders[i]->SetBrushColor(FLinearColor(0.05f, 0.07f, 0.10f, 0.92f));
			}
		}

		if (SlotKeyBadges.IsValidIndex(i) && SlotKeyBadges[i])
		{
			SlotKeyBadges[i]->SetBrushColor(
				bIsHovered
				? FLinearColor(0.95f, 0.72f, 0.15f, 1.0f)
				: FLinearColor(0.15f, 0.20f, 0.28f, 1.0f)
			);
		}

		if (Wep)
		{
			const int32 CurBullets = Wep->GetBulletCount();
			const int32 MagSize = Wep->GetMagazineSize();
			const int32 BagAmmo = Wep->GetAvailableInventoryAmmo();

			if (SlotNameTexts.IsValidIndex(i) && SlotNameTexts[i])
			{
				SlotNameTexts[i]->SetText(FText::FromString(Wep->GetWeaponDisplayName().ToUpper()));
				SlotNameTexts[i]->SetColorAndOpacity(FSlateColor(
					bIsHovered
					? FLinearColor(1.0f, 0.95f, 0.45f, 1.0f)
					: FLinearColor(0.92f, 0.95f, 0.98f, 1.0f)
				));
			}
			if (SlotSubtitleTexts.IsValidIndex(i) && SlotSubtitleTexts[i])
			{
				SlotSubtitleTexts[i]->SetText(FText::FromString(Wep->GetRequiredAmmoDisplayName()));
			}
			if (SlotAmmoTexts.IsValidIndex(i) && SlotAmmoTexts[i])
			{
				SlotAmmoTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("MAG: %d/%d   |   BAG: %d"), CurBullets, MagSize, BagAmmo)));
			}

			if (bIsHovered)
			{
				if (CenterWeaponTitleText)
				{
					CenterWeaponTitleText->SetText(FText::FromString(FString::Printf(TEXT("[%d] %s"), i + 1, *Wep->GetWeaponDisplayName().ToUpper())));
				}
				if (CenterWeaponSubtitleText)
				{
					CenterWeaponSubtitleText->SetText(FText::FromString(FString::Printf(TEXT("%s  (%s)"), *Wep->GetWeaponSubtitle(), *Wep->GetRequiredAmmoDisplayName())));
				}
				if (CenterWeaponAmmoText)
				{
					CenterWeaponAmmoText->SetText(FText::FromString(FString::Printf(TEXT("MAG: %d / %d   |   BACKPACK: %d"), CurBullets, MagSize, BagAmmo)));
				}
			}
		}
	}
}

int32 UShooterWeaponWheelWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center = LocalSize * 0.5f;

	// Draw radial spokes from the center ring to each of the 7 weapon slots
	for (int32 i = 0; i < NUM_WHEEL_SLOTS; ++i)
	{
		const bool bIsHovered = (i == HoveredSlotIndex);
		const FVector2D Dir = GetSlotDirection(i);
		const FVector2D StartPt = Center + Dir * 72.0f;
		const FVector2D EndPt = Center + Dir * (WHEEL_RADIUS_PX - 55.0f);

		TArray<FVector2D> LinePts;
		LinePts.Add(StartPt);
		LinePts.Add(EndPt);

		const FLinearColor LineColor = bIsHovered
			? FLinearColor(1.0f, 0.85f, 0.25f, 0.95f)
			: FLinearColor(0.25f, 0.42f, 0.58f, 0.38f);
		const float Thickness = bIsHovered ? 3.5f : 1.5f;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MaxLayer + 1,
			AllottedGeometry.ToPaintGeometry(),
			LinePts,
			ESlateDrawEffect::None,
			LineColor,
			true,
			Thickness
		);
	}

	return MaxLayer + 2;
}
