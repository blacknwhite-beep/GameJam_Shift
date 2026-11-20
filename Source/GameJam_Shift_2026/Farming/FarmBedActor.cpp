// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Farming/FarmBedActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFarmBedActor::AFarmBedActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = CubeFinder.Object;

	const FLinearColor WoodFrame(0.36f, 0.22f, 0.12f, 1.0f);
	const FLinearColor WhiteSheet(0.92f, 0.92f, 0.88f, 1.0f);
	const FLinearColor Blanket(0.62f, 0.06f, 0.08f, 1.0f);
	const FLinearColor Pillow(0.96f, 0.96f, 0.94f, 1.0f);

	AddPiece(TEXT("BedFrame"), FVector(0.0f, 0.0f, 15.0f), FVector(200.0f, 100.0f, 30.0f), WoodFrame);
	AddPiece(TEXT("Headboard"), FVector(-100.0f, 0.0f, 40.0f), FVector(10.0f, 100.0f, 80.0f), WoodFrame);
	AddPiece(TEXT("Footboard"), FVector(100.0f, 0.0f, 20.0f), FVector(10.0f, 100.0f, 40.0f), WoodFrame);
	AddPiece(TEXT("Mattress"), FVector(0.0f, 0.0f, 40.0f), FVector(190.0f, 90.0f, 20.0f), WhiteSheet);
	AddPiece(TEXT("Blanket"), FVector(31.5f, 0.0f, 53.0f), FVector(127.0f, 94.0f, 6.0f), Blanket);
	AddPiece(TEXT("Pillow"), FVector(-75.0f, 0.0f, 57.0f), FVector(40.0f, 60.0f, 14.0f), Pillow);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetBoxExtent(FVector(115.0f, 60.0f, 50.0f));
	InteractionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PromptText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PromptText"));
	PromptText->SetupAttachment(RootComponent);
	PromptText->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	PromptText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	PromptText->SetText(FText::FromString(TEXT("[E] SLEEP")));
	PromptText->SetTextRenderColor(FColor(255, 230, 150));
	PromptText->SetWorldSize(20.0f);
	PromptText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	PromptText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFarmBedActor::AddPiece(const FName Name, const FVector& Location, const FVector& Size, const FLinearColor& Color)
{
	UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Piece->SetupAttachment(RootComponent);
	Piece->SetStaticMesh(CubeMesh);
	Piece->SetRelativeLocation(Location);
	Piece->SetRelativeScale3D(Size / 100.0f);
	Piece->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BedPieces.Add(Piece);
	BedPieceColors.Add(Color);
}

void AFarmBedActor::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (!BaseMat)
	{
		return;
	}

	for (int32 i = 0; i < BedPieces.Num(); ++i)
	{
		if (UMaterialInstanceDynamic* MID = BedPieces[i]->CreateDynamicMaterialInstance(0, BaseMat))
		{
			MID->SetVectorParameterValue(TEXT("Color"), BedPieceColors[i]);
			MID->SetVectorParameterValue(TEXT("Emissive"), BedPieceColors[i] * 0.05f);
		}
	}
}

FVector AFarmBedActor::GetSleepAnchorLocation() const
{
	// Positioned cleanly above the pillow (pillow top is Z=64) looking down the blanket
	return GetActorTransform().TransformPosition(FVector(-65.0f, 0.0f, 74.0f));
}

FVector AFarmBedActor::GetWakeUpLocation() const
{
	return GetActorTransform().TransformPosition(FVector(0.0f, 110.0f, 98.0f));
}

FRotator AFarmBedActor::GetSleepFacingRotation() const
{
	return FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
}
