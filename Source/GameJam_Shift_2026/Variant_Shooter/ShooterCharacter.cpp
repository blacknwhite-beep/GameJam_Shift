// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "DimensionShiftSubsystem.h"
#include "FarmingComponent.h"
#include "GameJam_Shift_2026.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "ShooterGameMode.h"

namespace
{
	/**
	 *  Plays a montage on the given mesh if its skeleton matches the montage's skeleton.
	 *  Prevents engine warnings when a montage is played on an incompatible rig
	 *  (e.g. a mannequin montage played on first person arms with a different skeleton)
	 */
	void TryPlayCharacterMontage(USkeletalMeshComponent* MeshComponent, UAnimMontage* Montage)
	{
		if (!MeshComponent || !Montage)
		{
			return;
		}

		USkeletalMesh* SkeletalMesh = MeshComponent->GetSkeletalMeshAsset();

		if (!SkeletalMesh || SkeletalMesh->GetSkeleton() != Montage->GetSkeleton())
		{
			return;
		}

		if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage);
		}
	}
}

AShooterCharacter::AShooterCharacter()
{
	// create the noise emitter component
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// create the Tarkov inventory component
	TarkovInventory = CreateDefaultSubobject<UTarkovInventoryComponent>(TEXT("TarkovInventory"));

	// create the Farming component
	FarmingComponent = CreateDefaultSubobject<UFarmingComponent>(TEXT("FarmingComponent"));

	// default weapon input mapping context
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> WeaponContextFinder(TEXT("/Game/Variant_Shooter/Input/IMC_Weapons.IMC_Weapons"));
	if (WeaponContextFinder.Succeeded())
	{
		WeaponMappingContext = WeaponContextFinder.Object;
	}

	// configure movement
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// reset HP to max
	CurrentHP = MaxHP;

	// update the HUD
	OnDamaged.Broadcast(1.0f);

	// spawn and equip initial weapon
	if (InitialWeaponClass)
	{
		AddWeaponClass(InitialWeaponClass);
	}

	// Register with DimensionShiftSubsystem
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Subsystem->OnDimensionShifted.AddDynamic(this, &AShooterCharacter::HandleOnDimensionShifted);
		}
	}
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// Unregister from DimensionShiftSubsystem
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Subsystem->OnDimensionShifted.RemoveDynamic(this, &AShooterCharacter::HandleOnDimensionShifted);
		}
	}

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);

	Super::EndPlay(EndPlayReason);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// base class handles move, aim and jump inputs
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Firing
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);

		// Switch weapon
		EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoSwitchWeapon);

		// Reload
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &AShooterCharacter::DoReload);

		// Toggle Inventory
		if (ToggleInventoryAction)
		{
			EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AShooterCharacter::DoToggleInventory);
		}

		// Dimension Shift (Shift key)
		if (DimensionShiftAction)
		{
			EnhancedInputComponent->BindAction(DimensionShiftAction, ETriggerEvent::Triggered, this, &AShooterCharacter::HandleDimensionShift);
		}
	}

	// Direct key fallback: guarantees Left Shift ALWAYS triggers even if the Enhanced Input asset is unassigned!
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AShooterCharacter::HandleDimensionShift);

	// Direct key fallback for Left Mouse Button so clicking triggers DoStartFiring (firing in combat, farm tool in farm)
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AShooterCharacter::DoStartFiring);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AShooterCharacter::DoStopFiring);

	// Farming tool selection keybindings (active only when CurrentDimension == EGameDimension::Farming)
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AShooterCharacter::SelectTool1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AShooterCharacter::SelectTool2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AShooterCharacter::SelectTool3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AShooterCharacter::SelectTool4);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AShooterCharacter::CycleToolPrevious);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AShooterCharacter::CycleToolNext);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &AShooterCharacter::CycleSeedNext);

}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// ignore if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// Reduce HP
	CurrentHP -= Damage;

	// Have we depleted HP?
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	// update the HUD
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));

	return Damage;
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoAim(Yaw, Pitch);
	}
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoMove(Right, Forward);
	}
}

void AShooterCharacter::DoJumpStart()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpStart();
	}
}

void AShooterCharacter::DoJumpEnd()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpEnd();
	}
}

void AShooterCharacter::DoStartFiring()
{
	// In Farming dimension, route Left Click to FarmingComponent!
	if (CurrentDimension == EGameDimension::Farming)
	{
		if (FarmingComponent)
		{
			FarmingComponent->UseActiveTool();
		}
		return;
	}

	// fire the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StartFiring();
	}
}

void AShooterCharacter::DoStopFiring()
{
	if (CurrentDimension == EGameDimension::Farming)
	{
		return;
	}

	// stop firing the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StopFiring();
	}
}

