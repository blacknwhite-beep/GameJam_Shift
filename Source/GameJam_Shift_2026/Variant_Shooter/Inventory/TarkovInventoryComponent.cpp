// Copyright Epic Games, Inc. All Rights Reserved.

#include "TarkovInventoryComponent.h"
#include "ShooterCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "../UI/TarkovInventoryWidgets.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "TimerManager.h"
#include "Weapons/ShooterWeapon.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UTarkovInventoryComponent::ResolveItemVisuals(const FString& ItemName, int32& OutWidth, int32& OutHeight, UTexture2D*& OutIcon)
{
	OutWidth = 1;
	OutHeight = 1;
	const TCHAR* AssetPath = nullptr;

	// Ammo (small 1x1 so backpack holds plenty of ammo)
	if (ItemName.Contains(TEXT("9mm"), ESearchCase::IgnoreCase) || ItemName.Contains(TEXT("Pistol Ammo"), ESearchCase::IgnoreCase))
	{
		OutWidth = 1;
		OutHeight = 1;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Ammo_9mm.T_Icon_Ammo_9mm");
	}
	else if (ItemName.Contains(TEXT("12G"), ESearchCase::IgnoreCase) || ItemName.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase))
	{
		OutWidth = 1;
		OutHeight = 1;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Ammo_12G.T_Icon_Ammo_12G");
	}
	else if (ItemName.Contains(TEXT("5.56"), ESearchCase::IgnoreCase) || ItemName.Contains(TEXT("Rifle Ammo"), ESearchCase::IgnoreCase))
	{
		OutWidth = 1;
		OutHeight = 1;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Ammo_556.T_Icon_Ammo_556");
	}
	// Medkit (compact 1x2 tactical IFAK, rotatable with R)
	else if (ItemName.Contains(TEXT("Medkit"), ESearchCase::IgnoreCase) || ItemName.Contains(TEXT("Herb"), ESearchCase::IgnoreCase))
	{
		OutWidth = 1;
		OutHeight = 2;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Medkit.T_Icon_Medkit");
	}
	// Landmine (medium-sized 2x2)
	else if (ItemName.Contains(TEXT("Landmine"), ESearchCase::IgnoreCase) || ItemName.Contains(TEXT("Mine"), ESearchCase::IgnoreCase))
	{
		OutWidth = 2;
		OutHeight = 2;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Landmine.T_Icon_Landmine");
	}
	// Farm Plot (biggest item in inventory: 3x2, rotatable with R)
	else if (ItemName.Contains(TEXT("Farm Plot"), ESearchCase::IgnoreCase) || ItemName.Contains(TEXT("Plot"), ESearchCase::IgnoreCase))
	{
		OutWidth = 3;
		OutHeight = 2;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_FarmPlot.T_Icon_FarmPlot");
	}
	// Rifles (3x1)
	else if (ItemName.Contains(TEXT("M4A1"), ESearchCase::IgnoreCase)
		|| ItemName.Contains(TEXT("M16"), ESearchCase::IgnoreCase)
		|| ItemName.Contains(TEXT("SCAR"), ESearchCase::IgnoreCase)
		|| ItemName.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase)
		|| ItemName.Contains(TEXT("Carbine"), ESearchCase::IgnoreCase))
	{
		OutWidth = 3;
		OutHeight = 1;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Weapon_Rifle.T_Icon_Weapon_Rifle");
	}
	// SMGs (2x1)
	else if (ItemName.Contains(TEXT("MP5"), ESearchCase::IgnoreCase)
		|| ItemName.Contains(TEXT("MP7"), ESearchCase::IgnoreCase)
		|| ItemName.Contains(TEXT("SMG"), ESearchCase::IgnoreCase)
		|| ItemName.Contains(TEXT("PDW"), ESearchCase::IgnoreCase))
	{
		OutWidth = 2;
		OutHeight = 1;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Weapon_SMG.T_Icon_Weapon_SMG");
	}
	// Pistols (2x1)
	else if (ItemName.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase) || ItemName.Contains(TEXT("M1911"), ESearchCase::IgnoreCase))
	{
		OutWidth = 2;
		OutHeight = 1;
		AssetPath = TEXT("/Game/Dev_Blueprint/UI/Icons/T_Icon_Weapon_Pistol.T_Icon_Weapon_Pistol");
	}

	OutIcon = AssetPath ? LoadObject<UTexture2D>(nullptr, AssetPath) : nullptr;
}

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
			int32 W = 2;
			int32 H = 1;
			UTexture2D* IconTex = nullptr;
			ResolveItemVisuals(EquippedWeaponItem.ItemName, W, H, IconTex);
			EquippedWeaponItem.Width = W;
			EquippedWeaponItem.Height = H;
			EquippedWeaponItem.bIsRotated = false;
			EquippedWeaponItem.Icon = IconTex;
			EquippedWeaponItem.AmmoCount = 10;
			bHasEquippedWeapon = true;
		}
	}

	// Grant starting stacks of 100x 9mm Pistol Ammo in the Tarkov backpack
	if (StartingPistolAmmoStacks > 0)
	{
		int32 AmmoW = 1;
		int32 AmmoH = 1;
		UTexture2D* AmmoIcon = nullptr;
		ResolveItemVisuals(TEXT("9mm Pistol Ammo"), AmmoW, AmmoH, AmmoIcon);

		for (int32 StackIdx = 0; StackIdx < StartingPistolAmmoStacks; ++StackIdx)
		{
			int32 FreeX = -1;
			int32 FreeY = -1;
			if (FindFreeSpace(AmmoW, AmmoH, FreeX, FreeY))
			{
				FInventoryGridItem AmmoStack;
				AmmoStack.ItemId = FGuid::NewGuid();
				AmmoStack.ItemName = TEXT("9mm Pistol Ammo");
				AmmoStack.Width = AmmoW;
				AmmoStack.Height = AmmoH;
				AmmoStack.bIsRotated = false;
				AmmoStack.AmmoCount = 100;
				AmmoStack.Icon = AmmoIcon;
				AmmoStack.GridX = FreeX;
				AmmoStack.GridY = FreeY;
				BackpackItems.Add(AmmoStack);
			}
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

	if (!Item.Icon)
	{
		int32 ResolvedW = Item.Width;
		int32 ResolvedH = Item.Height;
		UTexture2D* ResolvedIcon = nullptr;
		ResolveItemVisuals(Item.ItemName, ResolvedW, ResolvedH, ResolvedIcon);
		Item.Icon = ResolvedIcon;
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

	// If a weapon is already equipped, swap it into the backpack
	if (bHasEquippedWeapon)
	{
		if (AShooterWeapon* ActiveWep = ShooterChar->GetCurrentWeapon())
		{
			EquippedWeaponItem.AmmoCount = ActiveWep->GetBulletCount();
		}
		FInventoryGridItem OldEquipped = EquippedWeaponItem;
		if (CanPlaceItemAt(ItemToEquip.GridX, ItemToEquip.GridY, OldEquipped.Width, OldEquipped.Height, ItemToEquip.ItemId))
		{
			OldEquipped.GridX = ItemToEquip.GridX;
			OldEquipped.GridY = ItemToEquip.GridY;
			BackpackItems[ItemIdx] = OldEquipped;
		}
		else
		{
			// Remove ItemToEquip first, then find a valid spot for OldEquipped (including rotated)
			BackpackItems.RemoveAt(ItemIdx);
			int32 FreeX = -1;
			int32 FreeY = -1;
			bool bRotated = false;
			if (FindFreeSpaceWithRotation(OldEquipped.Width, OldEquipped.Height, FreeX, FreeY, bRotated))
			{
				if (bRotated)
				{
					Swap(OldEquipped.Width, OldEquipped.Height);
					OldEquipped.bIsRotated = !OldEquipped.bIsRotated;
				}
				OldEquipped.GridX = FreeX;
				OldEquipped.GridY = FreeY;
				BackpackItems.Add(OldEquipped);
			}
			else
			{
				// Restore ItemToEquip and abort swap if backpack has no room for the larger weapon
				BackpackItems.Insert(ItemToEquip, ItemIdx);
				return false;
			}
		}
	}
	else
	{
		// Remove from backpack
		BackpackItems.RemoveAt(ItemIdx);
	}

	// Set as equipped
	EquippedWeaponItem = ItemToEquip;
	bHasEquippedWeapon = true;

	// Tell character to equip the weapon and restore its exact saved magazine bullet count
	ShooterChar->EquipWeaponClass(ItemToEquip.WeaponClass);
	if (AShooterWeapon* NewActiveWep = ShooterChar->GetCurrentWeapon())
	{
		NewActiveWep->SetBulletCount(ItemToEquip.AmmoCount);
		EquippedWeaponItem.AmmoCount = NewActiveWep->GetBulletCount();
	}

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

	bool bRotatedPlacement = false;
	if (TargetX < 0 || TargetY < 0)
	{
		if (!FindFreeSpaceWithRotation(EquippedWeaponItem.Width, EquippedWeaponItem.Height, TargetX, TargetY, bRotatedPlacement))
		{
			return false;
		}
	}

	const int32 PlaceW = bRotatedPlacement ? EquippedWeaponItem.Height : EquippedWeaponItem.Width;
	const int32 PlaceH = bRotatedPlacement ? EquippedWeaponItem.Width : EquippedWeaponItem.Height;

	// Check if target fits in backpack
	if (!CanPlaceItemAt(TargetX, TargetY, PlaceW, PlaceH))
	{
		return false;
	}

	if (AShooterWeapon* ActiveWep = ShooterChar->GetCurrentWeapon())
	{
		EquippedWeaponItem.AmmoCount = ActiveWep->GetBulletCount();
	}

	FInventoryGridItem ItemToAdd = EquippedWeaponItem;
	ItemToAdd.Width = PlaceW;
	ItemToAdd.Height = PlaceH;
	if (bRotatedPlacement)
	{
		ItemToAdd.bIsRotated = !ItemToAdd.bIsRotated;
	}
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

bool UTarkovInventoryComponent::FindFreeSpaceWithRotation(int32 W, int32 H, int32& OutX, int32& OutY, bool& OutRotated) const
{
	if (FindFreeSpace(W, H, OutX, OutY))
	{
		OutRotated = false;
		return true;
	}

	if (W != H && FindFreeSpace(H, W, OutX, OutY))
	{
		OutRotated = true;
		return true;
	}

	OutRotated = false;
	return false;
}

void UTarkovInventoryComponent::ToggleInventory()
{
	bIsInventoryOpen = !bIsInventoryOpen;

	APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(this, 0));

	if (bIsInventoryOpen)
	{
		// Open
		if (!ActiveInventoryWidget && PC)
		{
			TSubclassOf<UUserWidget> WidgetClassToUse = InventoryWidgetClass
				? InventoryWidgetClass
				: TSubclassOf<UUserWidget>(UTarkovInventoryMainWidget::StaticClass());
			ActiveInventoryWidget = CreateWidget<UUserWidget>(PC, WidgetClassToUse);
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
