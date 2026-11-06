// Copyright Epic Games, Inc. All Rights Reserved.

#include "TarkovInventoryComponent.h"
#include "ShooterCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "../UI/TarkovInventoryWidgets.h"
#include "Engine/World.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "TimerManager.h"
#include "Weapons/ShooterWeapon.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

UTarkovInventoryComponent::UTarkovInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	GridColumns = 6;
	GridRows = 6;
	CellPixelSize = 20.0f;
	BackpackPixelWidth = GridColumns * CellPixelSize;   // 120.0f
	BackpackPixelHeight = GridRows * CellPixelSize;     // 120.0f
}

void UTarkovInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// If owner has an initial weapon, register it as equipped weapon
	if (AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(GetOwner()))
	{
		if (ShooterChar->GetInitialWeaponClass())
		{
			EquippedWeaponItem.ItemId = FGuid::NewGuid();
			EquippedWeaponItem.ItemName = TEXT("Pistol");
			EquippedWeaponItem.WeaponClass = ShooterChar->GetInitialWeaponClass();
			EquippedWeaponItem.Width = 1;  // 1x1 cell = 20x20 pixels
			EquippedWeaponItem.Height = 1;
			EquippedWeaponItem.AmmoCount = 10;
			bHasEquippedWeapon = true;
		}
	}
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTarkovInventoryComponent::CreateHUD);
}

bool UTarkovInventoryComponent::CanPlaceItemAt(int32 X, int32 Y, int32 W, int32 H, FGuid IgnoreItemId) const
{
	// Bounds check
	if (X < 0 || Y < 0 || X + W > GridColumns || Y + H > GridRows)
	{
		return false;
	}

	// Collision check against existing items in backpack
	for (const FInventoryGridItem& Other : BackpackItems)
	{
		if (Other.ItemId == IgnoreItemId)
		{
			continue;
		}

		// AABB overlap check in grid coordinates
		const bool bOverlapX = (X < Other.GridX + Other.Width) && (X + W > Other.GridX);
		const bool bOverlapY = (Y < Other.GridY + Other.Height) && (Y + H > Other.GridY);

		if (bOverlapX && bOverlapY)
		{
			return false;
		}
	}

	return true;
}

bool UTarkovInventoryComponent::AddItemAt(FInventoryGridItem& Item, int32 X, int32 Y)
{
	if (!CanPlaceItemAt(X, Y, Item.Width, Item.Height))
	{
		return false;
	}

	if (!Item.ItemId.IsValid())
	{
		Item.ItemId = FGuid::NewGuid();
	}

	Item.GridX = X;
	Item.GridY = Y;

	BackpackItems.Add(Item);

	if (SoundItemDrop)
	{
		UGameplayStatics::PlaySound2D(this, SoundItemDrop);
	}

	OnInventoryUpdated.Broadcast();
	return true;
}

bool UTarkovInventoryComponent::MoveItem(FGuid ItemId, int32 NewX, int32 NewY)
{
	for (FInventoryGridItem& Item : BackpackItems)
	{
		if (Item.ItemId == ItemId)
		{
			if (!CanPlaceItemAt(NewX, NewY, Item.Width, Item.Height, ItemId))
			{
				return false;
			}

			Item.GridX = NewX;
			Item.GridY = NewY;

			if (SoundItemDrop)
			{
				UGameplayStatics::PlaySound2D(this, SoundItemDrop);
			}

			OnInventoryUpdated.Broadcast();
			return true;
		}
	}

	return false;
}

bool UTarkovInventoryComponent::RotateItem(FGuid ItemId)
{
	for (FInventoryGridItem& Item : BackpackItems)
	{
		if (Item.ItemId == ItemId)
		{
			const int32 NewW = Item.Height;
			const int32 NewH = Item.Width;

			if (!CanPlaceItemAt(Item.GridX, Item.GridY, NewW, NewH, ItemId))
			{
				return false;
			}

			Item.Width = NewW;
			Item.Height = NewH;
			Item.bIsRotated = !Item.bIsRotated;

			if (SoundItemRotate)
			{
				UGameplayStatics::PlaySound2D(this, SoundItemRotate);
			}

			OnInventoryUpdated.Broadcast();
			return true;
		}
	}

	return false;
}

bool UTarkovInventoryComponent::RemoveItem(FGuid ItemId)
{
	for (int32 i = 0; i < BackpackItems.Num(); ++i)
	{
		if (BackpackItems[i].ItemId == ItemId)
		{
			BackpackItems.RemoveAt(i);
			OnInventoryUpdated.Broadcast();
			return true;
		}
	}

	return false;
}