void AShooterCharacter::DoSwitchWeapon()
{
	if (CurrentDimension == EGameDimension::Farming)
	{
		if (FarmingComponent)
		{
			FarmingComponent->CycleTool(true);
		}
		return;
	}

	// ensure we have at least two weapons two switch between
	if (OwnedWeapons.Num() > 1 && !IsDead())
	{
		// deactivate the old weapon
		CurrentWeapon->DeactivateWeapon();

		// find the index of the current weapon in the owned list
		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		// is this the last weapon?
		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			// loop back to the beginning of the array
			WeaponIndex = 0;
		}
		else {
			// select the next weapon index
			++WeaponIndex;
		}

		// set the new weapon as current
		CurrentWeapon = OwnedWeapons[WeaponIndex];

		// activate the new weapon
		CurrentWeapon->ActivateWeapon();
	}
}

void AShooterCharacter::DoReload()
{
	if (CurrentDimension == EGameDimension::Farming)
	{
		return;
	}

	// Do not reload weapon while inventory overlay is open
	if (TarkovInventory && TarkovInventory->IsInventoryOpen())
	{
		return;
	}

	// reload the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StartReload();
	}
}

void AShooterCharacter::DoToggleInventory()
{
	if (TarkovInventory && !IsDead())
	{
		TarkovInventory->ToggleInventory();
	}
}

void AShooterCharacter::HandleDimensionShift()
{
	if (IsDead())
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT(">>> [INPUT] Left Shift Pressed! Requesting Dimension Shift..."));
	}

	// Close inventory overlay if open before shifting
	if (TarkovInventory && TarkovInventory->IsInventoryOpen())
	{
		TarkovInventory->ToggleInventory();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Subsystem = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Subsystem->RequestShift(this);
		}
	}
}

void AShooterCharacter::HandleOnDimensionShifted(EGameDimension NewDimension, FVector TargetLocation)
{
	CurrentDimension = NewDimension;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	// Make sure WeaponMappingContext is cached
	if (!WeaponMappingContext)
	{
		WeaponMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Variant_Shooter/Input/IMC_Weapons.IMC_Weapons"));
	}

	if (NewDimension == EGameDimension::Farming)
	{
		// Entering Farming dimension: remove weapon mapping context so clicks don't trigger fire actions
		if (WeaponMappingContext)
		{
			Subsystem->RemoveMappingContext(WeaponMappingContext);
		}

		// Apply farming input mapping context
		if (FarmingMappingContext)
		{
			Subsystem->AddMappingContext(FarmingMappingContext, FarmingMappingPriority);
		}

		// Holster weapon while in farming dimension (keep weapon actor valid so animation blueprints don't error)
		if (CurrentWeapon)
		{
			CurrentWeapon->DeactivateWeapon();
			if (CurrentWeapon->GetFirstPersonMesh())
			{
				CurrentWeapon->GetFirstPersonMesh()->SetVisibility(false, true);
			}
			if (CurrentWeapon->GetThirdPersonMesh())
			{
				CurrentWeapon->GetThirdPersonMesh()->SetVisibility(false, true);
			}
		}

		// Clear character weapon anim instances so ABP_FP_Pistol does not tick on inactive weapon
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->SetVisibility(false, true);
			GetFirstPersonMesh()->SetAnimInstanceClass(nullptr);
		}
		if (GetMesh())
		{
			GetMesh()->SetAnimInstanceClass(nullptr);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				9122, 5.0f, FColor::Emerald,
				TEXT(">>> [FARM WORLD] Left Click: Use Tool | 1-4 or Q/E: Tool | T: Seed | Shift: Combat")
			);
		}

		UE_LOG(LogGameJam_Shift_2026, Log, TEXT("ShooterCharacter: Swapped to Farming input context."));
	}
	else
	{
		// Returning to Combat dimension: remove farming input context
		if (FarmingMappingContext)
		{
			Subsystem->RemoveMappingContext(FarmingMappingContext);
		}

		// Restore weapon mapping context
		if (WeaponMappingContext)
		{
			Subsystem->AddMappingContext(WeaponMappingContext, 0);
		}

		// Restore first person mesh visibility
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->SetVisibility(true, true);
		}

		// Unholster and reactivate weapon when returning to combat
		if (CurrentWeapon)
		{
			if (CurrentWeapon->GetFirstPersonMesh())
			{
				CurrentWeapon->GetFirstPersonMesh()->SetVisibility(true, true);
			}
			if (CurrentWeapon->GetThirdPersonMesh())
			{
				CurrentWeapon->GetThirdPersonMesh()->SetVisibility(true, true);
			}
			CurrentWeapon->ActivateWeapon();

			// Restore character mesh AnimInstances
			if (GetFirstPersonMesh())
			{
				GetFirstPersonMesh()->SetAnimInstanceClass(CurrentWeapon->GetFirstPersonAnimInstanceClass());
			}
			if (GetMesh())
			{
				GetMesh()->SetAnimInstanceClass(CurrentWeapon->GetThirdPersonAnimInstanceClass());
			}
		}
		else if (InitialWeaponClass)
		{
			EquipWeaponClass(InitialWeaponClass);
		}

		UE_LOG(LogGameJam_Shift_2026, Log, TEXT("ShooterCharacter: Swapped to Combat input context."));
	}
}

