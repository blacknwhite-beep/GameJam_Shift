// Copyright Epic Games, Inc. All Rights Reserved.
// Tarkov-Style Grid Inventory UMG Native Base Implementation (fw-qwen)

#include "TarkovInventoryWidgets.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

// ============================================================================
// UTarkovInventoryItemWidget (20x20px item widget)
// ============================================================================

void UTarkovInventoryItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UTarkovInventoryItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	SetFocus();
}

void UTarkovInventoryItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
}

FReply UTarkovInventoryItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTarkovInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (PickupSound)
	{
		UGameplayStatics::PlaySound2D(this, PickupSound);
	}
	else if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		if (UTarkovInventoryComponent* Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>())
		{
			if (Comp->SoundItemPickup)
			{
				UGameplayStatics::PlaySound2D(this, Comp->SoundItemPickup);
			}
		}
	}

	UTarkovItemDragDropOp* DragOp = NewObject<UTarkovItemDragDropOp>(this);
	DragOp->ItemId = ItemId;
	DragOp->SourceSlot = SlotSource;
	DragOp->OriginX = GridX;
	DragOp->OriginY = GridY;
	DragOp->Width = CellWidth;
	DragOp->Height = CellHeight;
	DragOp->DefaultDragVisual = this;
	DragOp->Pivot = EDragPivot::CenterCenter;

	OutOperation = DragOp;
}

FReply UTarkovInventoryItemWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::R)
	{
		if (APawn* Pawn = GetOwningPlayerPawn())
		{
			if (UTarkovInventoryComponent* Comp = Pawn->FindComponentByClass<UTarkovInventoryComponent>())
			{
				if (Comp->RotateItem(ItemId))
				{
					return FReply::Handled();
				}
			}
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================================
// UTarkovBackpackGridWidget (120x120px, 6x6 cells of 20x20px)
// ============================================================================

bool UTarkovBackpackGridWidget::PixelToCell(const FVector2D& LocalPos, int32& OutCol, int32& OutRow) const
{
	OutCol = FMath::FloorToInt(LocalPos.X / DisplayCellSize);
	OutRow = FMath::FloorToInt(LocalPos.Y / DisplayCellSize);
	return (OutCol >= 0 && OutCol < GridColumns && OutRow >= 0 && OutRow < GridRows);
}

void UTarkovBackpackGridWidget::EnsureCellGrid()
{
	if (CellBorders.Num() == GridColumns * GridRows)
	{
		return; // already created
	}

	// If the Blueprint already provides named cell borders, use them.
	UBorder* Existing = GetCellBorder(0, 0);
	if (Existing)
	{
		CellBorders.Reset();
		for (int32 Row = 0; Row < GridRows; ++Row)
		{
			for (int32 Col = 0; Col < GridColumns; ++Col)
			{
				CellBorders.Add(GetCellBorder(Col, Row));
			}
		}
		return;
	}

	// Otherwise create the cell grid in C++.
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Root;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root)
	{
		return;
	}

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GridBackground"));
	Background->SetBrushColor(GridLineColor);
	UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background);
	BackgroundSlot->SetPosition(FVector2D::ZeroVector);
	BackgroundSlot->SetSize(FVector2D(DisplayTotalGridSize, DisplayTotalGridSize));

	CellBorders.Reset();
	for (int32 Row = 0; Row < GridRows; ++Row)
	{
		for (int32 Col = 0; Col < GridColumns; ++Col)
		{
			UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("Cell_%d_%d"), Row, Col));
			Cell->SetBrushColor(DefaultCellColor);
			UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Cell);
			CanvasSlot->SetPosition(FVector2D(Col * DisplayCellSize + CellInset, Row * DisplayCellSize + CellInset));
			CanvasSlot->SetSize(FVector2D(DisplayCellSize - 2.0f * CellInset, DisplayCellSize - 2.0f * CellInset));
			CellBorders.Add(Cell);
		}
	}
}

void UTarkovBackpackGridWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureCellGrid();
}

void UTarkovBackpackGridWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CellBorders.Num() == 0)
	{
		EnsureCellGrid();
	}
}

