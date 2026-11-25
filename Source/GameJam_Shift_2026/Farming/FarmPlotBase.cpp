// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmPlotBase.h"
#include "GameJam_Shift_2026.h"
#include "DimensionShiftSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

AFarmPlotBase::AFarmPlotBase()
	: CurrentStage(ECropStage::Empty)
	, bIsWatered(false)
	, bAwaitingSleepGrowth(false)
	, StageDuration(8.0f)
	, PlantedSeedID(NAME_None)
	, StageStartTime(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

	// Floating 3D nighttime health bar (hidden by default; shown ONLY in Combat world during nighttime when planted)
	HealthBarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HealthBarRoot"));
	HealthBarRoot->SetupAttachment(RootComponent);
	HealthBarRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 128.0f));
	HealthBarRoot->SetVisibility(false, true);

	HealthBarBgMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarBgMesh"));
	HealthBarBgMesh->SetupAttachment(HealthBarRoot);
	HealthBarBgMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarBgMesh->SetCastShadow(false);
	HealthBarBgMesh->SetRelativeLocation(FVector(-1.0f, 0.0f, 0.0f));
	HealthBarBgMesh->SetRelativeScale3D(FVector(0.025f, 0.92f, 0.14f));
	HealthBarBgMesh->SetVisibility(false);

	HealthBarFillMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarFillMesh"));
	HealthBarFillMesh->SetupAttachment(HealthBarRoot);
	HealthBarFillMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarFillMesh->SetCastShadow(false);
	HealthBarFillMesh->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f));
	HealthBarFillMesh->SetRelativeScale3D(FVector(0.03f, 0.86f, 0.095f));
	HealthBarFillMesh->SetVisibility(false);
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

	// Initialize 3D nighttime health bar meshes & materials
	UStaticMesh* CubeAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseCropMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (CubeAsset && HealthBarBgMesh && HealthBarFillMesh)
	{
		HealthBarBgMesh->SetStaticMesh(CubeAsset);
		HealthBarFillMesh->SetStaticMesh(CubeAsset);
		if (BaseCropMat)
		{
			if (UMaterialInstanceDynamic* BgMID = UMaterialInstanceDynamic::Create(BaseCropMat, this))
			{
				BgMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.03f, 0.03f, 0.04f, 1.0f));
				BgMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.01f, 0.01f, 0.015f, 1.0f));
				HealthBarBgMesh->SetMaterial(0, BgMID);
			}
			HealthBarFillMID = UMaterialInstanceDynamic::Create(BaseCropMat, this);
			if (HealthBarFillMID)
			{
				HealthBarFillMesh->SetMaterial(0, HealthBarFillMID);
			}
		}
	}

	// Only Combat-world plots tick to billboard the nighttime crop health bar toward the player
	SetActorTickEnabled(IsCombatWorldPlot());
	UpdateHealthBarVisuals();

	// If spawned in-level with a pre-configured stage, trigger initial Blueprint event
	if (CurrentStage != ECropStage::Empty)
	{
		OnStageChanged(CurrentStage);
	}
}

void AFarmPlotBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsCombatWorldPlot() || !HealthBarRoot)
	{
		return;
	}

	bool bIsNight = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bIsNight = Sub->IsNightWindow() && !Sub->bIsStartMenuActive;
			}
		}
	}

	const bool bShouldShow = bIsNight && HasLivingCrop();
	if (HealthBarBgMesh && HealthBarBgMesh->IsVisible() != bShouldShow)
	{
		HealthBarRoot->SetVisibility(bShouldShow, true);
		HealthBarBgMesh->SetVisibility(bShouldShow);
		if (HealthBarFillMesh)
		{
			HealthBarFillMesh->SetVisibility(bShouldShow);
		}
	}

	if (bShouldShow)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				const FVector ToCam = (PC->PlayerCameraManager->GetCameraLocation() - HealthBarRoot->GetComponentLocation()).GetSafeNormal2D();
				if (!ToCam.IsNearlyZero())
				{
					HealthBarRoot->SetWorldRotation(ToCam.Rotation());
				}
			}
		}
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
	bAwaitingSleepGrowth = false;
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

