// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Farming/FarmHouseActor.h"
#include "Farming/FarmBedActor.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFarmHouseActor::AFarmHouseActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = CubeFinder.Object;

	const FLinearColor Timber(0.25f, 0.16f, 0.09f, 1.0f);
	const FLinearColor Wood(0.62f, 0.42f, 0.24f, 1.0f);
	const FLinearColor Trim(0.30f, 0.20f, 0.12f, 1.0f);
	const FLinearColor RoofRed(0.45f, 0.12f, 0.10f, 1.0f);

	// Floor top sits cleanly at Z = +10 (avoids Z-fighting with Lvl_Farm grass at Z = 0)
	AddPiece(TEXT("Floor"), FVector(0.0f, 0.0f, 5.0f), FVector(650.0f, 600.0f, 10.0f), FRotator::ZeroRotator, Timber);

	// Walls: 340 cm tall, 20 cm thick, front wall (+Y) has a 200 x 250 doorway
	AddPiece(TEXT("BackWall"), FVector(0.0f, -290.0f, 170.0f), FVector(650.0f, 20.0f, 340.0f), FRotator::ZeroRotator, Wood);
	AddPiece(TEXT("LeftWall"), FVector(-315.0f, 0.0f, 170.0f), FVector(20.0f, 600.0f, 340.0f), FRotator::ZeroRotator, Wood);
	AddPiece(TEXT("RightWall"), FVector(315.0f, 0.0f, 170.0f), FVector(20.0f, 600.0f, 340.0f), FRotator::ZeroRotator, Wood);
	AddPiece(TEXT("FrontWallLeft"), FVector(-212.5f, 290.0f, 170.0f), FVector(225.0f, 20.0f, 340.0f), FRotator::ZeroRotator, Wood);
	AddPiece(TEXT("FrontWallRight"), FVector(212.5f, 290.0f, 170.0f), FVector(225.0f, 20.0f, 340.0f), FRotator::ZeroRotator, Wood);
	AddPiece(TEXT("DoorLintel"), FVector(0.0f, 290.0f, 295.0f), FVector(200.0f, 20.0f, 90.0f), FRotator::ZeroRotator, Wood);
	AddPiece(TEXT("DoorFrameLeft"), FVector(-105.0f, 292.0f, 125.0f), FVector(10.0f, 24.0f, 250.0f), FRotator::ZeroRotator, Trim);
	AddPiece(TEXT("DoorFrameRight"), FVector(105.0f, 292.0f, 125.0f), FVector(10.0f, 24.0f, 250.0f), FRotator::ZeroRotator, Trim);

	// Porch step leading smoothly up onto the Z = +10 floor
	AddPiece(TEXT("PorchStep"), FVector(0.0f, 360.0f, 2.5f), FVector(200.0f, 120.0f, 5.0f), FRotator::ZeroRotator, Timber);

	// Cozy flat timber ceiling closing the interior above the walls
	AddPiece(TEXT("Ceiling"), FVector(0.0f, 0.0f, 345.0f), FVector(650.0f, 600.0f, 10.0f), FRotator::ZeroRotator, Timber);

	// Pitched roof: two slabs meeting at a ridge (eave at Z = 340, slope 30 degrees)
	AddPiece(TEXT("RoofRight"), FVector(172.5f, 0.0f, 439.5f), FVector(420.0f, 680.0f, 20.0f), FRotator(-30.0f, 0.0f, 0.0f), RoofRed);
	AddPiece(TEXT("RoofLeft"), FVector(-172.5f, 0.0f, 439.5f), FVector(420.0f, 680.0f, 20.0f), FRotator(30.0f, 0.0f, 0.0f), RoofRed);

	InteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	InteriorLight->SetupAttachment(RootComponent);
	InteriorLight->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f));
	InteriorLight->SetLightColor(FLinearColor(1.0f, 0.72f, 0.45f, 1.0f));
	InteriorLight->SetIntensity(3000.0f);
	InteriorLight->SetAttenuationRadius(800.0f);

	SignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignText"));
	SignText->SetupAttachment(RootComponent);
	SignText->SetRelativeLocation(FVector(0.0f, 305.0f, 295.0f));
	SignText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	SignText->SetText(FText::FromString(TEXT("FARMHOUSE")));
	SignText->SetTextRenderColor(FColor(255, 220, 95));
	SignText->SetWorldSize(40.0f);
	SignText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	SignText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFarmHouseActor::AddPiece(const FName Name, const FVector& Location, const FVector& Size, const FRotator& Rotation, const FLinearColor& Color)
{
	UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Piece->SetupAttachment(RootComponent);
	Piece->SetStaticMesh(CubeMesh);
	Piece->SetRelativeLocation(Location);
	Piece->SetRelativeRotation(Rotation);
	Piece->SetRelativeScale3D(Size / 100.0f);
	Piece->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	HousePieces.Add(Piece);
	HousePieceColors.Add(Color);
}

void AFarmHouseActor::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (BaseMat)
	{
		for (int32 i = 0; i < HousePieces.Num(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = HousePieces[i]->CreateDynamicMaterialInstance(0, BaseMat))
			{
				MID->SetVectorParameterValue(TEXT("Color"), HousePieceColors[i]);
				MID->SetVectorParameterValue(TEXT("Emissive"), HousePieceColors[i] * 0.05f);
			}
		}
	}

	if (!Bed && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Bed = GetWorld()->SpawnActor<AFarmBedActor>(AFarmBedActor::StaticClass(), GetActorLocation() + FVector(-160.0f, -140.0f, 10.0f), FRotator::ZeroRotator, Params);
		if (Bed)
		{
			Bed->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}
