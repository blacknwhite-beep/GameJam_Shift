// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterWeapon.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"

AShooterWeapon::AShooterWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);

	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;

	// create the third person mesh
	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;
}

void AShooterWeapon::BeginPlay()
{
	Super::BeginPlay();

	// subscribe to the owner's destroyed delegate
	GetOwner()->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

	// cast the weapon owner
	WeaponOwner = Cast<IShooterWeaponHolder>(GetOwner());
	PawnOwner = Cast<APawn>(GetOwner());

	// Enforce sensible max magazine capacities per weapon archetype
	const FString ClassStr = GetClass()->GetName();
	if (ClassStr.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 12);
	}
	else if (ClassStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Grenade"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Launcher"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 8);
	}
	else if (ClassStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 30);
	}

	// fill the first ammo clip
	CurrentBullets = MagazineSize;

	// attach the meshes to the owner
	WeaponOwner->AttachWeaponMeshes(this);
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the timers
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	// ensure this weapon is destroyed when the owner is destroyed
	Destroy();
}

void AShooterWeapon::ActivateWeapon()
{
	// unhide this weapon
	SetActorHiddenInGame(false);

	// notify the owner
	WeaponOwner->OnWeaponActivated(this);
}

void AShooterWeapon::DeactivateWeapon()
{
	// ensure we're no longer firing this weapon while deactivated
	StopFiring();

	// ensure we're no longer reloading this weapon while deactivated
	CancelReload();

	// hide the weapon
	SetActorHiddenInGame(true);

	// notify the owner
	WeaponOwner->OnWeaponDeactivated(this);
}

void AShooterWeapon::StartFiring()
{
	// weapons can't fire while hidden or deactivated
	if (IsHidden())
	{
		return;
	}

	// raise the firing flag
	bIsFiring = true;

	// check how much time has passed since we last shot
	// this may be under the refire rate if the weapon shoots slow enough and the player is spamming the trigger
	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot > RefireRate)
	{
		// fire the weapon right away
		Fire();

	} else {

		// if we're full auto, schedule the next shot
		if (bFullAuto)
		{
			GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, TimeSinceLastShot, false);
		}

	}
}

void AShooterWeapon::StopFiring()
{
	// lower the firing flag
	bIsFiring = false;

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::StartReload()
{
	// only reload when the weapon can actually accept more ammunition
	if (!CanReload())
	{
		if (CurrentBullets < MagazineSize && GetAvailableInventoryAmmo() <= 0 && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				9160, 3.0f, FColor::Orange,
				FString::Printf(TEXT("[NO AMMO] No %s in Tarkov Backpack! Harvest crops on the Farm!"), *GetRequiredAmmoDisplayName())
			);
		}
		return;
	}

	// raise the reloading flag, this blocks firing until the reload completes
	bIsReloading = true;

	// note: firing is locked out by the bIsReloading checks in Fire() rather than
	// StopFiring(), so a held full auto trigger resumes automatically in FinishReload()
	// while semi auto weapons still require a fresh trigger press

	// play the reload montage on the weapon's owner
	if (ReloadMontage)
	{
		WeaponOwner->PlayReloadMontage(ReloadMontage);
	}

	// play the reload sound
	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	}

	// schedule the magazine refill
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &AShooterWeapon::FinishReload, ReloadDuration, false);
}

void AShooterWeapon::FinishReload()
{
	// Pull matching ammo directly from the player's Tarkov Backpack
	const int32 RoundsNeeded = FMath::Max(0, MagazineSize - CurrentBullets);
	const int32 RoundsLoaded = ConsumeInventoryAmmo(RoundsNeeded);
	CurrentBullets = FMath::Clamp(CurrentBullets + RoundsLoaded, 0, MagazineSize);

	if (AActor* OwnerActor = GetOwner())
	{
		if (UTarkovInventoryComponent* Inv = OwnerActor->FindComponentByClass<UTarkovInventoryComponent>())
		{
			if (Inv->bHasEquippedWeapon)
			{
				Inv->EquippedWeaponItem.AmmoCount = CurrentBullets;
			}
		}
	}

	// lower the reloading flag, the weapon can fire again
	bIsReloading = false;

	// update the weapon HUD
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}

	// if the trigger is still held on a full auto weapon, resume firing
	if (bIsFiring && bFullAuto)
	{
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	}
}