UBorder* UTarkovBackpackGridWidget::GetCellBorder(int32 Col, int32 Row) const
{
	if (Col < 0 || Col >= GridColumns || Row < 0 || Row >= GridRows)
	{
		return nullptr;
	}

	// Check C++-created borders first
	int32 Index = Row * GridColumns + Col;
	if (CellBorders.IsValidIndex(Index) && CellBorders[Index])
	{
		return CellBorders[Index];
	}

	// Fall back to named widget lookup (Blueprint-created)
	return Cast<UBorder>(GetWidgetFromName(FName(*FString::Printf(TEXT("Cell_%d_%d"), Row, Col))));
}

void UTarkovBackpackGridWidget::HighlightCells(int32 Col, int32 Row, int32 W, int32 H, const FLinearColor& Color)
{
	for (const FIntPoint& Cell : HighlightedCells)
	{
		if (UBorder* Border = GetCellBorder(Cell.X, Cell.Y))
		{
			Border->SetBrushColor(DefaultCellColor);
		}
	}
	HighlightedCells.Reset();

	if (Col < 0 || Row < 0)
	{
		return;
	}

	for (int32 Y = Row; Y < FMath::Min(Row + H, GridRows); ++Y)
	{
		for (int32 X = Col; X < FMath::Min(Col + W, GridColumns); ++X)
		{
			if (UBorder* Border = GetCellBorder(X, Y))
			{
				Border->SetBrushColor(Color);
				HighlightedCells.Add(FIntPoint(X, Y));
			}
		}
	}
}

void UTarkovBackpackGridWidget::ResetCellHover()
{
	HighlightCells(-1, -1, 0, 0, DefaultCellColor);
	CurrentHoverColor = DefaultCellColor;

	if (HoveredCol >= 0 && HoveredRow >= 0)
	{
		OnCellHoverChanged(HoveredCol, HoveredRow, false, false);
	}
	HoveredCol = -1;
	HoveredRow = -1;
	bHoverIsValid = false;
}

bool UTarkovBackpackGridWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UTarkovItemDragDropOp* DragOp = Cast<UTarkovItemDragDropOp>(InOperation);
	if (!DragOp)
	{
		ResetCellHover();
		return false;
	}

	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	int32 NewCol = -1;
	int32 NewRow = -1;
	const bool bInside = PixelToCell(LocalPos, NewCol, NewRow);

	UTarkovInventoryComponent* Comp = nullptr;
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>();
	}

	bool bCanPlace = false;
	if (bInside && Comp)
	{
		bCanPlace = Comp->CanPlaceAt(NewCol, NewRow, DragOp->Width, DragOp->Height, DragOp->ItemId);
	}

	CurrentHoverColor = (bInside && bCanPlace) ? ValidHoverColor : InvalidHoverColor;
	HighlightCells(bInside ? NewCol : -1, bInside ? NewRow : -1, DragOp->Width, DragOp->Height, CurrentHoverColor);

	if (NewCol != HoveredCol || NewRow != HoveredRow || bCanPlace != bHoverIsValid)
	{
		if (HoveredCol >= 0 && HoveredRow >= 0)
		{
			OnCellHoverChanged(HoveredCol, HoveredRow, false, false);
		}

		HoveredCol = NewCol;
		HoveredRow = NewRow;
		bHoverIsValid = bCanPlace;

		if (bInside && HoveredCol >= 0 && HoveredRow >= 0)
		{
			OnCellHoverChanged(HoveredCol, HoveredRow, bCanPlace, true);
		}
	}

	return bInside && bCanPlace;
}

void UTarkovBackpackGridWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	ResetCellHover();
}