void AShooterCharacter::UnequipCurrentWeapon()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->DeactivateWeapon();
		CurrentWeapon = nullptr;
	}
}

void AShooterCharacter::EquipWeaponClass(TSubclassOf<AShooterWeapon> InWeaponClass)
{
	if (!InWeaponClass)
	{
		return;
	}

	// Check if we already own a weapon of this class
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(InWeaponClass);
	if (OwnedWeapon)
	{
		// Deactivate current weapon if different
		if (CurrentWeapon && CurrentWeapon != OwnedWeapon)
		{
			CurrentWeapon->DeactivateWeapon();
		}

		// Switch to and activate the owned weapon
		CurrentWeapon = OwnedWeapon;
		CurrentWeapon->ActivateWeapon();
	}
	else
	{
		AddWeaponClass(InWeaponClass);
	}
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	Weapon->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, FirstPersonWeaponSocket);
	
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	// play on the first person mesh
	TryPlayCharacterMontage(GetFirstPersonMesh(), Montage);

	// play on the third person mesh so other players see the shot
	TryPlayCharacterMontage(GetMesh(), Montage);
}

void AShooterCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	// play on the first person mesh
	TryPlayCharacterMontage(GetFirstPersonMesh(), Montage);

	// play on the third person mesh so other players see the reload
	TryPlayCharacterMontage(GetMesh(), Montage);
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
	// apply the recoil as pitch input
	AddControllerPitchInput(Recoil);
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
	// trace ahead from the camera viewpoint
	FHitResult OutHit;

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	// do we already own this weapon?
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (!OwnedWeapon)
	{
		// spawn the new weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (AddedWeapon)
		{
			// add the weapon to the owned list
			OwnedWeapons.Add(AddedWeapon);

			// if we have an existing weapon, deactivate it
			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			// switch to the new weapon
			CurrentWeapon = AddedWeapon;
			CurrentWeapon->ActivateWeapon();
		}
	}
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// update the bullet counter
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

	// set the character mesh AnimInstances
	GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	// unused
}

void AShooterCharacter::OnSemiWeaponRefire()
{
	// unused
}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	// check each owned weapon
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	// weapon not found
	return nullptr;

}

void AShooterCharacter::Die()
{
	// clear anim instances before unpossession to avoid ABP_FP_Pistol Accessed None errors
	if (GetFirstPersonMesh())
	{
		GetFirstPersonMesh()->SetAnimInstanceClass(nullptr);
	}
	if (GetMesh())
	{
		GetMesh()->SetAnimInstanceClass(nullptr);
	}

	// deactivate the weapon
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// increment the team score
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	// grant the death tag to the character
	Tags.Add(DeathTag);
		
	// stop character movement
	GetCharacterMovement()->StopMovementImmediately();

	// disable controls
	DisableInput(nullptr);

	// reset the bullet counter UI
	OnBulletCountUpdated.Broadcast(0, 0);

	// call the BP handler
	BP_OnDeath();

	// schedule character respawn
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
}

void AShooterCharacter::OnRespawn()
{
	// destroy the character to force the PC to respawn
	Destroy();
}

bool AShooterCharacter::IsDead() const
{
	// the character is dead if their current HP drops to zero
	return CurrentHP <= 0.0f;
}

void AShooterCharacter::SelectTool1()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::None);
	}
}

void AShooterCharacter::SelectTool2()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::Hoe);
	}
}

void AShooterCharacter::SelectTool3()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::WateringCan);
	}
}

void AShooterCharacter::SelectTool4()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->SetActiveTool(EFarmTool::Harvester);
	}
}

void AShooterCharacter::CycleToolNext()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->CycleTool(true);
	}
}

void AShooterCharacter::CycleToolPrevious()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->CycleTool(false);
	}
}

void AShooterCharacter::CycleSeedNext()
{
	if (CurrentDimension == EGameDimension::Farming && FarmingComponent)
	{
		FarmingComponent->CycleSeed(true);
	}
}