void AFarmPlotBase::ConfigureVisualsForSeed(FName SeedID)
{
	// Ensure Sprout mesh is always loaded so all plots show proper 3D sprouts during growth
	if (Mesh_Sprout && Mesh_Sprout->GetStaticMesh() == nullptr)
	{
		if (UStaticMesh* SproutAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Meshes/SM_Crop_Sprout.SM_Crop_Sprout")))
		{
			Mesh_Sprout->SetStaticMesh(SproutAsset);
		}
	}

	const FString SeedStr = SeedID.ToString();
	if (SeedStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase) || SeedStr.Contains(TEXT("Buckshot"), ESearchCase::IgnoreCase))
	{
		HarvestYield.ItemID = FName("Item_Ammo_Shotgun");
		HarvestYield.Quantity = 16;
		HarvestFloatingText = TEXT("+16 Shotgun Shells (12G)");

		if (Mesh_Mature)
		{
			if (UStaticMesh* MatureAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Meshes/SM_Crop_ShotgunCorn.SM_Crop_ShotgunCorn")))
			{
				Mesh_Mature->SetStaticMesh(MatureAsset);
				const int32 NumMats = Mesh_Mature->GetNumMaterials();
				for (int32 SlotIdx = 0; SlotIdx < NumMats; ++SlotIdx)
				{
					Mesh_Mature->SetMaterial(SlotIdx, nullptr);
				}
			}
		}
	}
	else if (SeedStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase) || SeedStr.Contains(TEXT("556"), ESearchCase::IgnoreCase))
	{
		HarvestYield.ItemID = FName("Item_Ammo_Rifle");
		HarvestYield.Quantity = 30;
		HarvestFloatingText = TEXT("+30 Rifle Ammo (5.56mm)");

		if (Mesh_Mature)
		{
			if (UStaticMesh* MatureAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Meshes/SM_Crop_RifleWheat.SM_Crop_RifleWheat")))
			{
				Mesh_Mature->SetStaticMesh(MatureAsset);
				const int32 NumMats = Mesh_Mature->GetNumMaterials();
				for (int32 SlotIdx = 0; SlotIdx < NumMats; ++SlotIdx)
				{
					Mesh_Mature->SetMaterial(SlotIdx, nullptr);
				}
			}
		}
	}
	else if (SeedStr.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase) || SeedStr.Contains(TEXT("9mm"), ESearchCase::IgnoreCase))
	{
		HarvestYield.ItemID = FName("Item_Ammo_Pistol");
		HarvestYield.Quantity = 25;
		HarvestFloatingText = TEXT("+25 Pistol Ammo (9mm)");

		if (Mesh_Mature)
		{
			if (UStaticMesh* MatureAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Meshes/SM_Crop_RifleWheat.SM_Crop_RifleWheat")))
			{
				Mesh_Mature->SetStaticMesh(MatureAsset);
				const int32 NumMats = Mesh_Mature->GetNumMaterials();
				for (int32 SlotIdx = 0; SlotIdx < NumMats; ++SlotIdx)
				{
					Mesh_Mature->SetMaterial(SlotIdx, nullptr);
				}
				if (UMaterialInterface* BerryRed = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/MI_Crop_ShellRed.MI_Crop_ShellRed")))
				{
					if (NumMats > 1)
					{
						Mesh_Mature->SetMaterial(1, BerryRed);
					}
				}
				if (UMaterialInterface* CopperMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/MI_Crop_Copper.MI_Crop_Copper")))
				{
					if (NumMats > 2)
					{
						Mesh_Mature->SetMaterial(2, CopperMat);
					}
				}
			}
		}
	}
	else if (SeedStr.Contains(TEXT("Health"), ESearchCase::IgnoreCase) || SeedStr.Contains(TEXT("Medkit"), ESearchCase::IgnoreCase) || SeedStr.Contains(TEXT("Herb"), ESearchCase::IgnoreCase))
	{
		HarvestYield.ItemID = FName("Item_Medkit");
		HarvestYield.Quantity = 1;
		HarvestFloatingText = TEXT("+1 Combat Medkit");

		if (Mesh_Mature)
		{
			if (UStaticMesh* MatureAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Meshes/SM_Crop_MedHerb.SM_Crop_MedHerb")))
			{
				Mesh_Mature->SetStaticMesh(MatureAsset);
				const int32 NumMats = Mesh_Mature->GetNumMaterials();
				for (int32 SlotIdx = 0; SlotIdx < NumMats; ++SlotIdx)
				{
					Mesh_Mature->SetMaterial(SlotIdx, nullptr);
				}
			}
		}
	}
	else if (!SeedID.IsNone())
	{
		HarvestYield.ItemID = SeedID;
		HarvestYield.Quantity = 5;
		HarvestFloatingText = FString::Printf(TEXT("+5 %s"), *SeedID.ToString());
	}
}

void AFarmPlotBase::ApplyMirroredState(AFarmPlotBase* SourceFarmPlot)
{
	if (!SourceFarmPlot)
	{
		return;
	}

	LinkedFarmSourcePlot = SourceFarmPlot;
	PlantedSeedID = SourceFarmPlot->PlantedSeedID;
	CurrentStage = SourceFarmPlot->CurrentStage;
	bIsWatered = SourceFarmPlot->bIsWatered;
	bAwaitingSleepGrowth = SourceFarmPlot->bAwaitingSleepGrowth;
	HarvestYield = SourceFarmPlot->HarvestYield;
	HarvestFloatingText = SourceFarmPlot->HarvestFloatingText;
	MaxCropHealth = SourceFarmPlot->MaxCropHealth;

	if (CurrentStage != ECropStage::Empty && CurrentStage != ECropStage::Withered)
	{
		CurrentCropHealth = (SourceFarmPlot->CurrentCropHealth > 0.0f) ? SourceFarmPlot->CurrentCropHealth : MaxCropHealth;
		ConfigureVisualsForSeed(PlantedSeedID);
	}
	else
	{
		CurrentCropHealth = 0.0f;
	}

	if (SoilMesh)
	{
		if (bIsWatered && WetSoilMaterial)
		{
			SoilMesh->SetMaterial(0, WetSoilMaterial);
		}
		else if (!bIsWatered && DrySoilMaterial)
		{
			SoilMesh->SetMaterial(0, DrySoilMaterial);
		}
	}

	UpdateCropMeshForStage(CurrentStage);
	UpdateHealthBarVisuals();
}

void AFarmPlotBase::ReceiveZombieAttack(float Damage)
{
	if (!HasLivingCrop() || Damage <= 0.0f)
	{
		return;
	}

	CurrentCropHealth = FMath::Max(0.0f, CurrentCropHealth - Damage);
	if (AFarmPlotBase* SourcePlot = LinkedFarmSourcePlot.Get())
	{
		SourcePlot->CurrentCropHealth = CurrentCropHealth;
	}

	UpdateHealthBarVisuals();

	if (CurrentCropHealth <= 0.0f)
	{
		DestroyCropByZombies();
		if (AFarmPlotBase* SourcePlot = LinkedFarmSourcePlot.Get())
		{
			SourcePlot->DestroyCropByZombies();
		}
		SpawnFloatingHarvestText(TEXT("CROP DESTROYED!"), FColor(255, 55, 55));
	}
	else
	{
		SpawnFloatingHarvestText(FString::Printf(TEXT("-%.0f HP"), Damage), FColor(255, 110, 80));
	}
}

void AFarmPlotBase::DestroyCropByZombies()
{
	CurrentStage = ECropStage::Empty;
	bIsWatered = false;
	bAwaitingSleepGrowth = false;
	PlantedSeedID = NAME_None;
	CurrentCropHealth = 0.0f;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrowthTimerHandle);
	}

	if (SoilMesh && DrySoilMaterial)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
	}

	UpdateCropMeshForStage(CurrentStage);
	UpdateHealthBarVisuals();
	OnStageChanged(CurrentStage);
}

void AFarmPlotBase::UpdateHealthBarVisuals()
{
	if (!HealthBarRoot || !HealthBarBgMesh || !HealthBarFillMesh)
	{
		return;
	}

	bool bIsNight = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				bIsNight = Sub->IsNightWindow() && !Sub->bIsStartMenuActive;
			}
		}
	}

	const bool bShowBar = IsCombatWorldPlot() && HasLivingCrop() && bIsNight;
	HealthBarRoot->SetVisibility(bShowBar, true);
	HealthBarBgMesh->SetVisibility(bShowBar);
	HealthBarFillMesh->SetVisibility(bShowBar);

	if (!bShowBar)
	{
		return;
	}

	const float HealthPct = (MaxCropHealth > 0.0f) ? FMath::Clamp(CurrentCropHealth / MaxCropHealth, 0.0f, 1.0f) : 0.0f;
	const float FullBarWidth = 0.86f;
	const float FillWidth = FMath::Max(0.01f, FullBarWidth * HealthPct);
	// Anchor left edge so the bar drains cleanly from right to left
	const float LeftOffsetY = -0.5f * (FullBarWidth - FillWidth) * 100.0f;
	HealthBarFillMesh->SetRelativeScale3D(FVector(0.03f, FillWidth, 0.095f));
	HealthBarFillMesh->SetRelativeLocation(FVector(1.0f, LeftOffsetY, 0.0f));

	if (HealthBarFillMID)
	{
		FLinearColor BarColor;
		if (HealthPct > 0.55f)
		{
			BarColor = FLinearColor(0.12f, 0.92f, 0.22f, 1.0f); // Healthy Green
		}
		else if (HealthPct > 0.28f)
		{
			BarColor = FLinearColor(0.98f, 0.78f, 0.08f, 1.0f); // Warning Yellow
		}
		else
		{
			BarColor = FLinearColor(0.96f, 0.10f, 0.08f, 1.0f); // Critical Red
		}
		HealthBarFillMID->SetVectorParameterValue(TEXT("Color"), BarColor);
		HealthBarFillMID->SetVectorParameterValue(TEXT("Emissive"), BarColor * 1.4f);
	}
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
	bAwaitingSleepGrowth = false;
	CurrentCropHealth = MaxCropHealth;

	ConfigureVisualsForSeed(SeedID);

	if (SoilMesh && DrySoilMaterial && !bIsWatered)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
	}

	UpdateCropMeshForStage(CurrentStage);
	UpdateHealthBarVisuals();
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
	// The crop grows through 3 watering phases (Planted -> Sprout -> Growing).
	// The 4th phase (Mature) is gated behind a night's sleep: finishing the Growing
	// timer does NOT mature it, it marks the watered crop as awaiting sleep instead.
	if (CurrentStage == ECropStage::Growing)
	{
		bAwaitingSleepGrowth = true;
		bIsWatered = true; // keep the soil watered so "watered before sleep" grows it the next day
		if (SoilMesh && WetSoilMaterial)
		{
			SoilMesh->SetMaterial(0, WetSoilMaterial);
		}
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);
		}

		UpdateCropMeshForStage(CurrentStage);
		OnStageChanged(CurrentStage);

		const FString SleepHint = TEXT("The crop is watered and ready. Sleep in the farmhouse bed to let it grow overnight.");
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor(255, 210, 110), SleepHint);
		}
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
				{
					Sub->ShowCenterMessage(SleepHint, 4.0f, FLinearColor(1.0f, 0.8f, 0.35f, 1.0f));
				}
			}
		}
		UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: %s finished 3rd phase (Growing), watered & awaiting sleep to mature."), *GetName());
		return;
	}

	bIsWatered = false;
	bAwaitingSleepGrowth = false;
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
		const FColor StageColor = FColor::Green;
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, StageColor,
			FString::Printf(TEXT(">>> [FARMING] Crop on %s grew to: %s!"), 
				*GetName(), *UEnum::GetValueAsString(CurrentStage)));
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: %s advanced to stage: %s."),
		*GetName(), *UEnum::GetValueAsString(CurrentStage));
}

void AFarmPlotBase::MatureAfterSleep()
{
	if (!IsAwaitingSleepGrowth())
	{
		return;
	}

	bAwaitingSleepGrowth = false;
	bIsWatered = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);
	}
	if (SoilMesh && DrySoilMaterial)
	{
		SoilMesh->SetMaterial(0, DrySoilMaterial);
	}
	OnSoilDried();

	CurrentStage = ECropStage::Mature;
	UpdateCropMeshForStage(CurrentStage);
	OnStageChanged(CurrentStage);

	if (MatureSparkleVFX && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), MatureSparkleVFX, GetActorLocation() + FVector(0.0f, 0.0f, 80.0f));
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
			FString::Printf(TEXT(">>> [FARMING] Crop on %s grew overnight and is ready to harvest!"), *GetName()));
	}
	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmPlot: %s matured after sleep (watered before sleeping)."), *GetName());
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
	bAwaitingSleepGrowth = false;
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