bool UTarkovBackpackGridWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ResetCellHover();

	const UTarkovItemDragDropOp* DragOp = Cast<UTarkovItemDragDropOp>(InOperation);
	if (!DragOp)
	{
		return false;
	}

	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	int32 TargetCol = 0;
	int32 TargetRow = 0;
	if (!PixelToCell(LocalPos, TargetCol, TargetRow))
	{
		return false;
	}

	UTarkovInventoryComponent* Comp = nullptr;
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>();
	}

	bool bHandled = false;
	if (Comp)
	{
		if (DragOp->SourceSlot == ETarkovSlotSource::EquippedSlot)
		{
			bHandled = Comp->UnequipWeaponToBackpack(TargetCol, TargetRow);
		}
		else if (DragOp->SourceSlot == ETarkovSlotSource::BackpackGrid)
		{
			bHandled = Comp->MoveItem(DragOp->ItemId, TargetCol, TargetRow);
		}
	}

	return bHandled;
}

// ============================================================================
// UTarkovEquippedSlotWidget (Dedicated weapon slot)
// ============================================================================

void UTarkovEquippedSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSizeBox"));
		SlotSizeBox->SetWidthOverride(192.0f);
		SlotSizeBox->SetHeightOverride(128.0f);
		WidgetTree->RootWidget = SlotSizeBox;

		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SlotRoot"));
		SlotSizeBox->SetContent(Root);

		SlotFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotFill"));
		SlotFill->SetBrushColor(FLinearColor(0.102f, 0.114f, 0.125f, 0.95f));
		Root->AddChildToOverlay(SlotFill);

		SlotHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotHeader"));
		SlotHeader->SetText(NSLOCTEXT("TarkovInventory", "EquippedLabel", "EQUIPPED"));
		SlotHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
		SlotHeader->SetJustification(ETextJustify::Center);
		UOverlaySlot* HeaderSlot = Root->AddChildToOverlay(SlotHeader);
		HeaderSlot->SetHorizontalAlignment(HAlign_Center);
		HeaderSlot->SetVerticalAlignment(VAlign_Top);
		HeaderSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

		SlotContent = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotContent"));
		SlotContent->SetText(NSLOCTEXT("TarkovInventory", "EquippedEmpty", "EMPTY"));
		SlotContent->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.9f, 1.0f)));
		SlotContent->SetJustification(ETextJustify::Center);
		FSlateFontInfo ContentFont = SlotContent->GetFont();
		ContentFont.Size = 12;
		SlotContent->SetFont(ContentFont);
		UOverlaySlot* ContentSlot = Root->AddChildToOverlay(SlotContent);
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
}

FReply UTarkovEquippedSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (APawn* OwningPawn = GetOwningPlayerPawn())
		{
			if (UTarkovInventoryComponent* Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>())
			{
				if (Comp->bHasEquippedWeapon)
				{
					return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
				}
			}
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTarkovEquippedSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	UTarkovInventoryComponent* Comp = nullptr;
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>();
	}
	if (!Comp || !Comp->bHasEquippedWeapon)
	{
		return;
	}

	UTarkovItemDragDropOp* DragOp = NewObject<UTarkovItemDragDropOp>(this);
	DragOp->ItemId = Comp->EquippedWeaponItem.ItemId;
	DragOp->SourceSlot = ETarkovSlotSource::EquippedSlot;
	DragOp->Width = 1;
	DragOp->Height = 1;
	DragOp->DefaultDragVisual = this;
	DragOp->Pivot = EDragPivot::CenterCenter;

	OutOperation = DragOp;
}

bool UTarkovEquippedSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UTarkovItemDragDropOp* DragOp = Cast<UTarkovItemDragDropOp>(InOperation);
	if (!DragOp)
	{
		return false;
	}

	UTarkovInventoryComponent* Comp = nullptr;
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>();
	}

	bool bHandled = false;
	if (Comp)
	{
		bHandled = Comp->EquipItemFromBackpack(DragOp->ItemId);
	}

	return bHandled;
}

// ============================================================================
// UTarkovInventoryMainWidget (Main overlay #1A1D20)
// ============================================================================

void UTarkovInventoryMainWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);

	// Create the widget tree in C++ since the Blueprint has no widget tree
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	if (!WidgetTree->RootWidget)
	{
		// Use a SizeBox as root so the widget has a fixed visible size
		USizeBox* RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSizeBox"));
		WidgetTree->RootWidget = RootSizeBox;

		UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
		RootBorder->SetBrushColor(FLinearColor(0.102f, 0.114f, 0.125f, 0.95f));
		RootBorder->SetPadding(FMargin(8.0f));
		RootSizeBox->SetContent(RootBorder);

		RootSizeBox->SetWidthOverride(584.0f);
		RootSizeBox->SetHeightOverride(408.0f);

		UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
		RootBorder->SetContent(RootOverlay);

		// Title
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
		Title->SetText(NSLOCTEXT("TarkovInventory", "BackpackTitle", "INVENTORY / BACKPACK"));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 16;
		Title->SetFont(TitleFont);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)));
		Title->SetJustification(ETextJustify::Center);
		UOverlaySlot* TitleSlot = RootOverlay->AddChildToOverlay(Title);
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetVerticalAlignment(VAlign_Top);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));

		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		UOverlaySlot* CanvasSlot = RootOverlay->AddChildToOverlay(Root);
		CanvasSlot->SetHorizontalAlignment(HAlign_Left);
		CanvasSlot->SetVerticalAlignment(VAlign_Top);
		CanvasSlot->SetPadding(FMargin(0.0f, 32.0f, 0.0f, 0.0f));

		// Create the backpack grid widget
		UTarkovBackpackGridWidget* Grid = WidgetTree->ConstructWidget<UTarkovBackpackGridWidget>(UTarkovBackpackGridWidget::StaticClass(), TEXT("BackpackGrid"));
		if (Grid)
		{
			UCanvasPanelSlot* GridSlot = Root->AddChildToCanvas(Grid);
			GridSlot->SetPosition(FVector2D(0.0f, 0.0f));
			GridSlot->SetSize(FVector2D(UTarkovBackpackGridWidget::DisplayTotalGridSize, UTarkovBackpackGridWidget::DisplayTotalGridSize));
			Grid->Initialize();
			Grid->EnsureCellGrid();
		}

		// Create the equipped slot widget
		UTarkovEquippedSlotWidget* Equipped = WidgetTree->ConstructWidget<UTarkovEquippedSlotWidget>(UTarkovEquippedSlotWidget::StaticClass(), TEXT("EquippedSlot"));
		if (Equipped)
		{
			Equipped->Initialize();
			UCanvasPanelSlot* EquippedSlot = Root->AddChildToCanvas(Equipped);
			EquippedSlot->SetPosition(FVector2D(376.0f, 0.0f));
			EquippedSlot->SetSize(FVector2D(192.0f, 128.0f));
		}
	}

}

FReply UTarkovInventoryMainWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::R)
	{
		if (APawn* Pawn = GetOwningPlayerPawn())
		{
			if (UTarkovInventoryComponent* Comp = Pawn->FindComponentByClass<UTarkovInventoryComponent>())
			{
				FGuid ItemToRotate = FocusedOrHoveredItemId;
				if (!ItemToRotate.IsValid())
				{
					const TArray<FInventoryGridItem>& Items = Comp->GetBackpackItems();
					if (Items.Num() == 1)
					{
						ItemToRotate = Items[0].ItemId;
					}
				}

				if (ItemToRotate.IsValid())
				{
					if (Comp->RotateItem(ItemToRotate))
					{
						return FReply::Handled();
					}
				}
			}
		}
		// Always handle/consume R key so it does not trigger gameplay weapon reload while inventory is open
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UTarkovInventoryMainWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UTarkovInventoryMainWidget::ToggleInventoryUI(bool bOpen)
{
	SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (bOpen)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
		else
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;
		}
	}
}

void UTarkovEquippedSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromInventory();
}

void UTarkovEquippedSlotWidget::RefreshFromInventory()
{
	if (!SlotContent)
	{
		return;
	}

	UTarkovInventoryComponent* Comp = nullptr;
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>();
	}

	const bool bEquipped = Comp && Comp->bHasEquippedWeapon;
	const FString Shown = bEquipped ? Comp->EquippedWeaponItem.ItemName.ToUpper() : FString(TEXT("EMPTY"));
	if (Shown == LastShown)
	{
		return;
	}
	LastShown = Shown;

	SlotContent->SetText(FText::FromString(Shown));
	if (SlotFill)
	{
		SlotFill->SetBrushColor(bEquipped ? FLinearColor(0.30f, 0.33f, 0.22f, 1.0f) : FLinearColor(0.102f, 0.114f, 0.125f, 0.95f));
	}
}

void UTarkovInventoryItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ItemRoot"));
		WidgetTree->RootWidget = Root;

		ItemFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ItemFill"));
		ItemFill->SetBrushColor(FLinearColor(0.30f, 0.33f, 0.22f, 1.0f));
		Root->AddChildToOverlay(ItemFill);

		ItemLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemLabel"));
		ItemLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.9f, 1.0f)));
		ItemLabel->SetJustification(ETextJustify::Center);
		FSlateFontInfo LabelFont = ItemLabel->GetFont();
		LabelFont.Size = 9;
		ItemLabel->SetFont(LabelFont);
		UOverlaySlot* LabelSlot = Root->AddChildToOverlay(ItemLabel);
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UTarkovInventoryItemWidget::SetDisplayName(const FString& Name)
{
	if (ItemLabel)
	{
		ItemLabel->SetText(FText::FromString(Name.ToUpper()));
	}
}

void UTarkovBackpackGridWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SyncItemWidgets();
}

void UTarkovBackpackGridWidget::SyncItemWidgets()
{
	UTarkovInventoryComponent* Comp = nullptr;
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		Comp = OwningPawn->FindComponentByClass<UTarkovInventoryComponent>();
	}
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Comp || !Root)
	{
		return;
	}

	const TArray<FInventoryGridItem>& Items = Comp->GetBackpackItems();

	for (int32 Index = ItemWidgets.Num() - 1; Index >= 0; --Index)
	{
		UTarkovInventoryItemWidget* Widget = ItemWidgets[Index];
		bool bStillInBackpack = false;
		for (const FInventoryGridItem& Item : Items)
		{
			if (Item.ItemId == Widget->ItemId)
			{
				bStillInBackpack = true;
				break;
			}
		}
		if (!bStillInBackpack)
		{
			Widget->RemoveFromParent();
			ItemWidgets.RemoveAt(Index);
		}
	}

	for (const FInventoryGridItem& Item : Items)
	{
		UTarkovInventoryItemWidget* Widget = nullptr;
		for (UTarkovInventoryItemWidget* Existing : ItemWidgets)
		{
			if (Existing->ItemId == Item.ItemId)
			{
				Widget = Existing;
				break;
			}
		}

		if (!Widget)
		{
			Widget = WidgetTree->ConstructWidget<UTarkovInventoryItemWidget>(UTarkovInventoryItemWidget::StaticClass());
			if (!Widget)
			{
				continue;
			}
			Widget->Initialize();
			Widget->ItemId = Item.ItemId;
			Widget->SlotSource = ETarkovSlotSource::BackpackGrid;
			Widget->SetDisplayName(Item.ItemName);
			Root->AddChildToCanvas(Widget);
			ItemWidgets.Add(Widget);
		}

		Widget->GridX = Item.GridX;
		Widget->GridY = Item.GridY;
		Widget->CellWidth = Item.Width;
		Widget->CellHeight = Item.Height;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(Item.GridX * DisplayCellSize + CellInset, Item.GridY * DisplayCellSize + CellInset));
			CanvasSlot->SetSize(FVector2D(Item.Width * DisplayCellSize - 2.0f * CellInset, Item.Height * DisplayCellSize - 2.0f * CellInset));
		}
	}
}

void UTarkovHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HUDRoot"));
		WidgetTree->RootWidget = Root;

		EquippedSlot = WidgetTree->ConstructWidget<UTarkovEquippedSlotWidget>(UTarkovEquippedSlotWidget::StaticClass(), TEXT("HUDEquippedSlot"));
		EquippedSlot->Initialize();
		UOverlaySlot* Placement = Root->AddChildToOverlay(EquippedSlot);
		Placement->SetHorizontalAlignment(HAlign_Right);
		Placement->SetVerticalAlignment(VAlign_Bottom);
		Placement->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 24.0f));
	}
}
