// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletTracer.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABulletTracer::ABulletTracer()
{
	PrimaryActorTick.bCanEverTick = true;

	// Constructor-time hard references make the cooker include these assets even
	// though the tracer actor itself is spawned dynamically at runtime.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeMaterialFinder(
		TEXT("/Game/Dev_Blueprint/VFX/Materials/M_BulletSmoke.M_BulletSmoke"));
	if (SmokeMaterialFinder.Succeeded())
	{
		SmokeMaterial = SmokeMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoreMaterialFinder(
		TEXT("/Game/Dev_Blueprint/VFX/Materials/M_BulletTracerCore.M_BulletTracerCore"));
	if (CoreMaterialFinder.Succeeded())
	{
		CoreMaterial = CoreMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CoreMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CoreMeshFinder.Succeeded())
	{
		TracerCoreMesh = CoreMeshFinder.Object;
	}

	USceneComponent* TracerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TracerRoot"));
	RootComponent = TracerRoot;

	SetActorEnableCollision(false);
	SetCanBeDamaged(false);

	TracerCore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracerCore"));
	TracerCore->SetupAttachment(TracerRoot);
	TracerCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TracerCore->SetCastShadow(false);
	TracerCore->SetReceivesDecals(false);
	TracerCore->SetTranslucentSortPriority(8);
	TracerCore->SetVisibility(false);

	ImpactLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ImpactLight"));
	ImpactLight->SetupAttachment(TracerRoot);
	ImpactLight->SetIntensityUnits(ELightUnits::Candelas);
	ImpactLight->SetIntensity(InitialLightIntensity);
	ImpactLight->SetAttenuationRadius(240.0f);
	ImpactLight->SetLightColor(FColor(255, 214, 150));
	ImpactLight->SetCastShadows(false);
	ImpactLight->SetVisibility(false);
}

void ABulletTracer::SpawnTracer(UWorld* World, const FVector& MuzzleLocation, const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	if (!World || MuzzleLocation.Equals(ImpactLocation, 2.0f))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ABulletTracer* Tracer = World->SpawnActor<ABulletTracer>(
		ABulletTracer::StaticClass(),
		FTransform(MuzzleLocation),
		SpawnParams
	);

	if (Tracer)
	{
		Tracer->InitializeTrace(MuzzleLocation, ImpactLocation, ImpactNormal);
	}
}

void ABulletTracer::InitializeTrace(const FVector& MuzzleLocation, const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	SetActorLocation(MuzzleLocation);

	if (!SmokeMaterial)
	{
		SmokeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/VFX/Materials/M_BulletSmoke.M_BulletSmoke"));
	}
	if (!CoreMaterial)
	{
		CoreMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/VFX/Materials/M_BulletTracerCore.M_BulletTracerCore"));
	}

	if (!TracerCoreMesh)
	{
		TracerCoreMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}

	const FVector ShotDelta = ImpactLocation - MuzzleLocation;
	const float ShotLength = ShotDelta.Size();
	const FVector ShotDirection = ShotDelta.GetSafeNormal();
	const FVector SafeImpactNormal = ImpactNormal.GetSafeNormal();

	// Thin, warm additive core: visible against a night sky without looking like a laser.
	if (CoreMaterial && TracerCoreMesh && TracerCore)
	{
		CoreMID = UMaterialInstanceDynamic::Create(CoreMaterial, this);
		if (CoreMID)
		{
			CoreMID->SetScalarParameterValue(TEXT("Opacity"), 0.62f);
			TracerCore->SetStaticMesh(TracerCoreMesh);
			TracerCore->SetMaterial(0, CoreMID);
			TracerCore->SetRelativeLocation(ShotDelta * 0.5f);
			TracerCore->SetRelativeRotation(FQuat::FindBetweenNormals(FVector::UpVector, ShotDirection).Rotator());
			// The engine cylinder is 100 cm tall and 100 cm across before scaling.
			TracerCore->SetRelativeScale3D(FVector(0.012f, 0.012f, FMath::Max(ShotLength / 100.0f, 0.01f)));
			TracerCore->SetVisibility(true);
		}
	}

	if (SmokeMaterial)
	{
		// A faint, broken smoky trail. The spacing stays roughly constant in world
		// space, so close shots do not turn into one large opaque blob.
		const int32 PathPuffCount = 9;
		for (int32 Index = 0; Index < PathPuffCount; ++Index)
		{
			const float Alpha = 0.08f + (static_cast<float>(Index) / (PathPuffCount - 1)) * 0.86f;
			const FVector LateralJitter = FMath::VRand() * FMath::FRandRange(1.5f, 5.0f);
			const FVector RelativeLocation = ShotDelta * Alpha + LateralJitter;
			const FVector Drift = SafeImpactNormal * FMath::FRandRange(1.0f, 4.0f)
				+ FVector(FMath::FRandRange(-4.0f, 4.0f), FMath::FRandRange(-4.0f, 4.0f), FMath::FRandRange(5.0f, 12.0f));

			AddSmokePuff(
				RelativeLocation,
				Drift,
				FMath::FRandRange(10.0f, 19.0f),
				FMath::FRandRange(0.07f, 0.12f),
				FMath::FRandRange(0.48f, 0.72f)
			);
		}

		// One small puff right at the barrel sells the muzzle smoke without a large cloud.
		for (int32 Index = 0; Index < 1; ++Index)
		{
			const FVector RelativeLocation = ShotDirection * FMath::FRandRange(5.0f, 16.0f) + FMath::VRand() * 4.0f;
			const FVector Drift = ShotDirection * FMath::FRandRange(5.0f, 12.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(8.0f, 16.0f));
			AddSmokePuff(RelativeLocation, Drift, FMath::FRandRange(13.0f, 22.0f), FMath::FRandRange(0.13f, 0.19f), FMath::FRandRange(0.58f, 0.78f));
		}

		// Slightly larger impact cluster makes the landing point apparent in low light.
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FVector RelativeLocation = ShotDelta + SafeImpactNormal * FMath::FRandRange(1.0f, 7.0f) + FMath::VRand() * 6.0f;
			const FVector Drift = SafeImpactNormal * FMath::FRandRange(7.0f, 16.0f)
				+ FVector(FMath::FRandRange(-8.0f, 8.0f), FMath::FRandRange(-8.0f, 8.0f), FMath::FRandRange(8.0f, 18.0f));
			AddSmokePuff(RelativeLocation, Drift, FMath::FRandRange(19.0f, 34.0f), FMath::FRandRange(0.17f, 0.26f), FMath::FRandRange(0.62f, 0.86f));
		}
	}

	// A brief impact pop catches nearby geometry and reads clearly at night.
	if (ImpactLight)
	{
		ImpactLight->SetRelativeLocation(ShotDelta + SafeImpactNormal * 3.0f);
		ImpactLight->SetIntensity(InitialLightIntensity);
		ImpactLight->SetVisibility(true);
	}

	SetLifeSpan(1.05f);
}

