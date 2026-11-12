// Copyright GameJam Shift 2026. All Rights Reserved.

#include "InterWorldBridge.h"
#include "TarkovInventoryComponent.h"
#include "ShooterCharacter.h"
#include "GameJam_Shift_2026.h"
#include "Engine/Engine.h"

void UInterWorldBridge::ResolveItemMetadata(FName ItemID, FString& OutDisplayName, int32& OutMaxStack, int32& OutWidth, int32& OutHeight)
{
	const FString ItemStr = ItemID.ToString();
	OutWidth = 1;
	OutHeight = 1;

	if (ItemStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase) || ItemStr.Contains(TEXT("12G"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("12G Shotgun Shells");
		OutMaxStack = 20;
	}
	else if (ItemStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase) || ItemStr.Contains(TEXT("556"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("5.56mm Rifle Ammo");
		OutMaxStack = 60;
	}
	else if (ItemStr.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase) || ItemStr.Contains(TEXT("9mm"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("9mm Pistol Ammo");
		OutMaxStack = 50;
	}
	else if (ItemStr.Contains(TEXT("Medkit"), ESearchCase::IgnoreCase) || ItemStr.Contains(TEXT("Health"), ESearchCase::IgnoreCase) || ItemStr.Contains(TEXT("Herb"), ESearchCase::IgnoreCase))
	{
		OutDisplayName = TEXT("Medkit");
		OutMaxStack = 1;
	}
	else
	{
		OutDisplayName = ItemStr;
		OutMaxStack = 50;
	}
}

bool UInterWorldBridge::DepositHarvestToInventory(AActor* Player, FName ItemID, int32 Quantity)
{
	if (!Player || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	UTarkovInventoryComponent* Inventory = Player->FindComponentByClass<UTarkovInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogGameJam_Shift_2026, Warning, TEXT("DepositHarvestToInventory: Actor %s has no UTarkovInventoryComponent."), *Player->GetName());
		return false;
	}

	FString DisplayName;
	int32 MaxStack = 50;
	int32 ItemWidth = 1;
	int32 ItemHeight = 1;
	ResolveItemMetadata(ItemID, DisplayName, MaxStack, ItemWidth, ItemHeight);

	int32 RemainingToDeposit = Quantity;

	// 1. Try stacking into existing items that have available capacity
	for (FInventoryGridItem& ExistingItem : Inventory->BackpackItems)
	{
		if (ExistingItem.ItemName == DisplayName && ExistingItem.AmmoCount < MaxStack)
		{
			const int32 SpaceInStack = MaxStack - ExistingItem.AmmoCount;
			const int32 AmountToAdd = FMath::Min(RemainingToDeposit, SpaceInStack);
			ExistingItem.AmmoCount += AmountToAdd;
			RemainingToDeposit -= AmountToAdd;

			if (RemainingToDeposit <= 0)
			{
				break;
			}
		}
	}

	// 2. For remaining quantity, place new 1x1 items in the grid
	while (RemainingToDeposit > 0)
	{
		int32 TargetX = -1;
		int32 TargetY = -1;
		if (!Inventory->FindFreeSpace(ItemWidth, ItemHeight, TargetX, TargetY))
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1, 4.0f, FColor::Red,
					FString::Printf(TEXT("[BACKPACK FULL] No room for %d x %s!"), RemainingToDeposit, *DisplayName)
				);
			}
			UE_LOG(LogGameJam_Shift_2026, Warning, TEXT("DepositHarvestToInventory: Backpack full on %s, dropped %d x %s"),
				*Player->GetName(), RemainingToDeposit, *DisplayName);
			Inventory->OnInventoryUpdated.Broadcast();
			return false;
		}

		const int32 AmountForThisTile = FMath::Min(RemainingToDeposit, MaxStack);

		FInventoryGridItem NewItem;
		NewItem.ItemId = FGuid::NewGuid();
		NewItem.ItemName = DisplayName;
		NewItem.Width = ItemWidth;
		NewItem.Height = ItemHeight;
		NewItem.AmmoCount = AmountForThisTile;
		NewItem.GridX = TargetX;
		NewItem.GridY = TargetY;

		Inventory->AddItemAt(NewItem, TargetX, TargetY);
		RemainingToDeposit -= AmountForThisTile;
	}

	Inventory->OnInventoryUpdated.Broadcast();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 4.0f, FColor::Green,
			FString::Printf(TEXT(">>> [INVENTORY] Deposited +%d %s to Backpack! (Press Tab to view)"), Quantity, *DisplayName)
		);
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("DepositHarvestToInventory: Deposited %d x %s into %s's backpack."),
		Quantity, *DisplayName, *Player->GetName());

	return true;
}

