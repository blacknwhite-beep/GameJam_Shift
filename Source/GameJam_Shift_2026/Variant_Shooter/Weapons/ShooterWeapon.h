// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterWeaponHolder.h"
#include "Animation/AnimInstance.h"
#include "ShooterWeapon.generated.h"

class IShooterWeaponHolder;
class AShooterProjectile;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UPointLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UAnimMontage;
class UAnimInstance;
class USoundBase;

/**
 *  Base class for a simple first person shooter weapon
 *  Provides both first person and third person perspective meshes
 *  Handles ammo and firing logic
 *  Interacts with the weapon owner through the ShooterWeaponHolder interface
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API AShooterWeapon : public AActor
{
	GENERATED_BODY()
	
	/** First person perspective mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** Third person perspective mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMesh;

	/** First person custom static mesh for 3D gun pack models (with procedural recoil & reload) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FirstPersonGunMesh;

	/** Third person custom static mesh for 3D gun pack models */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ThirdPersonGunMesh;

	/** First person muzzle flash star-burst flare plane1 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MuzzleFlashCoreMesh;

	/** First person muzzle flash star-burst flare plane2 (crossed at 90 deg) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MuzzleFlashCrossMesh;

	/** Dynamic point light burst at the gun barrel tip on each shot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> MuzzleFlashLight;

	/** Continuous looping audio component used while holding the trigger on full-auto machine guns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioComponent> AutoFireAudioComp;

	/** Stylized Cubic Low-Poly Farmer First-Person Hands & Sleeves gripping the weapon (replaces gray Mannequin arms) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FarmerRightHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FarmerRightArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FarmerLeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FarmerLeftArm;

protected:

	/** Builds & colors the Cubic Farmer First-Person hands/sleeves to match SKM_Farmer_male */
	void SetupFarmerHandsVisuals();

	/** True for pistols (two-handed Weaver grip on pistol grip), false for SMGs/rifles (left hand on foregrip) */
	bool bIsPistolArchetype = true;

	/** Rest & reload keyframes for the Farmer's left hand in FirstPersonMesh local space (+Y forward, +X left, +Z up) */
	FVector LeftHandRestOffset = FVector(1.8f, 3.5f, -1.2f);
	FVector LeftHandMagOffset = FVector(0.8f, 1.0f, -6.5f);
	FVector LeftHandRackOffset = FVector(2.2f, 8.0f, 5.2f);
	float IdleSwayTime = 0.0f;

	/** Stops the continuous full-auto firing loop cleanly */
	void StopAutoFireAudio();

	/** Remaining lifetime in seconds of the active muzzle flare */
	float MuzzleFlashTimeRemaining = 0.0f;

	/** Local barrel tip offset along the gun's forward (+Y) and up (+Z) axis in FirstPersonMesh space */
	FVector MuzzleTipRelativeLocation = FVector(0.0f, 32.0f, 6.5f);

	/** Emissive dynamic material instance for the muzzle flare */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MuzzleFlashMID;

	/** Display name of this weapon (e.g. "M4A1 CARBINE") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FString WeaponDisplayName = TEXT("Pistol");

	/** Category/caliber subtitle shown in the weapon wheel (e.g. "5.56mm Full-Auto Carbine") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FString WeaponSubtitle = TEXT("9mm Sidearm");

	/** Explicit ammo display name override (e.g. "9mm Pistol Ammo", "5.56mm Rifle Ammo") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	FString ExplicitAmmoType;

	/** Base relative offset of FirstPersonGunMesh when held at rest */
	FVector BaseGunRelativeLocation = FVector::ZeroVector;

	/** Base relative rotation of FirstPersonGunMesh when held at rest */
	FRotator BaseGunRelativeRotation = FRotator::ZeroRotator;

	/** Base uniform scale of FirstPersonGunMesh */
	FVector BaseGunRelativeScale = FVector(0.055f, 0.055f, 0.055f);

	/** Current procedural recoil translation offset in local space */
	FVector CurrentRecoilOffset = FVector::ZeroVector;

	/** Current procedural recoil rotation offset in local space */
	FRotator CurrentRecoilRot = FRotator::ZeroRotator;

	/**Elapsed seconds into the active reload animation */
	float ReloadAnimElapsed = 0.0f;

	/**Elapsed seconds into the weapon equip/draw animation */
	float EquipAnimElapsed = 0.0f;

	/** Cast pointer to the weapon owner */
	IShooterWeaponHolder* WeaponOwner;

	/** Type of projectiles this weapon will shoot */
	UPROPERTY(EditAnywhere, Category="Ammo")
	TSubclassOf<AShooterProjectile> ProjectileClass;

	/** Number of bullets in a magazine */
	UPROPERTY(EditAnywhere, Category="Ammo", meta = (ClampMin = 0, ClampMax = 100))
	int32 MagazineSize = 10;

	/** Number of bullets in the current magazine */
	int32 CurrentBullets = 0;

	/** Time taken to reload this weapon's magazine */
	UPROPERTY(EditAnywhere, Category="Ammo")
	float ReloadDuration = 1.5f;
	
	/** Animation montage to play when firing this weapon */
	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* FiringMontage;

	/** Animation montage to play when reloading this weapon */
	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* ReloadMontage;

	/** AnimInstance class to set for the first person character mesh when this weapon is active */
	UPROPERTY(EditAnywhere, Category="Animation")
	TSubclassOf<UAnimInstance> FirstPersonAnimInstanceClass;

	/** AnimInstance class to set for the third person character mesh when this weapon is active */
	UPROPERTY(EditAnywhere, Category="Animation")
	TSubclassOf<UAnimInstance> ThirdPersonAnimInstanceClass;

	/** Sound to play when firing this weapon */
	UPROPERTY(EditAnywhere, Category="Sound")
	USoundBase* FireSound;

	/** Sound to play when reloading this weapon */
	UPROPERTY(EditAnywhere, Category="Sound")
	USoundBase* ReloadSound;

	/** Sound to play when pulling the trigger on an empty magazine */
	UPROPERTY(EditAnywhere, Category="Sound")
	USoundBase* DryFireSound;

	/** Cone half-angle for variance while aiming */
	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 90, Units = "Degrees"))
	float AimVariance = 0.0f;

	/** Amount of firing recoil to apply to the owner */
	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 100))
	float FiringRecoil = 0.0f;

	/** Name of the first person muzzle socket where projectiles will spawn */
	UPROPERTY(EditAnywhere, Category="Aim")
	FName MuzzleSocketName;

	/** Distance ahead of the muzzle that bullets will spawn at */
	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float MuzzleOffset = 10.0f;

	/** If true, this weapon will automatically fire at the refire rate */
	UPROPERTY(EditAnywhere, Category="Refire")
	bool bFullAuto = false;

	/** Time between shots for this weapon. Affects both full auto and semi auto modes */
	UPROPERTY(EditAnywhere, Category="Refire", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float RefireRate = 0.5f;

	/** Game time of last shot fired, used to enforce refire rate on semi auto */
	float TimeOfLastShot = 0.0f;

	/** If true, the weapon is currently firing */
	bool bIsFiring = false;

	/** If true, the weapon is currently reloading */
	bool bIsReloading = false;

	/** Timer to handle full auto refiring */
	FTimerHandle RefireTimer;

	/** Timer to handle completing a reload */
	FTimerHandle ReloadTimer;

	/** Cast pawn pointer to the owner for AI perception system interactions */
	TObjectPtr<APawn> PawnOwner;

	/** Loudness of the shot for AI perception system interactions */
	UPROPERTY(EditAnywhere, Category="Perception", meta = (ClampMin = 0, ClampMax = 100))
	float ShotLoudness = 1.0f;

	/** Max range of shot AI perception noise */
	UPROPERTY(EditAnywhere, Category="Perception", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float ShotNoiseRange = 3000.0f;

	/** Tag to apply to noise generated by shooting this weapon */
	UPROPERTY(EditAnywhere, Category="Perception")
	FName ShotNoiseTag = FName("Shot");

public:	

	/** Constructor */
	AShooterWeapon();

protected:
	
	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Per-frame procedural recoil and reload animation update */
	virtual void Tick(float DeltaTime) override;

	/** Gameplay Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

protected:

	/** Called when the weapon's owner is destroyed */
	UFUNCTION()
	void OnOwnerDestroyed(AActor* DestroyedActor);

public:

	/** Activates this weapon and gets it ready to fire */
	void ActivateWeapon();

	/** Deactivates this weapon */
	void DeactivateWeapon();

	/** Start firing this weapon */
	void StartFiring();

	/** Stop firing this weapon */
	void StopFiring();

	/** Begins a reload of this weapon if CanReload() allows it */
	virtual void StartReload();

	/** Completes an in-progress reload, refilling the magazine */
	virtual void FinishReload();

	/** Cancels an in-progress reload, e.g. when this weapon is deactivated */
	virtual void CancelReload();

	/** Returns true if this weapon is able to start a reload right now */
	UFUNCTION(BlueprintPure, Category="Weapon")
	bool CanReload() const;

	/** Returns true if this weapon is currently reloading */
	UFUNCTION(BlueprintPure, Category="Weapon")
	bool IsReloading() const;

protected:

	/** Fire the weapon */
	virtual void Fire();

	/** Called when the refire rate time has passed while shooting semi auto weapons */
	void FireCooldownExpired();

	/** Fire a projectile towards the target location */
	virtual void FireProjectile(const FVector& TargetLocation);

	/** Calculates the spawn transform for projectiles shot by this weapon */
	FTransform CalculateProjectileSpawnTransform(const FVector& TargetLocation) const;

public:

	/** Returns the first person mesh */
	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; };

	/** Returns the third person mesh */
	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; };

	/** Returns the first person anim instance class */
	const TSubclassOf<UAnimInstance>& GetFirstPersonAnimInstanceClass() const;

	/** Returns the third person anim instance class */
	const TSubclassOf<UAnimInstance>& GetThirdPersonAnimInstanceClass() const;

	/** Returns the magazine size */
	int32 GetMagazineSize() const { return MagazineSize; };

	/** Returns the current bullet count */
	int32 GetBulletCount() const { return CurrentBullets; }

	/** Sets the current magazine bullet count (clamped to [0, MagazineSize]) and updates HUD */
	void SetBulletCount(int32 NewCount);

	/** Returns display name of the ammo type required by this weapon (e.g. "9mm Pistol Ammo") */
	UFUNCTION(BlueprintPure, Category="Ammo")
	FString GetRequiredAmmoDisplayName() const;

	/** Returns total matching ammo rounds currently stored in the owner's Tarkov Backpack */
	UFUNCTION(BlueprintPure, Category="Ammo")
	int32 GetAvailableInventoryAmmo() const;

	/** Consumes up to RoundsNeeded matching ammo from the owner's Tarkov Backpack */
	int32 ConsumeInventoryAmmo(int32 RoundsNeeded);

	/** Configures this weapon instance directly from one of the 7 imported Gun Pack models */
	void ConfigureCustomGunPackArchetype(
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
		USoundBase* InDryFireSound);

	/** Returns the display name of this weapon */
	UFUNCTION(BlueprintPure, Category="Weapon")
	const FString& GetWeaponDisplayName() const { return WeaponDisplayName; }

	/** Returns the subtitle/category of this weapon */
	UFUNCTION(BlueprintPure, Category="Weapon")
	const FString& GetWeaponSubtitle() const { return WeaponSubtitle; }

	/** Returns the custom first person static gun mesh */
	UStaticMeshComponent* GetFirstPersonGunMesh() const { return FirstPersonGunMesh; }

	/** Returns the custom third person static gun mesh */
	UStaticMeshComponent* GetThirdPersonGunMesh() const { return ThirdPersonGunMesh; }
};
