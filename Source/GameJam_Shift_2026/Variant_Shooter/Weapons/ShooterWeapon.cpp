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
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

	// create custom static mesh components for the 3D Gun Pack models
	FirstPersonGunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonGunMesh"));
	FirstPersonGunMesh->SetupAttachment(FirstPersonMesh);
	FirstPersonGunMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonGunMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonGunMesh->bOnlyOwnerSee = true;
	FirstPersonGunMesh->SetCastShadow(false);

	ThirdPersonGunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonGunMesh"));
	ThirdPersonGunMesh->SetupAttachment(ThirdPersonMesh);
	ThirdPersonGunMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonGunMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonGunMesh->bOwnerNoSee = true;
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
		MagazineSize = FMath::Clamp(MagazineSize, 1, 15);
	}
	else if (ClassStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Grenade"), ESearchCase::IgnoreCase) || ClassStr.Contains(TEXT("Launcher"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 8);
	}
	else if (ClassStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase))
	{
		MagazineSize = FMath::Clamp(MagazineSize, 1, 40);
	}

	// fill the first ammo clip
	CurrentBullets = MagazineSize;

	// attach the meshes to the owner
	WeaponOwner->AttachWeaponMeshes(this);
}

void AShooterWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsHidden() || !FirstPersonGunMesh || !FirstPersonGunMesh->GetStaticMesh())
	{
		return;
	}

	// 1. Decay procedural recoil spring back to zero
	CurrentRecoilOffset = FMath::VInterpTo(CurrentRecoilOffset, FVector::ZeroVector, DeltaTime, 18.0f);
	CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, 16.0f);

	// 2. Compute procedural reload animation offset (smooth mag-swap dip, tilt & bolt rack)
	FVector ReloadOffset = FVector::ZeroVector;
	FRotator ReloadRot = FRotator::ZeroRotator;
	if (bIsReloading && ReloadDuration > 0.05f)
	{
		ReloadAnimElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(ReloadAnimElapsed / ReloadDuration, 0.0f, 1.0f);

		// Smooth 3-stage reload motion curve:
		// Stage A (0.0 -> 0.25): Tilt weapon 32 deg & dip down to eject mag
		// Stage B (0.25 -> 0.72): Hold tilted down with two tactile mag-out / mag-in pulses
		// Stage C (0.72 -> 1.0): Rack bolt backward slightly and snap back to ready position
		float DipFactor = 0.0f;
		float BoltRackBack = 0.0f;
		if (Alpha < 0.25f)
		{
			DipFactor = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha / 0.25f, 2.0f);
		}
		else if (Alpha < 0.72f)
		{
			const float MidPhase = (Alpha - 0.25f) / 0.47f;
			// Add tactile mag-eject and mag-insert vertical jolt
			const float Jolt = FMath::Sin(MidPhase * PI * 2.0f) * 0.18f;
			DipFactor = 1.0f + Jolt;
		}
		else
		{
			const float EndPhase = (Alpha - 0.72f) / 0.28f;
			DipFactor = FMath::InterpEaseInOut(1.0f, 0.0f, EndPhase, 2.0f);
			BoltRackBack = FMath::Sin(EndPhase * PI) * 4.2f;
		}

		ReloadOffset = FVector(-BoltRackBack, -4.5f * DipFactor, -11.5f * DipFactor);
		ReloadRot = FRotator(26.0f * DipFactor, -14.0f * DipFactor, -34.0f * DipFactor);
	}
	else
	{
		ReloadAnimElapsed = 0.0f;
	}

	// 3. Quick equip / draw rise animation when switching weapons on the wheel
	FVector EquipOffset = FVector::ZeroVector;
	FRotator EquipRot = FRotator::ZeroRotator;
	if (EquipAnimElapsed < 0.22f)
	{
		EquipAnimElapsed += DeltaTime;
		const float EquipAlpha = 1.0f - FMath::Clamp(EquipAnimElapsed / 0.22f, 0.0f, 1.0f);
		const float Smooth = EquipAlpha * EquipAlpha;
		EquipOffset = FVector(0.0f, -3.0f * Smooth, -9.0f * Smooth);
		EquipRot = FRotator(-18.0f * Smooth, 0.0f, -15.0f * Smooth);
	}

	FirstPersonGunMesh->SetRelativeLocation(BaseGunRelativeLocation + CurrentRecoilOffset + ReloadOffset + EquipOffset);
	const FQuat FinalQuat = FQuat(BaseGunRelativeRotation) * FQuat(CurrentRecoilRot + ReloadRot + EquipRot);
	FirstPersonGunMesh->SetRelativeRotation(FinalQuat.Rotator());
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
	EquipAnimElapsed = 0.0f;

	if (FirstPersonGunMesh && FirstPersonGunMesh->GetStaticMesh())
	{
		FirstPersonGunMesh->SetVisibility(true, true);
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(false, false);
		}
	}

	if (ThirdPersonGunMesh && ThirdPersonGunMesh->GetStaticMesh())
	{
		ThirdPersonGunMesh->SetVisibility(true, true);
		if (ThirdPersonMesh)
		{
			ThirdPersonMesh->SetVisibility(false, false);
		}
	}

	// notify the owner
	if (WeaponOwner)
	{
		WeaponOwner->OnWeaponActivated(this);
	}
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
	ReloadAnimElapsed = 0.0f;

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

	// apply procedural 3D weapon recoil impulse on the gun mesh
	CurrentRecoilOffset += FVector(0.0f, -3.2f, 0.9f);
	CurrentRecoilRot += FRotator(8.5f, FMath::FRandRange(-1.8f, 1.8f), -5.0f);

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
	FVector MuzzleLoc = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);
	if (FirstPersonGunMesh && FirstPersonGunMesh->GetStaticMesh())
	{
		MuzzleLoc = FirstPersonGunMesh->GetComponentLocation() + FirstPersonMesh->GetRightVector() * 18.0f + FirstPersonMesh->GetUpVector() * 4.0f;
	}

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
	if (!ExplicitAmmoType.IsEmpty())
	{
		return ExplicitAmmoType;
	}

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

