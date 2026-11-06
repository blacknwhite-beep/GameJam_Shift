// Copyright Epic Games, Inc. All Rights Reserved.
// Tarkov-Style Grid Inventory UMG Native Base Classes (fw-qwen)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "TarkovInventoryWidgets.generated.h"

class UTarkovInventoryComponent;
class UBorder;
class UImage;
class UTextBlock;
class UUniformGridPanel;
class UCanvasPanel;
class UOverlay;
class UOverlaySlot;
class USizeBox;
class UWidgetTree;
class USoundBase;

/**
 * Source slot type for inventory drag-and-drop operations.
 */
UENUM(BlueprintType)
enum class ETarkovSlotSource : uint8
{
	BackpackGrid UMETA(DisplayName = "Backpack Grid"),
	EquippedSlot UMETA(DisplayName = "Equipped Weapon Slot")
};

/**
 * Drag & Drop operation carrying a Tarkov grid item between the 6x6 backpack
 * (120x120px, 20x20px cells) and the dedicated equipped firearm slot.
 */
UCLASS(BlueprintType, Blueprintable)
class UTarkovItemDragDropOp : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/** Unique identifier of the dragged inventory item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tarkov|DragDrop", meta = (ExposeOnSpawn = "true"))
	FGuid ItemId;

	/** Origin container (BackpackGrid or EquippedSlot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tarkov|DragDrop", meta = (ExposeOnSpawn = "true"))
	ETarkovSlotSource SourceSlot = ETarkovSlotSource::BackpackGrid;

	/** Original column in backpack grid (0..5) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tarkov|DragDrop", meta = (ExposeOnSpawn = "true"))
	int32 OriginX = 0;

	/** Original row in backpack grid (0..5) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tarkov|DragDrop", meta = (ExposeOnSpawn = "true"))
	int32 OriginY = 0;

	/** Item width in cells (Pistol = 1 cell = 20px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tarkov|DragDrop", meta = (ExposeOnSpawn = "true"))
	int32 Width = 1;

	/** Item height in cells (Pistol = 1 cell = 20px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tarkov|DragDrop", meta = (ExposeOnSpawn = "true"))
	int32 Height = 1;

	/** Whether item was rotated with R during drag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tarkov|DragDrop")
	bool bIsRotated = false;
};

/**
 * WBP_InventoryItem native base: 20x20px widget representing a 1x1 cell item (e.g., Pistol).
 */
UCLASS(Blueprintable)
class UTarkovInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr float CellPixelSize = 20.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Tarkov|Item")
	FGuid ItemId;

	UPROPERTY(BlueprintReadWrite, Category = "Tarkov|Item")
	ETarkovSlotSource SlotSource = ETarkovSlotSource::BackpackGrid;

	UPROPERTY(BlueprintReadWrite, Category = "Tarkov|Item")
	int32 GridX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Tarkov|Item")
	int32 GridY = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Tarkov|Item")
	int32 CellWidth = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Tarkov|Item")
	int32 CellHeight = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Audio")
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Audio")
	TObjectPtr<USoundBase> RotateSound;

	UPROPERTY()
	TObjectPtr<UBorder> ItemFill;

	UPROPERTY()
	TObjectPtr<UTextBlock> ItemLabel;

	/** Shows the item name on the block */
	void SetDisplayName(const FString& Name);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
};

/**
 * WBP_BackpackGrid native base: 120x120px container holding 6x6 cells of 20x20px each.
 */
