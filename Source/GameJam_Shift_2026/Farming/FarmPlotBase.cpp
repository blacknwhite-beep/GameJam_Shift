// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmPlotBase.h"
#include "GameJam_Shift_2026.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

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

	Mesh_Sprout = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh_Sprout"));
	Mesh_Sprout->SetupAttachment(RootComponent);
	Mesh_Sprout->SetCollisionProfileName(TEXT("NoCollision"));
	Mesh_Sprout->SetVisibility(false);

	Mesh_Mature = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh_Mature"));
	Mesh_Mature->SetupAttachment(RootComponent);
	Mesh_Mature->SetCollisionProfileName(TEXT("NoCollision"));
	Mesh_Mature->SetVisibility(false);

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

	if (SoilMesh && DrySoilMaterial && !bIsWatered)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
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
	const bool bHasBespokeMeshes = (Mesh_Sprout && Mesh_Sprout->GetStaticMesh() != nullptr)
		|| (Mesh_Mature && Mesh_Mature->GetStaticMesh() != nullptr);

	if (bHasBespokeMeshes)
	{
		if (CropMesh)
		{
			CropMesh->SetVisibility(false);
		}

		switch (Stage)
		{
		case ECropStage::Empty:
			if (Mesh_Sprout) { Mesh_Sprout->SetVisibility(false); }
			if (Mesh_Mature) { Mesh_Mature->SetVisibility(false); }
			break;
		case ECropStage::Planted:
			if (Mesh_Sprout)
			{
				Mesh_Sprout->SetVisibility(true);
				Mesh_Sprout->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.45f));
			}
			if (Mesh_Mature) { Mesh_Mature->SetVisibility(false); }
			break;
		case ECropStage::Sprout:
			if (Mesh_Sprout)
			{
				Mesh_Sprout->SetVisibility(true);
				Mesh_Sprout->SetRelativeScale3D(FVector(0.85f, 0.85f, 0.85f));
			}
			if (Mesh_Mature) { Mesh_Mature->SetVisibility(false); }
			break;
		case ECropStage::Growing:
			if (Mesh_Sprout)
			{
				Mesh_Sprout->SetVisibility(true);
				Mesh_Sprout->SetRelativeScale3D(FVector(1.25f, 1.25f, 1.3f));
			}
			if (Mesh_Mature) { Mesh_Mature->SetVisibility(false); }
			break;
		case ECropStage::Mature:
			if (Mesh_Sprout) { Mesh_Sprout->SetVisibility(false); }
			if (Mesh_Mature)
			{
				Mesh_Mature->SetVisibility(true);
				Mesh_Mature->SetRelativeScale3D(FVector::OneVector);
			}
			break;
		case ECropStage::Withered:
			if (Mesh_Sprout)
			{
				Mesh_Sprout->SetVisibility(true);
				Mesh_Sprout->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.3f));
			}
			if (Mesh_Mature) { Mesh_Mature->SetVisibility(false); }
			break;
		}
		return;
	}

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

	if (SoilMesh && DrySoilMaterial && !bIsWatered)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
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
	if (!bPreserveConfiguredYield)
	{
		StageDuration = FMath::Max(1.0f, InStageDuration);
	}
	CurrentStage = ECropStage::Planted;
	bIsWatered = false;

	if (!bPreserveConfiguredYield)
	{
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
	}

	if (SoilMesh && DrySoilMaterial && !bIsWatered)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
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

	if (SoilMesh && WetSoilMaterial)
	{
		SoilMesh->SetMaterial(0, WetSoilMaterial);
	}
	if (WaterSplashVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WaterSplashVFX, GetActorLocation() + FVector(0.0f, 0.0f, 25.0f));
	}

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
	if (SoilMesh && DrySoilMaterial)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
	}
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

	if (SoilMesh && DrySoilMaterial && !bIsWatered)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
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
		if (MatureSparkleVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), MatureSparkleVFX, GetActorLocation() + FVector(0.0f, 0.0f, 80.0f));
		}
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

	if (SoilMesh && DrySoilMaterial)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
	}
	if (HarvestPopSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HarvestPopSound, GetActorLocation() + FVector(0.0f, 0.0f, 40.0f));
	}

	const FString FloatingMsg = !HarvestFloatingText.IsEmpty()
		? HarvestFloatingText
		: FString::Printf(TEXT("+%d %s"), HarvestedCount, *HarvestedItem.ToString());
	SpawnFloatingHarvestText(FloatingMsg, FColor(255, 225, 90));

	if (CropMesh)
	{
		CropMesh->SetVisibility(false);
	}

	OnHarvested(HarvestedItem, HarvestedCount);
	if (SoilMesh && DrySoilMaterial && !bIsWatered)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
	}

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

void AFarmPlotBase::SpawnFloatingHarvestText(const FString& Text, const FColor& Color)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* TextActor = World->SpawnActor<AActor>(AActor::StaticClass(), GetActorLocation() + FVector(0.0f, 0.0f, 110.0f), FRotator::ZeroRotator, Params);
	if (!TextActor)
	{
		return;
	}

	UTextRenderComponent* TextComp = NewObject<UTextRenderComponent>(TextActor, TEXT("FloatingHarvestText"));
	TextComp->RegisterComponent();
	TextActor->SetRootComponent(TextComp);
	TextComp->SetText(FText::FromString(Text));
	TextComp->SetTextRenderColor(Color);
	TextComp->SetWorldSize(28.0f);
	TextComp->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	TextComp->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	TextComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Gently drift upward by facing the player camera and self-destructing after 1.6s
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector ToCam = (PC->PlayerCameraManager->GetCameraLocation() - TextActor->GetActorLocation()).GetSafeNormal2D();
			TextActor->SetActorRotation(ToCam.Rotation());
		}
	}
	TextActor->SetLifeSpan(1.6f);
}