void ABulletTracer::AddSmokePuff(const FVector& RelativeLocation, const FVector& DriftVelocity, float Size, float Opacity, float Lifetime)
{
	UMaterialBillboardComponent* Puff = NewObject<UMaterialBillboardComponent>(this);
	if (!Puff)
	{
		return;
	}

	Puff->SetupAttachment(RootComponent);
	Puff->RegisterComponent();
	Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puff->SetCastShadow(false);
	Puff->SetReceivesDecals(false);
	Puff->SetTranslucentSortPriority(6);
	Puff->SetRelativeLocation(RelativeLocation);
	Puff->SetRelativeScale3D(FVector::OneVector);

	UMaterialInstanceDynamic* PuffMID = UMaterialInstanceDynamic::Create(SmokeMaterial, this);
	if (!PuffMID)
	{
		Puff->DestroyComponent();
		return;
	}

	PuffMID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	PuffMID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.72f, 0.76f, 0.82f, 1.0f));
	// Material billboard BaseSize is a half-extent, so halve the intended diameter.
	Puff->AddElement(PuffMID, nullptr, false, Size * 0.5f, Size * 0.5f, nullptr);
	Puff->SetVisibility(true);

	FTracerSmokePuff& PuffState = SmokePuffs.AddDefaulted_GetRef();
	PuffState.Component = Puff;
	PuffState.Material = PuffMID;
	PuffState.BaseRelativeLocation = RelativeLocation;
	PuffState.DriftVelocity = DriftVelocity;
	PuffState.BaseOpacity = Opacity;
	PuffState.Lifetime = Lifetime;
}

void ABulletTracer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;

	if (TracerCore && CoreMID)
	{
		const float CoreAlpha = ElapsedTime < CoreLifetime
			? 1.0f - FMath::Clamp(ElapsedTime / CoreLifetime, 0.0f, 1.0f)
			: 0.0f;
		CoreMID->SetScalarParameterValue(TEXT("Opacity"), 0.62f * CoreAlpha);
		if (CoreAlpha <= 0.0f)
		{
			TracerCore->SetVisibility(false);
		}
	}

	if (ImpactLight)
	{
		const float LightAlpha = ElapsedTime < LightLifetime
			? 1.0f - FMath::Clamp(ElapsedTime / LightLifetime, 0.0f, 1.0f)
			: 0.0f;
		ImpactLight->SetIntensity(InitialLightIntensity * LightAlpha * LightAlpha);
		if (LightAlpha <= 0.0f)
		{
			ImpactLight->SetVisibility(false);
		}
	}

	for (const FTracerSmokePuff& Puff : SmokePuffs)
	{
		if (!Puff.Component || !Puff.Material)
		{
			continue;
		}

		const float Age = FMath::Clamp(ElapsedTime / Puff.Lifetime, 0.0f, 1.0f);
		const float FadeIn = FMath::SmoothStep(0.0f, 0.06f, ElapsedTime);
		const float FadeOut = 1.0f - FMath::SmoothStep(0.12f, 1.0f, Age);
		const float Opacity = Puff.BaseOpacity * FadeIn * FadeOut;

		Puff.Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		Puff.Component->SetRelativeLocation(Puff.BaseRelativeLocation + Puff.DriftVelocity * ElapsedTime);
		Puff.Component->SetRelativeScale3D(FVector(1.0f + Age * 0.55f));

		if (Opacity <= 0.001f)
		{
			Puff.Component->SetVisibility(false);
		}
	}
}