UCLASS(Blueprintable)
class UTarkovBackpackGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 GridColumns = 6;
	static constexpr int32 GridRows = 6;
	static constexpr float CellSize = 20.0f;
	static constexpr float TotalGridSize = 120.0f; // 6 * 20.0f = 120x120 pixels (logical)

	/** On-screen scale of the logical grid (3x: 20px logical cell -> 60px on screen) */
	static constexpr float DisplayScale = 3.0f;
	static constexpr float DisplayCellSize = CellSize * DisplayScale;      // 60px per cell
	static constexpr float DisplayTotalGridSize = TotalGridSize * DisplayScale; // 360px
	/** Gridline width: each cell is inset by half of this on every side, giving equal squares */
	static constexpr float GridLineWidth = 2.0f;
	static constexpr float CellInset = GridLineWidth * 0.5f;

	/** Valid hover highlight color (tactical green) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Style")
	FLinearColor ValidHoverColor = FLinearColor(0.2f, 0.45f, 0.2f, 0.85f);

	/** Invalid/occupied hover highlight color (tactical red) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Style")
	FLinearColor InvalidHoverColor = FLinearColor(0.55f, 0.18f, 0.18f, 0.85f);

	/** Default cell color (dark slate #1A1D20) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Style")
	FLinearColor DefaultCellColor = FLinearColor(0.102f, 0.114f, 0.125f, 0.95f);

	/** Gridline colour shown between cells (lighter slate, Tarkov-style) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Style")
	FLinearColor GridLineColor = FLinearColor(0.20f, 0.22f, 0.25f, 1.0f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Tarkov|Style")
	FLinearColor CurrentHoverColor = FLinearColor(0.102f, 0.114f, 0.125f, 0.95f);

	UPROPERTY(BlueprintReadOnly, Category = "Tarkov|Grid")
	int32 HoveredCol = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Tarkov|Grid")
	int32 HoveredRow = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Tarkov|Grid")
	bool bHoverIsValid = false;

	/** Event fired when drag hover changes cell or validity state */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tarkov|Grid")
	void OnCellHoverChanged(int32 Col, int32 Row, bool bIsValid, bool bIsHovered);

	UFUNCTION(BlueprintCallable, Category = "Tarkov|Grid")
	void ResetCellHover();

	/** Border of the Cell_<row>_<col> widget built by WBP_BackpackGrid, or null if absent */
	UBorder* GetCellBorder(int32 Col, int32 Row) const;

	/** Restores previously highlighted cells, then paints the W x H footprint at (Col, Row) with Color */
	void HighlightCells(int32 Col, int32 Row, int32 W, int32 H, const FLinearColor& Color);

	/** Ensure a 6x6 cell grid exists (created in C++ if the Blueprint has no widget tree) */
	void EnsureCellGrid();

	TArray<FIntPoint> HighlightedCells;

	/** Cell borders created in C++ (owned by WidgetTree) */
	TArray<TObjectPtr<UBorder>> CellBorders;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Audio")
	TObjectPtr<USoundBase> DropSound;

	/** Convert local pixel coordinate inside the 120x120px grid to (Col, Row) in [0..5, 0..5] */
	UFUNCTION(BlueprintPure, Category = "Tarkov|Grid")
	bool PixelToCell(const FVector2D& LocalPos, int32& OutCol, int32& OutRow) const;

	/** Item blocks currently drawn on the grid (kept in sync with the component each tick) */
	UPROPERTY()
	TArray<TObjectPtr<UTarkovInventoryItemWidget>> ItemWidgets;

	void SyncItemWidgets();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
};

/**
 * WBP_EquippedSlot native base: Dedicated equipped firearm slot.
 * Dropping a backpack weapon here equips it onto the character mesh.
 */
UCLASS(Blueprintable)
class UTarkovEquippedSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Audio")
	TObjectPtr<USoundBase> EquipDropSound;

	UPROPERTY()
	TObjectPtr<UBorder> SlotFill;

	UPROPERTY()
	TObjectPtr<UTextBlock> SlotHeader;

	UPROPERTY()
	TObjectPtr<UTextBlock> SlotContent;

	FString LastShown;

	/** Updates the slot from the owner's inventory component */
	void RefreshFromInventory();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
};

/**
 * WBP_TarkovInventory native base: Main tactical inventory overlay (#1A1D20).
 */
UCLASS(Abstract, Blueprintable)
class UTarkovInventoryMainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Tarkov|UI")
	FGuid FocusedOrHoveredItemId;

	UFUNCTION(BlueprintCallable, Category = "Tarkov|UI")
	void SetHoveredItem(FGuid InItemId) { FocusedOrHoveredItemId = InItemId; }

	UFUNCTION(BlueprintCallable, Category = "Tarkov|UI")
	void ClearHoveredItem(FGuid InItemId) { if (FocusedOrHoveredItemId == InItemId) FocusedOrHoveredItemId.Invalidate(); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Audio")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tarkov|Audio")
	TObjectPtr<USoundBase> CloseSound;

	UFUNCTION(BlueprintCallable, Category = "Tarkov|UI")
	void ToggleInventoryUI(bool bOpen);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
};

/**
 * HUD root: full-screen overlay that holds the equipped weapon slot in the bottom-right corner.
 */
UCLASS(Blueprintable)
class UTarkovHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UTarkovEquippedSlotWidget> EquippedSlot;

protected:
	virtual void NativeOnInitialized() override;
};