void AShooterWeapon::ConfigureCustomGunPackArchetype(
	const FString& InDisplayName,
	const FString& InSubtitle,
	const FString& InStaticMeshPath,
	const FString& InAmmoType,
	int32 InMagSize,
	bool bInFullAuto,
	float InRefireRate,
	float InReloadDuration,
	float InRecoil,
	float InAimVariance,
	const FVector& InMeshOffset,
	const FRotator& InMeshRotation,
	float InUniformScale,
	TSubclassOf<AShooterProjectile> InProjectileClass,
	TSubclassOf<UAnimInstance> InFPAnimClass,
	TSubclassOf<UAnimInstance> InTPAnimClass,
	UAnimMontage* InFireMontage,
	UAnimMontage* InReloadMontage,
	USoundBase* InFireSound,
	USoundBase* InReloadSound,
	USoundBase* InDryFireSound)
{
	WeaponDisplayName = InDisplayName;
	WeaponSubtitle = InSubtitle;
	ExplicitAmmoType = InAmmoType;
	MagazineSize = InMagSize;
	CurrentBullets = InMagSize;
	bFullAuto = bInFullAuto;
	RefireRate = InRefireRate;
	ReloadDuration = InReloadDuration;
	FiringRecoil = InRecoil;
	AimVariance = InAimVariance;
	MuzzleSocketName = FName("Muzzle");
	MuzzleOffset = 15.0f;

	if (InProjectileClass) { ProjectileClass = InProjectileClass; }
	if (InFPAnimClass) { FirstPersonAnimInstanceClass = InFPAnimClass; }
	if (InTPAnimClass) { ThirdPersonAnimInstanceClass = InTPAnimClass; }
	if (InFireMontage) { FiringMontage = InFireMontage; }
	if (InReloadMontage) { ReloadMontage = InReloadMontage; }
	if (InFireSound) { FireSound = InFireSound; }
	if (InReloadSound) { ReloadSound = InReloadSound; }
	if (InDryFireSound) { DryFireSound = InDryFireSound; }

	BaseGunRelativeLocation = InMeshOffset;
	BaseGunRelativeRotation = InMeshRotation;
	BaseGunRelativeScale = FVector(InUniformScale, InUniformScale, InUniformScale);

	if (UStaticMesh* LoadedGunMesh = LoadObject<UStaticMesh>(nullptr, *InStaticMeshPath))
	{
		if (FirstPersonGunMesh)
		{
			FirstPersonGunMesh->SetStaticMesh(LoadedGunMesh);
			FirstPersonGunMesh->SetRelativeLocation(BaseGunRelativeLocation);
			FirstPersonGunMesh->SetRelativeRotation(BaseGunRelativeRotation);
			FirstPersonGunMesh->SetRelativeScale3D(BaseGunRelativeScale);
			FirstPersonGunMesh->SetVisibility(true, true);
		}
		if (ThirdPersonGunMesh)
		{
			ThirdPersonGunMesh->SetStaticMesh(LoadedGunMesh);
			ThirdPersonGunMesh->SetRelativeLocation(BaseGunRelativeLocation);
			ThirdPersonGunMesh->SetRelativeRotation(BaseGunRelativeRotation);
			ThirdPersonGunMesh->SetRelativeScale3D(BaseGunRelativeScale);
			ThirdPersonGunMesh->SetVisibility(true, true);
		}
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(false, false);
		}
		if (ThirdPersonMesh)
		{
			ThirdPersonMesh->SetVisibility(false, false);
		}
	}
}