void AShooterWeapon::CancelReload()
{
	// nothing to cancel if a reload isn't in progress
	if (!bIsReloading)
	{
		return;
	}

	// clear the reload timer
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);

	// lower the reloading flag
	bIsReloading = false;

	// stop the reload montage if it is still playing on the owner
	if (ReloadMontage)
	{
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			TArray<USkeletalMeshComponent*> OwnerMeshes;
			OwnerCharacter->GetComponents<USkeletalMeshComponent>(OwnerMeshes);

			for (USkeletalMeshComponent* OwnerMesh : OwnerMeshes)
			{
				if (UAnimInstance* AnimInstance = OwnerMesh->GetAnimInstance())
				{
					AnimInstance->Montage_Stop(0.25f, ReloadMontage);
				}
			}
		}
	}
}

bool AShooterWeapon::CanReload() const
{
	// can't reload while a reload is already in progress
	if (bIsReloading)
	{
		return false;
	}

	// nothing to gain from reloading a full magazine
	if (CurrentBullets >= MagazineSize)
	{
		return false;
	}

	// require matching ammo in the player's Tarkov Backpack
	if (GetAvailableInventoryAmmo() <= 0)
	{
		return false;
	}

	// need a valid owner to play animations, sounds and update the HUD
	return WeaponOwner != nullptr;
}

bool AShooterWeapon::IsReloading() const
{
	return bIsReloading;
}

void AShooterWeapon::Fire()
{
	// weapons can't fire while hidden or deactivated
	if (IsHidden())
	{
		return;
	}

	// ensure the player still wants to fire. They may have let go of the trigger
	if (!bIsFiring)
	{
		return;
	}

	// weapons can't fire while reloading
	if (bIsReloading)
	{
		return;
	}

	// handle pulling the trigger on an empty magazine
	if (CurrentBullets <= 0)
	{
		// play the dry fire click on the empty chamber
		if (DryFireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DryFireSound, FirstPersonMesh->GetSocketLocation(MuzzleSocketName));
		}

		// update the time of this trigger pull so the refire rate still applies
		TimeOfLastShot = GetWorld()->GetTimeSeconds();

		// auto reload instead of silently refilling the magazine
		StartReload();

		// run the standard cooldown so the trigger behaves normally
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);

		return;
	}
	
	// fire a projectile at the target
	FireProjectile(WeaponOwner->GetWeaponTargetLocation());

	// update the time of our last shot
	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	// make noise so the AI perception system can hear us
	MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, ShotNoiseTag);

	// are we full auto?
	if (bFullAuto)
	{
		// schedule the next shot
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	} else {

		// for semi-auto weapons, schedule the cooldown notification
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);

	}
}

void AShooterWeapon::FireCooldownExpired()
{
	// notify the owner
	WeaponOwner->OnSemiWeaponRefire();
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
	// get the projectile transform
	FTransform ProjectileTransform = CalculateProjectileSpawnTransform(TargetLocation);
	
	// spawn the projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = PawnOwner;

	AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);

	// play the firing sound at the muzzle
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, FirstPersonMesh->GetSocketLocation(MuzzleSocketName));
	}

	// play the firing montage
	WeaponOwner->PlayFiringMontage(FiringMontage);

	// add recoil
	WeaponOwner->AddWeaponRecoil(FiringRecoil);

	// consume bullets
	--CurrentBullets;

	if (AActor* OwnerActor = GetOwner())
	{
		if (UTarkovInventoryComponent* Inv = OwnerActor->FindComponentByClass<UTarkovInventoryComponent>())
		{
			if (Inv->bHasEquippedWeapon)
			{
				Inv->EquippedWeaponItem.AmmoCount = CurrentBullets;
			}
		}
	}

	// update the weapon HUD
	// note: an empty magazine is no longer refilled silently here.
	// pulling the trigger while empty plays a dry fire and starts a reload instead
	WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
	// find the muzzle location
	const FVector MuzzleLoc = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);

	// calculate the spawn location ahead of the muzzle
	const FVector SpawnLoc = MuzzleLoc + ((TargetLocation - MuzzleLoc).GetSafeNormal() * MuzzleOffset);

	// find the aim rotation vector while applying some variance to the target 
	const FRotator AimRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, TargetLocation + (UKismetMathLibrary::RandomUnitVector() * AimVariance));

	// return the built transform
	return FTransform(AimRot, SpawnLoc, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
	return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
	return ThirdPersonAnimInstanceClass;
}

