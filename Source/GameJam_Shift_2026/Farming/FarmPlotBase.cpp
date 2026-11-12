// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmPlotBase.h"
#include "GameJam_Shift_2026.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

AFarmPlotBase::AFarmPlotBase()
	: CurrentStage(ECropStage::Empty)
	, bIsWatered(false)
	, StageDuration(8.0f)
	, PlantedSeedID(NAME_None)
	, StageStartTime(0.0f)
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SoilMesh"));
	SoilMesh->SetupAttachment(RootComponent);
	SoilMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SoilMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SoilMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	CropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CropMesh"));
	CropMesh->SetupAttachment(RootComponent);
	CropMesh->SetCollisionProfileName(TEXT("NoCollision"));
	CropMesh->SetVisibility(false);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetBoxExtent(FVector(80.0f, 80.0f, 60.0f));
	InteractionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	InteractionBox->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
}

void AFarmPlotBase::BeginPlay()
{
	Super::BeginPlay();

	// If no mesh was assigned in Blueprint, assign a basic shape so plots are visually discernible in prototype
	if (SoilMesh && SoilMesh->GetStaticMesh() == nullptr)
	{
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			SoilMesh->SetStaticMesh(CubeMesh);
			SoilMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.15f));
			SoilMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 7.5f));
		}
	}

	if (CropMesh && CropMesh->GetStaticMesh() == nullptr)
	{
		if (UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			CropMesh->SetStaticMesh(CylinderMesh);
			CropMesh->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.4f));
			CropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
		}
	}

	UpdateCropMeshForStage(CurrentStage);

	// If spawned in-level with a pre-configured stage, trigger initial Blueprint event
	if (CurrentStage != ECropStage::Empty)
	{
		OnStageChanged(CurrentStage);
	}
}

void AFarmPlotBase::UpdateCropMeshForStage(ECropStage Stage)
{
	if (!CropMesh)
	{
		return;
	}

	switch (Stage)
	{
	case ECropStage::Empty:
		CropMesh->SetVisibility(false);
		break;
	case ECropStage::Planted:
		CropMesh->SetVisibility(true);
		CropMesh->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.1f));
		CropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 12.0f));
		break;
	case ECropStage::Sprout:
		CropMesh->SetVisibility(true);
		CropMesh->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.35f));
		CropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
		break;
	case ECropStage::Growing:
		CropMesh->SetVisibility(true);
		CropMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.65f));
		CropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
		break;
	case ECropStage::Mature:
		CropMesh->SetVisibility(true);
		CropMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.0f));
		CropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
		break;
	case ECropStage::Withered:
		CropMesh->SetVisibility(true);
		CropMesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.2f));
		CropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 15.0f));
		break;
	}
}

void AFarmPlotBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrowthTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool AFarmPlotBase::TillPlot()
{
	if (CurrentStage == ECropStage::Empty && !bIsWatered)
	{
		return false;
	}

	CurrentStage = ECropStage::Empty;
	bIsWatered = false;
	PlantedSeedID = NAME_None;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrowthTimerHandle);
	}

	if (CropMesh)
	{
		CropMesh->SetVisibility(false);
	}

	UpdateCropMeshForStage(CurrentStage);
	OnTilled();
	OnStageChanged(CurrentStage);

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: Plot %s tilled and prepared for planting."), *GetName());
	return true;
}

bool AFarmPlotBase::PlantSeed(FName SeedID, float InStageDuration)
{
	if (!CanPlant())
	{
		UE_LOG(LogGameJam_Shift_2026, Verbose, TEXT("PlantSeed failed: Plot %s is not empty (CurrentStage: %d)."),
			*GetName(), static_cast<int32>(CurrentStage));
		return false;
	}

	if (SeedID.IsNone())
	{
		UE_LOG(LogGameJam_Shift_2026, Warning, TEXT("PlantSeed failed: SeedID is None on plot %s."), *GetName());
		return false;
	}

	PlantedSeedID = SeedID;
	StageDuration = FMath::Max(1.0f, InStageDuration);
	CurrentStage = ECropStage::Planted;
	bIsWatered = false;

	// Automatic default yield mapping based on seed identifier
	const FString SeedStr = SeedID.ToString();
	if (SeedStr.Contains(TEXT("Shotgun")) || SeedStr.Contains(TEXT("Buckshot")))
	{
		HarvestYield.ItemID = FName("Ammo_Shotgun_12G");
		HarvestYield.Quantity = 8;
	}
	else if (SeedStr.Contains(TEXT("Rifle")) || SeedStr.Contains(TEXT("556")))
	{
		HarvestYield.ItemID = FName("Ammo_Rifle_556");
		HarvestYield.Quantity = 30;
	}
	else if (SeedStr.Contains(TEXT("Pistol")) || SeedStr.Contains(TEXT("9mm")))
	{
		HarvestYield.ItemID = FName("Ammo_Pistol_9mm");
		HarvestYield.Quantity = 25;
	}
	else if (SeedStr.Contains(TEXT("Health")) || SeedStr.Contains(TEXT("Medkit")))
	{
		HarvestYield.ItemID = FName("Item_Medkit");
		HarvestYield.Quantity = 1;
	}
	else
	{
		HarvestYield.ItemID = SeedID;
		HarvestYield.Quantity = 5;
	}

	UpdateCropMeshForStage(CurrentStage);
	OnPlanted(SeedID);
	OnStageChanged(CurrentStage);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
			FString::Printf(TEXT(">>> [FARMING] Planted %s! Water this plot to start growth."), *SeedID.ToString()));
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: Planted seed '%s' on plot %s."), *SeedID.ToString(), *GetName());
	return true;
}

