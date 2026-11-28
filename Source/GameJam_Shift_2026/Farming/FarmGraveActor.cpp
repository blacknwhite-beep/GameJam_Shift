// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmGraveActor.h"
#include "DeadCivilianBody.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"

AFarmGraveActor::AFarmGraveActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	// 6ft+ grave interaction box (~260 x 150 cm footprint, 130 cm tall so looking down at the grave always hits)
	GraveInteractBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("GraveInteractBounds"));
	GraveInteractBounds->SetupAttachment(RootScene);
	GraveInteractBounds->SetBoxExtent(FVector(135.0f, 82.0f, 65.0f));
	GraveInteractBounds->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	GraveInteractBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GraveInteractBounds->SetCollisionObjectType(ECC_WorldDynamic);
	GraveInteractBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	GraveInteractBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GraveInteractBounds->SetCanEverAffectNavigation(false);
}

void AFarmGraveActor::BeginPlay()
{
	Super::BeginPlay();
	BuildGraveVisuals();
}

void AFarmGraveActor::BuildGraveVisuals()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!CubeMesh || !BaseMat)
	{
		return;
	}

	auto MakeMat = [&](const FLinearColor& Col, const FLinearColor& Emissive = FLinearColor::Black) -> UMaterialInstanceDynamic*
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Col);
			MID->SetVectorParameterValue(TEXT("Emissive"), Emissive);
		}
		return MID;
	};

	UMaterialInstanceDynamic* PitVoidMat   = MakeMat(FLinearColor(0.012f, 0.008f, 0.006f, 1.0f)); // Pitch-dark 6ft grave depth
	UMaterialInstanceDynamic* InnerWallMat = MakeMat(FLinearColor(0.055f, 0.036f, 0.022f, 1.0f)); // Damp dark excavated subsoil
	UMaterialInstanceDynamic* DirtRimMat   = MakeMat(FLinearColor(0.14f,  0.095f, 0.055f, 1.0f)); // Freshly dug brown topsoil
	UMaterialInstanceDynamic* DirtMoundMat = MakeMat(FLinearColor(0.16f,  0.108f, 0.062f, 1.0f)); // Excavated dirt mound
	UMaterialInstanceDynamic* WoodMat      = MakeMat(FLinearColor(0.24f,  0.15f,  0.08f,  1.0f)); // Shovel handle
	UMaterialInstanceDynamic* MetalMat     = MakeMat(FLinearColor(0.22f,  0.23f,  0.25f,  1.0f)); // Shovel blade

	auto AddPart = [&](UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FRotator& Rot, UMaterialInterface* Mat, bool bCollision = false)
	{
		if (!Mesh)
		{
			return;
		}
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetStaticMesh(Mesh);
		Comp->SetRelativeLocation(Loc);
		Comp->SetRelativeRotation(Rot);
		Comp->SetRelativeScale3D(Scale);
		Comp->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bCollision)
		{
			Comp->SetCollisionResponseToAllChannels(ECR_Block);
		}
		Comp->SetCastShadow(true);
		if (Mat)
		{
			Comp->SetMaterial(0, Mat);
		}
		Comp->SetupAttachment(RootScene);
		Comp->RegisterComponent();
	};

	// Grave opening: 230 cm long (X: -115..+115, >6ft) x 110 cm wide (Y: -55..+55)
	// 1. Pitch-dark grave pit bottom (sits just above grass at Z=2cm, surrounded by raised earthen rim)
	AddPart(CubeMesh, FVector(0.0f, 0.0f, 2.0f), FVector(2.30f, 1.10f, 0.04f), FRotator::ZeroRotator, PitVoidMat, false);

	// 2. Four steep dark inner earthen walls rising from the pit bottom to the dug rim (Z=0..26cm)
	// Long sides (North/South along X)
	AddPart(CubeMesh, FVector(0.0f,  60.0f, 13.0f), FVector(2.44f, 0.14f, 0.26f), FRotator::ZeroRotator, InnerWallMat, false);
	AddPart(CubeMesh, FVector(0.0f, -60.0f, 13.0f), FVector(2.44f, 0.14f, 0.26f), FRotator::ZeroRotator, InnerWallMat, false);
	// Short ends (East/West along Y)
	AddPart(CubeMesh, FVector( 120.0f, 0.0f, 13.0f), FVector(0.14f, 1.32f, 0.26f), FRotator::ZeroRotator, InnerWallMat, false);
	AddPart(CubeMesh, FVector(-120.0f, 0.0f, 13.0f), FVector(0.14f, 1.32f, 0.26f), FRotator::ZeroRotator, InnerWallMat, false);

	// 3. Sloped outer topsoil berms around all 4 sides so the grave blends naturally into the grass
	AddPart(CubeMesh, FVector(0.0f,  84.0f, 9.0f),  FVector(2.85f, 0.48f, 0.22f), FRotator(0.0f, 0.0f,  22.0f), DirtRimMat, false);
	AddPart(CubeMesh, FVector(0.0f, -84.0f, 9.0f),  FVector(2.85f, 0.48f, 0.22f), FRotator(0.0f, 0.0f, -22.0f), DirtRimMat, false);
	AddPart(CubeMesh, FVector( 144.0f, 0.0f, 9.0f), FVector(0.48f, 1.65f, 0.22f), FRotator(-22.0f, 0.0f, 0.0f), DirtRimMat, false);
	AddPart(CubeMesh, FVector(-144.0f, 0.0f, 9.0f), FVector(0.48f, 1.65f, 0.22f), FRotator( 22.0f, 0.0f, 0.0f), DirtRimMat, false);

	// 4. Big freshly-excavated pile of dirt heaped beside the back edge (-Y) of the grave
	if (SphereMesh)
	{
		AddPart(SphereMesh, FVector(-35.0f, -135.0f, 16.0f), FVector(1.55f, 0.95f, 0.58f), FRotator(0.0f,  12.0f, 0.0f), DirtMoundMat, true);
		AddPart(SphereMesh, FVector( 45.0f, -128.0f, 14.0f), FVector(1.30f, 0.85f, 0.50f), FRotator(0.0f, -18.0f, 0.0f), DirtMoundMat, true);
		AddPart(SphereMesh, FVector(  5.0f, -148.0f, 22.0f), FVector(1.10f, 0.72f, 0.46f), FRotator(0.0f,   5.0f, 0.0f), DirtRimMat, false);
		// Smaller scattered dirt clods around the rim
		AddPart(SphereMesh, FVector(-155.0f,  62.0f, 6.0f), FVector(0.42f, 0.35f, 0.18f), FRotator::ZeroRotator, DirtRimMat, false);
		AddPart(SphereMesh, FVector( 152.0f, -55.0f, 7.0f), FVector(0.48f, 0.38f, 0.20f), FRotator::ZeroRotator, DirtRimMat, false);
	}

	// 5. Shovel stabbed upright into the excavated dirt mound beside the grave
	if (CylinderMesh)
	{
		AddPart(CylinderMesh, FVector(65.0f, -135.0f, 68.0f), FVector(0.045f, 0.045f, 1.15f), FRotator(14.0f, 22.0f, -8.0f), WoodMat, false);
		AddPart(CubeMesh,     FVector(55.0f, -132.0f, 24.0f), FVector(0.24f,  0.04f,  0.30f), FRotator(14.0f, 22.0f, -8.0f), MetalMat, false);
	}
}

void AFarmGraveActor::DumpBodyIntoGrave(ADeadCivilianBody* Body)
{
	if (!Body)
	{
		return;
	}

	// Place the body inside the 230 x 110 cm grave pit, stacking slightly as more bodies pile up
	const float OffsetX = FMath::FRandRange(-28.0f, 28.0f);
	const float OffsetY = FMath::FRandRange(-18.0f, 18.0f);
	const float StackZ = 5.0f + FMath::Min(DumpedBodyCount, 6) * 10.0f;

	const FVector GraveDropWorld = GetActorTransform().TransformPosition(FVector(OffsetX, OffsetY, StackZ));
	const float GraveYaw = GetActorRotation().Yaw + (FMath::RandBool() ? 0.0f : 180.0f) + FMath::FRandRange(-14.0f, 14.0f);

	Body->DumpIntoGrave(GraveDropWorld, GraveYaw);
	++DumpedBodyCount;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 3.0f, FColor(220, 70, 60),
			FString::Printf(TEXT("[GRAVE] Dumped body into the grave behind the barn (%d buried)"), DumpedBodyCount)
		);
	}
}