void AShooterWeapon::SetBulletCount(int32 NewCount)
{
	CurrentBullets = FMath::Clamp(NewCount, 0, MagazineSize);
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}
}

FString AShooterWeapon::GetRequiredAmmoDisplayName() const
{
	const FString ClassStr = GetClass()->GetName();
	if (ClassStr.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase))
	{
		return TEXT("9mm Pistol Ammo");
	}
	if (ClassStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Grenade"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Launcher"), ESearchCase::IgnoreCase))
	{
		return TEXT("12G Shotgun Shells");
	}
	if (ClassStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase))
	{
		return TEXT("5.56mm Rifle Ammo");
	}
	return TEXT("9mm Pistol Ammo");
}

namespace
{
	bool DoesItemMatchWeaponAmmo(const FInventoryGridItem& Item, const FString& RequiredAmmoName)
	{
		if (Item.WeaponClass != nullptr || Item.AmmoCount <= 0)
		{
			return false;
		}
		if (Item.ItemName.Equals(RequiredAmmoName, ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (RequiredAmmoName.Contains(TEXT("9mm")) && (Item.ItemName.Contains(TEXT("9mm"), ESearchCase::IgnoreCase) || Item.ItemName.Contains(TEXT("Pistol Ammo"), ESearchCase::IgnoreCase)))
		{
			return true;
		}
		if (RequiredAmmoName.Contains(TEXT("12G")) && (Item.ItemName.Contains(TEXT("12G"), ESearchCase::IgnoreCase) || Item.ItemName.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase)))
		{
			return true;
		}
		if (RequiredAmmoName.Contains(TEXT("5.56")) && (Item.ItemName.Contains(TEXT("5.56"), ESearchCase::IgnoreCase) || Item.ItemName.Contains(TEXT("Rifle Ammo"), ESearchCase::IgnoreCase)))
		{
			return true;
		}
		return false;
	}
}

int32 AShooterWeapon::GetAvailableInventoryAmmo() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0;
	}

	const UTarkovInventoryComponent* Inv = OwnerActor->FindComponentByClass<UTarkovInventoryComponent>();
	if (!Inv)
	{
		return 0;
	}

	const FString ReqName = GetRequiredAmmoDisplayName();
	int32 Total = 0;
	for (const FInventoryGridItem& Item : Inv->BackpackItems)
	{
		if (DoesItemMatchWeaponAmmo(Item, ReqName))
		{
			Total += Item.AmmoCount;
		}
	}
	return Total;
}

int32 AShooterWeapon::ConsumeInventoryAmmo(int32 RoundsNeeded)
{
	if (RoundsNeeded <= 0)
	{
		return 0;
	}

	AActor* OwnerActor = GetOwner();
	UTarkovInventoryComponent* Inv = OwnerActor ? OwnerActor->FindComponentByClass<UTarkovInventoryComponent>() : nullptr;
	if (!Inv)
	{
		return 0;
	}

	const FString ReqName = GetRequiredAmmoDisplayName();
	int32 RemainingToConsume = RoundsNeeded;
	int32 TotalConsumed = 0;

	for (int32 i = Inv->BackpackItems.Num() - 1; i >= 0 && RemainingToConsume > 0; --i)
	{
		FInventoryGridItem& Item = Inv->BackpackItems[i];
		if (!DoesItemMatchWeaponAmmo(Item, ReqName))
		{
			continue;
		}

		const int32 Take = FMath::Min(RemainingToConsume, Item.AmmoCount);
		Item.AmmoCount -= Take;
		RemainingToConsume -= Take;
		TotalConsumed += Take;

		if (Item.AmmoCount <= 0)
		{
			Inv->BackpackItems.RemoveAt(i);
		}
	}

	if (TotalConsumed > 0)
	{
		Inv->OnInventoryUpdated.Broadcast();
	}
	return TotalConsumed;
}