bool UInterWorldBridge::ConsumeItemFromInventory(AActor* Player, FName ItemID, int32 Quantity)
{
	if (!Player || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	UTarkovInventoryComponent* Inventory = Player->FindComponentByClass<UTarkovInventoryComponent>();
	if (!Inventory)
	{
		return false;
	}

	FString DisplayName;
	int32 MaxStack, W, H;
	ResolveItemMetadata(ItemID, DisplayName, MaxStack, W, H);

	if (GetTotalItemCountInInventory(Player, ItemID) < Quantity)
	{
		return false;
	}

	int32 RemainingToConsume = Quantity;

	for (int32 i = Inventory->BackpackItems.Num() - 1; i >= 0; --i)
	{
		FInventoryGridItem& Item = Inventory->BackpackItems[i];
		if (Item.ItemName == DisplayName)
		{
			if (Item.AmmoCount <= RemainingToConsume)
			{
				RemainingToConsume -= Item.AmmoCount;
				Inventory->BackpackItems.RemoveAt(i);
			}
			else
			{
				Item.AmmoCount -= RemainingToConsume;
				RemainingToConsume = 0;
			}

			if (RemainingToConsume <= 0)
			{
				break;
			}
		}
	}

	Inventory->OnInventoryUpdated.Broadcast();
	return true;
}

int32 UInterWorldBridge::GetTotalItemCountInInventory(AActor* Player, FName ItemID)
{
	if (!Player || ItemID.IsNone())
	{
		return 0;
	}

	UTarkovInventoryComponent* Inventory = Player->FindComponentByClass<UTarkovInventoryComponent>();
	if (!Inventory)
	{
		return 0;
	}

	FString DisplayName;
	int32 MaxStack, W, H;
	ResolveItemMetadata(ItemID, DisplayName, MaxStack, W, H);

	int32 Total = 0;
	for (const FInventoryGridItem& Item : Inventory->BackpackItems)
	{
		if (Item.ItemName == DisplayName)
		{
			Total += Item.AmmoCount;
		}
	}
	return Total;
}

bool UInterWorldBridge::HasItemInInventory(AActor* Player, FName ItemID, int32 RequiredQuantity)
{
	return GetTotalItemCountInInventory(Player, ItemID) >= RequiredQuantity;
}

bool UInterWorldBridge::UseMedkitFromInventory(AActor* Player, float HealthToRestore)
{
	if (!Player)
	{
		return false;
	}

	AShooterCharacter* Shooter = Cast<AShooterCharacter>(Player);
	if (!Shooter)
	{
		return false;
	}

	if (!ConsumeItemFromInventory(Player, FName("Item_Medkit"), 1))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[MEDKIT] No Medkit in backpack!"));
		}
		return false;
	}

	Shooter->Heal(HealthToRestore);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 3.5f, FColor::Green,
			FString::Printf(TEXT(">>> [MEDKIT USED] Restored +%.0f HP! (Current HP: %.0f/%.0f)"),
				HealthToRestore, Shooter->GetCurrentHP(), Shooter->GetMaxHP())
		);
	}

	return true;
}
