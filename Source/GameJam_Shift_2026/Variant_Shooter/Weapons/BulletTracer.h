// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BulletTracer.generated.h"

class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

class UMaterialInterface;

/** One camera-facing smoke puff in a bullet tracer. */
USTRUCT()
struct FTracerSmokePuff
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMaterialBillboardComponent> Component = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Material = nullptr;

	UPROPERTY()
	FVector BaseRelativeLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector DriftVelocity = FVector::ZeroVector;

	UPROPERTY()
	float BaseOpacity = 0.0f;

	UPROPERTY()
	float Lifetime = 0.65f;
};

/**
 * Short-lived, non-colliding bullet trail.
 *
 * A very thin additive core makes the shot readable at night. Soft smoke puffs
 * follow the same muzzle-to-impact line and drift/fade over the next fraction of
 * a second, with a slightly larger cluster at the actual impact point.
 */
UCLASS(NotPlaceable)
class GAMEJAM_SHIFT_2026_API ABulletTracer : public AActor
{
	GENERATED_BODY()

public:
	ABulletTracer();

	/** Spawns a tracer from the muzzle to the projectile's real impact point. */
	static void SpawnTracer(UWorld* World, const FVector& MuzzleLocation, const FVector& ImpactLocation, const FVector& ImpactNormal);

protected:
	virtual void Tick(float DeltaTime) override;

	/** Builds the core, path smoke, muzzle smoke and impact smoke. */
	void InitializeTrace(const FVector& MuzzleLocation, const FVector& ImpactLocation, const FVector& ImpactNormal);

	/** Creates a single world-space material billboard and records its animation state. */
	void AddSmokePuff(const FVector& RelativeLocation, const FVector& DriftVelocity, float Size, float Opacity, float Lifetime);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TracerCore = nullptr;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> ImpactLight = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CoreMID = nullptr;

	UPROPERTY()
	TArray<FTracerSmokePuff> SmokePuffs;

	UPROPERTY(EditDefaultsOnly, Category = "Bullet Tracer")
	TObjectPtr<UMaterialInterface> SmokeMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Bullet Tracer")
	TObjectPtr<UMaterialInterface> CoreMaterial = nullptr;

	/** Hard-referenced so the cooker includes the tracer's cylinder mesh. */
	UPROPERTY(EditDefaultsOnly, Category = "Bullet Tracer")
	TObjectPtr<UStaticMesh> TracerCoreMesh = nullptr;

	float ElapsedTime = 0.0f;
	float CoreLifetime = 0.085f;
	float LightLifetime = 0.10f;
	float InitialLightIntensity = 55.0f;
};