bool AFarmPlotBase::WaterPlot()
{
	if (!CanWater())
	{
		UE_LOG(LogGameJam_Shift_2026, Verbose, TEXT("WaterPlot failed on %s: CanWater returned false."), *GetName());
		return false;
	}

	bIsWatered = true;
	StageStartTime = GetWorld()->GetTimeSeconds();

	// Schedule growth transition timer for this stage
	GetWorld()->GetTimerManager().SetTimer(
		GrowthTimerHandle,
		this,
		&AFarmPlotBase::AdvanceGrowthStage,
		StageDuration,
		false
	);

	OnWatered();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			FString::Printf(TEXT(">>> [FARMING] Watered %s! Advancing in %.1f seconds..."), *GetName(), StageDuration));
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: Watered %s. Advancing stage '%d' in %.1f seconds."),
		*GetName(), static_cast<int32>(CurrentStage), StageDuration);
	return true;
}

void AFarmPlotBase::AdvanceGrowthStage()
{
	bIsWatered = false;
	OnSoilDried();

	switch (CurrentStage)
	{
	case ECropStage::Planted:
		CurrentStage = ECropStage::Sprout;
		break;
	case ECropStage::Sprout:
		CurrentStage = ECropStage::Growing;
		break;
	case ECropStage::Growing:
		CurrentStage = ECropStage::Mature;
		break;
	default:
		break;
	}

	UpdateCropMeshForStage(CurrentStage);
	OnStageChanged(CurrentStage);

	if (GEngine)
	{
		const FColor StageColor = (CurrentStage == ECropStage::Mature) ? FColor::Yellow : FColor::Green;
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, StageColor,
			FString::Printf(TEXT(">>> [FARMING] Crop on %s grew to: %s!"), 
				*GetName(), *UEnum::GetValueAsString(CurrentStage)));
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: %s advanced to stage: %s."),
		*GetName(), *UEnum::GetValueAsString(CurrentStage));

	if (CurrentStage == ECropStage::Mature)
	{
		GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);
	}
}

bool AFarmPlotBase::Harvest(FName& OutItemID, int32& OutQuantity)
{
	if (!CanHarvest())
	{
		UE_LOG(LogGameJam_Shift_2026, Verbose, TEXT("Harvest failed on %s: Crop is not mature."), *GetName());
		return false;
	}

	OutItemID = HarvestYield.ItemID.IsNone() ? PlantedSeedID : HarvestYield.ItemID;
	OutQuantity = HarvestYield.Quantity;

	const FName HarvestedItem = OutItemID;
	const int32 HarvestedCount = OutQuantity;

	// Reset plot to empty
	CurrentStage = ECropStage::Empty;
	bIsWatered = false;
	PlantedSeedID = NAME_None;
	GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);

	if (CropMesh)
	{
		CropMesh->SetVisibility(false);
	}

	OnHarvested(HarvestedItem, HarvestedCount);
	UpdateCropMeshForStage(CurrentStage);
	OnStageChanged(CurrentStage);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
			FString::Printf(TEXT(">>> [HARVEST SUCCESS] Harvested +%d %s!"), HarvestedCount, *HarvestedItem.ToString()));
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: Harvested %d of '%s' from %s."),
		HarvestedCount, *HarvestedItem.ToString(), *GetName());
	return true;
}

float AFarmPlotBase::GetStageProgress() const
{
	if (!bIsWatered || StageDuration <= 0.0f)
	{
		return 0.0f;
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - StageStartTime;
	return FMath::Clamp(Elapsed / StageDuration, 0.0f, 1.0f);
}
