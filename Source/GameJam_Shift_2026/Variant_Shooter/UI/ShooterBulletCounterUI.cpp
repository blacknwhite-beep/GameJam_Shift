// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterBulletCounterUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Engine/Texture2D.h"

void UShooterBulletCounterUI::NativeConstruct()
{
	Super::NativeConstruct();
	SetupBloodGoreOverlay();
}

void UShooterBulletCounterUI::SetupBloodGoreOverlay()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!BloodOverlayTexture)
	{
		BloodOverlayTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Dev_Blueprint/UI/T_BloodDamageOverlay.T_BloodDamageOverlay"));
	}

	// Hide any default template orange hit/damage vignette images inside UI_ShooterBulletCounter
	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* W : AllWidgets)
	{
		if (UImage* Img = Cast<UImage>(W))
		{
			if (Img == BloodGoreOverlayImage)
			{
				continue;
			}
			const FString NameStr = Img->GetName();
			if (NameStr.Contains(TEXT("Damage"), ESearchCase::IgnoreCase)
				|| NameStr.Contains(TEXT("Vignette"), ESearchCase::IgnoreCase)
				|| NameStr.Contains(TEXT("Hit"), ESearchCase::IgnoreCase)
				|| NameStr.Contains(TEXT("Border"), ESearchCase::IgnoreCase)
				|| NameStr.Contains(TEXT("Flash"), ESearchCase::IgnoreCase)
				|| NameStr.Contains(TEXT("Outline"), ESearchCase::IgnoreCase))
			{
				Img->SetVisibility(ESlateVisibility::Collapsed);
				Img->SetRenderOpacity(0.0f);
			}
			else
			{
				// Also catch unnamed Image_XX widgets that use an orange tint or fill the screen
				const FLinearColor Tint = Img->GetColorAndOpacity();
				if (Tint.R > 0.65f && Tint.G > 0.15f && Tint.G < 0.65f && Tint.B < 0.25f)
				{
					Img->SetVisibility(ESlateVisibility::Collapsed);
					Img->SetRenderOpacity(0.0f);
				}
			}
		}
	}

	if (!BloodGoreOverlayImage && BloodOverlayTexture)
	{
		BloodGoreOverlayImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BloodGoreDamageFrame"));
		if (BloodGoreOverlayImage)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(BloodOverlayTexture);
			Brush.ImageSize = FVector2D(1024.0f, 576.0f);
			BloodGoreOverlayImage->SetBrush(Brush);
			BloodGoreOverlayImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
			BloodGoreOverlayImage->SetRenderOpacity(0.0f);
			BloodGoreOverlayImage->SetVisibility(ESlateVisibility::HitTestInvisible);

			if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
			{
				if (UCanvasPanelSlot* BloodCanvasSlot = RootCanvas->AddChildToCanvas(BloodGoreOverlayImage))
				{
					BloodCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
					BloodCanvasSlot->SetOffsets(FMargin(0.0f));
					BloodCanvasSlot->SetZOrder(50);
				}
			}
			else if (UOverlay* RootOverlay = Cast<UOverlay>(WidgetTree->RootWidget))
			{
				if (UOverlaySlot* BloodOverlaySlot = RootOverlay->AddChildToOverlay(BloodGoreOverlayImage))
				{
					BloodOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
					BloodOverlaySlot->SetVerticalAlignment(VAlign_Fill);
				}
			}
			else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
			{
				RootPanel->AddChild(BloodGoreOverlayImage);
			}
		}
	}
}

void UShooterBulletCounterUI::TriggerBloodDamageEffect(float LifePercent)
{
	if (!BloodGoreOverlayImage)
	{
		SetupBloodGoreOverlay();
	}

	// Only trigger a strong splatter pulse when health actually dropped (not on spawn or heal)
	if (LifePercent < LastKnownLifePercent - KINDA_SMALL_NUMBER)
	{
		BloodPulseOpacity = 1.0f;
	}
	LastKnownLifePercent = FMath::Clamp(LifePercent, 0.0f, 1.0f);
}

void UShooterBulletCounterUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!BloodGoreOverlayImage)
	{
		return;
	}

	if (BloodPulseOpacity > 0.0f)
	{
		BloodPulseOpacity = FMath::Max(0.0f, BloodPulseOpacity - InDeltaTime * 1.15f);
	}

	// Low-HP persistent crimson gore edge (starts creeping in below 60% HP)
	const float LowHpBase = (LastKnownLifePercent < 0.60f)
		? FMath::Clamp((0.60f - LastKnownLifePercent) / 0.60f, 0.0f, 1.0f) * 0.68f
		: 0.0f;

	const float TargetOpacity = FMath::Clamp(FMath::Max(LowHpBase, BloodPulseOpacity * 0.96f), 0.0f, 0.98f);
	BloodGoreOverlayImage->SetRenderOpacity(TargetOpacity);
}