bool UTarkovInventoryComponent::EquipItemFromBackpack(FGuid ItemId)
{
	AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(GetOwner());
	if (!ShooterChar)
	{
		return false;
	}

	// Find the item in backpack
	int32 ItemIdx = INDEX_NONE;
	for (int32 i = 0; i < BackpackItems.Num(); ++i)
	{
		if (BackpackItems[i].ItemId == ItemId)
		{
			ItemIdx = i;
			break;
		}
	}

	if (ItemIdx == INDEX_NONE)
	{
		return false;
	}

	FInventoryGridItem ItemToEquip = BackpackItems[ItemIdx];
	if (!ItemToEquip.WeaponClass)
	{
		return false;
	}

	// If a weapon is already equipped, swap it into the backpack at ItemToEquip's position
	if (bHasEquippedWeapon)
	{
		FInventoryGridItem OldEquipped = EquippedWeaponItem;
		OldEquipped.GridX = ItemToEquip.GridX;
		OldEquipped.GridY = ItemToEquip.GridY;

		// Replace in backpack
		BackpackItems[ItemIdx] = OldEquipped;
	}
	else
	{
		// Remove from backpack
		BackpackItems.RemoveAt(ItemIdx);
	}

	// Set as equipped
	EquippedWeaponItem = ItemToEquip;
	bHasEquippedWeapon = true;

	// Tell character to equip the weapon
	ShooterChar->EquipWeaponClass(ItemToEquip.WeaponClass);

	if (SoundItemDrop)
	{
		UGameplayStatics::PlaySound2D(this, SoundItemDrop);
	}

	OnInventoryUpdated.Broadcast();
	return true;
}

bool UTarkovInventoryComponent::UnequipWeaponToBackpack(int32 TargetX, int32 TargetY)
{
	if (!bHasEquippedWeapon)
	{
		return false;
	}

	AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(GetOwner());
	if (!ShooterChar)
	{
		return false;
	}

	if (TargetX < 0 || TargetY < 0)
	{
		if (!FindFreeSpace(EquippedWeaponItem.Width, EquippedWeaponItem.Height, TargetX, TargetY))
		{
			return false;
		}
	}

	// Check if target fits in backpack
	if (!CanPlaceItemAt(TargetX, TargetY, EquippedWeaponItem.Width, EquippedWeaponItem.Height))
	{
		return false;
	}

	FInventoryGridItem ItemToAdd = EquippedWeaponItem;
	ItemToAdd.GridX = TargetX;
	ItemToAdd.GridY = TargetY;

	BackpackItems.Add(ItemToAdd);
	bHasEquippedWeapon = false;

	// Tell character to unequip
	ShooterChar->UnequipCurrentWeapon();

	if (SoundItemDrop)
	{
		UGameplayStatics::PlaySound2D(this, SoundItemDrop);
	}

	OnInventoryUpdated.Broadcast();
	return true;
}

bool UTarkovInventoryComponent::FindFreeSpace(int32 W, int32 H, int32& OutX, int32& OutY) const
{
	for (int32 Y = 0; Y <= GridRows - H; ++Y)
	{
		for (int32 X = 0; X <= GridColumns - W; ++X)
		{
			if (CanPlaceItemAt(X, Y, W, H))
			{
				OutX = X;
				OutY = Y;
				return true;
			}
		}
	}

	return false;
}

void UTarkovInventoryComponent::ToggleInventory()
{
	bIsInventoryOpen = !bIsInventoryOpen;

	APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(this, 0));

	if (bIsInventoryOpen)
	{
		// Open
		if (!ActiveInventoryWidget && InventoryWidgetClass && PC)
		{
			ActiveInventoryWidget = CreateWidget<UUserWidget>(PC, InventoryWidgetClass);
		}

		if (ActiveInventoryWidget)
		{
			ActiveInventoryWidget->SetDesiredSizeInViewport(FVector2D(584.0f, 408.0f));
			ActiveInventoryWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			ActiveInventoryWidget->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
			ActiveInventoryWidget->SetPositionInViewport(FVector2D(100.0f, 100.0f), false);
			ActiveInventoryWidget->AddToViewport(50);
			ActiveInventoryWidget->SetVisibility(ESlateVisibility::Visible);
			ActiveInventoryWidget->SetRenderOpacity(1.0f);
		}

		if (PC)
		{
			PC->SetShowMouseCursor(true);
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			if (ActiveInventoryWidget)
			{
				InputMode.SetWidgetToFocus(ActiveInventoryWidget->TakeWidget());
			}
			PC->SetInputMode(InputMode);
		}

		if (SoundInventoryOpen)
		{
			UGameplayStatics::PlaySound2D(this, SoundInventoryOpen);
		}
	}
	else
	{
		// Close
		if (ActiveInventoryWidget)
		{
			ActiveInventoryWidget->RemoveFromParent();
		}

		if (PC)
		{
			PC->SetShowMouseCursor(false);
			const FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
		}

		if (SoundInventoryClose)
		{
			UGameplayStatics::PlaySound2D(this, SoundInventoryClose);
		}
	}

	OnInventoryToggled.Broadcast(bIsInventoryOpen);
}

void UTarkovInventoryComponent::CreateHUD()
{
	APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!PC || HUDRoot)
	{
		return;
	}

	HUDRoot = CreateWidget<UTarkovHUDWidget>(PC, UTarkovHUDWidget::StaticClass());
	if (HUDRoot)
	{
		HUDRoot->AddToViewport(10);
	}
}
