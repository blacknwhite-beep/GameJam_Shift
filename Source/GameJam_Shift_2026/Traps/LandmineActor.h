// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandmineActor.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class USoundBase;

/**
 * One-shot proximity landmine.
 *
 * The mine deliberately does not use the projectile/bullet damage path: it calls
 * AZombieEnemy::KillByExplosion so zombies keep all limbs and become airborne
 * ragdolls instead of spawning dismemberment gibs.
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API ALandmineActor : public AActor
{
	GENERATED_BODY()

public:
	ALandmineActor();

	/** Stable pairing ID used to keep Farm and Combat copies synchronized. */
	FGuid GetMirrorPairId() const { return MirrorPairId; }
	bool IsCombatMirror() const { return bIsCombatMirror; }
	bool IsExploded() const { return bExploded; }
	ALandmineActor* GetLinkedMirror() const { return LinkedMirrorMine.Get(); }

	/** Links two dimension copies and copies the source pairing ID to the Combat mirror. */
	void SetLinkedMirror(ALandmineActor* Mirror);
	void ConfigureAsMirror(ALandmineActor* Source);

	/** Removes the paired copy when either mine explodes or its source disappears. */
	void DestroyLinkedMirror();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Arms the pressure trigger after the player has had time to move away. */
	void ArmMine();

	/** Starts the one-shot explosion sequence. */
	void Explode();

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landmine", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MineBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landmine", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PressurePlate = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landmine", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WarningLightMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landmine", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> TriggerSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landmine", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ExplosionFlash = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landmine", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> ExplosionLight = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlateMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WarningMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashMID = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Landmine")
	TObjectPtr<USoundBase> ExplosionSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmine", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ArmDelay = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmine", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", Units = "cm"))
	float TriggerRadius = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmine", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", Units = "cm"))
	float ExplosionRadius = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmine", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", Units = "cm/s"))
	float UpwardLaunchSpeed = 3600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmine", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
	float RadialLaunchSpeed = 2100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmine", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PropImpulseForce = 70000.0f;

	FTimerHandle ArmTimer;
	bool bArmed = false;
	bool bExploded = false;
	bool bIsCombatMirror = false;
	FGuid MirrorPairId;
	TWeakObjectPtr<ALandmineActor> LinkedMirrorMine;
	float BlinkTime = 0.0f;
	float ExplosionElapsed = 0.0f;
	float ExplosionVisualDuration = 0.50f;
};
