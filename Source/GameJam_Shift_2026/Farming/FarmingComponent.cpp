// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmingComponent.h"
#include "FarmPlotBase.h"
#include "FarmingHUDWidgets.h"
#include "InterWorldBridge.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "GameJam_Shift_2026.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "CollisionQueryParams.h"

UFarmingComponent::UFarmingComponent()
	: ActiveTool(EFarmTool::None)
	, SelectedSeedID(NAME_None)
	, SelectedSeedIndex(0)
	, InteractionDistance(400.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFarmingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AvailableSeeds.Num() > 0 && SelectedSeedID.IsNone())
	{
		SelectedSeedIndex = 0;
		SelectedSeedID = AvailableSeeds[0];
	}
}

void UFarmingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsPlacingPlot)
	{
		UpdatePlotPreviewTransform();
	}
}

AFarmPlotBase* UFarmingComponent::GetTargetedPlot() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FVector TraceStart = OwnerActor->GetActorLocation();
	FVector ForwardVector = OwnerActor->GetActorForwardVector();

	if (const APawn* Pawn = Cast<APawn>(OwnerActor))
	{
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			FRotator ViewRot;
			PC->GetPlayerViewPoint(TraceStart, ViewRot);
			ForwardVector = ViewRot.Vector();
		}
	}

	const FVector TraceEnd = TraceStart + (ForwardVector * InteractionDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FarmingToolTrace), true, OwnerActor);
	QueryParams.bReturnPhysicalMaterial = false;

	FHitResult HitResult;

	// Trace on Visibility channel first
	bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	// Fallback to WorldDynamic channel if needed
	if (!bHit || !HitResult.GetActor())
	{
		bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldDynamic, QueryParams);
	}

	if (bHit && HitResult.GetActor())
	{
		if (AFarmPlotBase* Plot = Cast<AFarmPlotBase>(HitResult.GetActor()))
		{
			return Plot;
		}

		if (AFarmPlotBase* ParentPlot = Cast<AFarmPlotBase>(HitResult.GetActor()->GetAttachParentActor()))
		{
			return ParentPlot;
		}
	}

	return nullptr;
}

void UFarmingComponent::UseActiveTool()
{
	AFarmPlotBase* TargetPlot = GetTargetedPlot();

	if (!TargetPlot)
	{
		FString ToolLabel;
		switch (ActiveTool)
		{
		case EFarmTool::None:        ToolLabel = TEXT("Hands / Seed Bag"); break;
		case EFarmTool::Hoe:         ToolLabel = TEXT("Hoe"); break;
		case EFarmTool::WateringCan: ToolLabel = TEXT("Watering Can"); break;
		case EFarmTool::Harvester:   ToolLabel = TEXT("Harvester"); break;
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				9120, 2.0f, FColor::Yellow,
				FString::Printf(TEXT("[FARM] Active Tool: %s (Aim at a farm plot within %.0f cm)"),
					*ToolLabel, InteractionDistance)
			);
		}
		return;
	}

	ExecuteToolOnPlot(TargetPlot);
}

void UFarmingComponent::ExecuteToolOnPlot(AFarmPlotBase* Plot)
{
	if (!Plot)
	{
		return;
	}

	switch (ActiveTool)
	{
	case EFarmTool::None:
	{
		if (Plot->CanPlant())
		{
			if (SelectedSeedID.IsNone() || GetSeedCount(SelectedSeedID) <= 0)
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(
						9124, 3.0f, FColor::Orange,
						TEXT("[FARM] No seeds in pouch! Visit the Barn Shop to purchase seeds.")
					);
				}
				return;
			}

			const FName SeedToPlant = SelectedSeedID;
			if (Plot->PlantSeed(SeedToPlant))
			{
				if (int32* CountPtr = SeedCounts.Find(SeedToPlant))
				{
					*CountPtr = FMath::Max(0, *CountPtr - 1);
					if (*CountPtr == 0)
					{
						SeedCounts.Remove(SeedToPlant);
						AvailableSeeds.Remove(SeedToPlant);
						if (AvailableSeeds.Num() > 0)
						{
							SelectedSeedIndex = SelectedSeedIndex % AvailableSeeds.Num();
							SelectedSeedID = AvailableSeeds[SelectedSeedIndex];
						}
						else
						{
							SelectedSeedIndex = 0;
							SelectedSeedID = NAME_None;
						}
					}
				}
				if (ActiveFarmingHUD)
				{
					ActiveFarmingHUD->RefreshHUD();
				}
				OnSeedPlanted.Broadcast(Plot, SeedToPlant);
			}
		}
		else if (Plot->CanHarvest())
		{
			FName HarvestedItem;
			int32 HarvestedQty = 0;
			if (Plot->Harvest(HarvestedItem, HarvestedQty))
			{
				OnCropHarvested.Broadcast(HarvestedItem, HarvestedQty);
				UInterWorldBridge::DepositHarvestToInventory(GetOwner(), HarvestedItem, HarvestedQty);
			}
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1, 2.5f, FColor::Silver,
					FString::Printf(TEXT("[FARM] Hands: Plot has '%s' (%s). Use Watering Can or wait for growth."),
						*Plot->PlantedSeedID.ToString(), *UEnum::GetValueAsString(Plot->GetCurrentStage()))
				);
			}
		}
		break;
	}

	case EFarmTool::Hoe:
	{
		if (Plot->TillPlot())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, TEXT(">>> [FARM] Tilled and prepared plot with Hoe!"));
			}
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Silver, TEXT("[FARM] Plot is already tilled and ready for planting."));
			}
		}
		break;
	}

	case EFarmTool::WateringCan:
	{
		if (Plot->CanWater())
		{
			if (Plot->WaterPlot())
			{
				OnPlotWatered.Broadcast(Plot);
			}
		}
		else if (Plot->IsWatered())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Silver, TEXT("[FARM] Soil is already wet and growing!"));
			}
		}
		else if (Plot->CanPlant())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[FARM] Plot is empty! Plant a seed before watering."));
			}
		}
		else if (Plot->CanHarvest())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, TEXT("[FARM] Crop is already mature! Use Harvester to collect."));
			}
		}
		break;
	}

	case EFarmTool::Harvester:
	{
		if (Plot->CanHarvest())
		{
			FName HarvestedItem;
			int32 HarvestedQty = 0;
			if (Plot->Harvest(HarvestedItem, HarvestedQty))
			{
				OnCropHarvested.Broadcast(HarvestedItem, HarvestedQty);
				UInterWorldBridge::DepositHarvestToInventory(GetOwner(), HarvestedItem, HarvestedQty);
			}
		}
		else if (Plot->CanPlant())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[FARM] Nothing to harvest - plot is empty."));
			}
		}
		else
		{
			const float Progress = Plot->GetStageProgress() * 100.0f;
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1, 2.5f, FColor::Yellow,
					FString::Printf(TEXT("[FARM] Crop not mature yet! Current stage: %s (%.0f%%)"),
						*UEnum::GetValueAsString(Plot->GetCurrentStage()), Progress)
				);
			}
		}
		break;
	}
	}
}

void UFarmingComponent::CycleTool(bool bNext)
{
	uint8 CurrentIndex = static_cast<uint8>(ActiveTool);
	constexpr uint8 MaxTools = 4; // None, Hoe, WateringCan, Harvester

	if (bNext)
	{
		CurrentIndex = (CurrentIndex + 1) % MaxTools;
	}
	else
	{
		CurrentIndex = (CurrentIndex == 0) ? (MaxTools - 1) : (CurrentIndex - 1);
	}

	SetActiveTool(static_cast<EFarmTool>(CurrentIndex));
}

void UFarmingComponent::SetActiveTool(EFarmTool NewTool)
{
	ActiveTool = NewTool;
	OnToolChanged.Broadcast(ActiveTool);

	FString ToolLabel;
	switch (ActiveTool)
	{
	case EFarmTool::None:        ToolLabel = TEXT("Hands / Seed Bag"); break;
	case EFarmTool::Hoe:         ToolLabel = TEXT("Hoe (Till Soil)"); break;
	case EFarmTool::WateringCan: ToolLabel = TEXT("Watering Can"); break;
	case EFarmTool::Harvester:   ToolLabel = TEXT("Harvester / Sickle"); break;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9121, 3.0f, FColor::Cyan,
			FString::Printf(TEXT(">>> [FARM TOOL] Equipped: %s (1-4 or Q/E to cycle)"), *ToolLabel)
		);
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmingComponent: Equipped tool %s"), *ToolLabel);
}

void UFarmingComponent::CycleSeed(bool bNext)
{
	if (AvailableSeeds.Num() == 0)
	{
		SelectedSeedID = NAME_None;
		SelectedSeedIndex = 0;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				9123, 2.5f, FColor::Orange,
				TEXT("[FARM SEED] No seeds purchased! Visit the Barn Shop to buy seeds.")
			);
		}
		return;
	}

	if (bNext)
	{
		SelectedSeedIndex = (SelectedSeedIndex + 1) % AvailableSeeds.Num();
	}
	else
	{
		SelectedSeedIndex = (SelectedSeedIndex == 0) ? (AvailableSeeds.Num() - 1) : (SelectedSeedIndex - 1);
	}

	SelectedSeedID = AvailableSeeds[SelectedSeedIndex];

	if (ActiveFarmingHUD)
	{
		ActiveFarmingHUD->RefreshHUD();
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9123, 3.0f, FColor::Green,
			FString::Printf(TEXT(">>> [FARM SEED] Selected: %s (x%d)"), *SelectedSeedID.ToString(), GetSeedCount(SelectedSeedID))
		);
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmingComponent: Selected seed %s"), *SelectedSeedID.ToString());
}

void UFarmingComponent::AddPurchasedSeed(FName SeedID, int32 Quantity)
{
	if (SeedID.IsNone() || Quantity <= 0)
	{
		return;
	}

	int32& Count = SeedCounts.FindOrAdd(SeedID);
	Count += Quantity;

	if (!AvailableSeeds.Contains(SeedID))
	{
		AvailableSeeds.Add(SeedID);
	}

	if (SelectedSeedID.IsNone())
	{
		SelectedSeedIndex = AvailableSeeds.Find(SeedID);
		SelectedSeedID = SeedID;
	}

	if (ActiveFarmingHUD)
	{
		ActiveFarmingHUD->RefreshHUD();
	}
}

int32 UFarmingComponent::GetSeedCount(FName SeedID) const
{
	if (const int32* Found = SeedCounts.Find(SeedID))
	{
		return *Found;
	}
	return 0;
}

void UFarmingComponent::StartPlotPlacement(FGuid PlotItemId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CancelPlotPlacement();

	bIsPlacingPlot = true;
	bPlacementValid = false;
	PendingPlotItemId = PlotItemId;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PlotPreviewActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (PlotPreviewActor)
	{
		UStaticMeshComponent* PreviewMesh = NewObject<UStaticMeshComponent>(PlotPreviewActor, TEXT("PlotPreviewMesh"));
		PreviewMesh->RegisterComponent();
		PlotPreviewActor->SetRootComponent(PreviewMesh);
		PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewMesh->SetCastShadow(false);

		UStaticMesh* MoundMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Meshes/SM_Crop_DirtMound.SM_Crop_DirtMound"));
		if (MoundMesh)
		{
			PreviewMesh->SetStaticMesh(MoundMesh);
			PreviewMesh->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
		}
		else if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			PreviewMesh->SetStaticMesh(CubeMesh);
			PreviewMesh->SetWorldScale3D(FVector(1.2f, 1.2f, 0.16f));
		}

		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
		if (BaseMat)
		{
			PlotPreviewMID = UMaterialInstanceDynamic::Create(BaseMat, PlotPreviewActor);
			if (PlotPreviewMID)
			{
				PlotPreviewMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.95f, 0.30f, 1.0f));
				PlotPreviewMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.08f, 0.85f, 0.25f, 1.0f));
				PreviewMesh->SetMaterial(0, PlotPreviewMID);
			}
		}
	}

	UpdatePlotPreviewTransform();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9125, 4.0f, FColor::Green, TEXT(">>> [PLOT PLACEMENT] Left Click to confirm plot placement | Right Click to cancel"));
	}
}

void UFarmingComponent::UpdatePlotPreviewTransform()
{
	if (!PlotPreviewActor || !GetOwner() || !GetWorld())
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;

	FVector ViewLoc = GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	FRotator ViewRot = GetOwner()->GetActorRotation();
	if (PC)
	{
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	}

	const FVector Forward2D = FVector(ViewRot.Vector().X, ViewRot.Vector().Y, 0.0f).GetSafeNormal();
	const FVector DesiredXY = GetOwner()->GetActorLocation() + Forward2D * 260.0f;

	FHitResult GroundHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlotPreviewGround), false);
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(PlotPreviewActor);

	const bool bHitGround = GetWorld()->LineTraceSingleByChannel(
		GroundHit,
		FVector(DesiredXY.X, DesiredXY.Y, ViewLoc.Z + 300.0f),
		FVector(DesiredXY.X, DesiredXY.Y, ViewLoc.Z - 600.0f),
		ECC_WorldStatic,
		Params
	);

	bPlacementValid = bHitGround && (GetOwner()->GetActorLocation().Z < -10000.0f);
	const FVector FinalLoc = bHitGround
		? GroundHit.ImpactPoint
		: FVector(DesiredXY.X, DesiredXY.Y, GetOwner()->GetActorLocation().Z - 88.0f);

	PlotPreviewActor->SetActorLocationAndRotation(FinalLoc, FRotator(0.0f, ViewRot.Yaw, 0.0f));

	if (PlotPreviewMID)
	{
		const FLinearColor Tint = bPlacementValid ? FLinearColor(0.10f, 0.95f, 0.30f, 1.0f) : FLinearColor(0.95f, 0.15f, 0.15f, 1.0f);
		const FLinearColor Glow = bPlacementValid ? FLinearColor(0.08f, 0.85f, 0.25f, 1.0f) : FLinearColor(0.85f, 0.10f, 0.10f, 1.0f);
		PlotPreviewMID->SetVectorParameterValue(TEXT("Color"), Tint);
		PlotPreviewMID->SetVectorParameterValue(TEXT("Emissive"), Glow);
	}
}

bool UFarmingComponent::ConfirmPlotPlacement()
{
	if (!bIsPlacingPlot || !bPlacementValid || !PlotPreviewActor || !GetWorld())
	{
		return false;
	}

	const FVector SpawnLoc = PlotPreviewActor->GetActorLocation();
	const FRotator SpawnRot = PlotPreviewActor->GetActorRotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFarmPlotBase* NewPlot = GetWorld()->SpawnActor<AFarmPlotBase>(AFarmPlotBase::StaticClass(), SpawnLoc, SpawnRot, Params);
	if (NewPlot)
	{
		if (UStaticMesh* MoundMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Meshes/SM_Crop_DirtMound.SM_Crop_DirtMound")))
		{
			if (NewPlot->SoilMesh)
			{
				NewPlot->SoilMesh->SetStaticMesh(MoundMesh);
				NewPlot->SoilMesh->SetRelativeLocation(FVector::ZeroVector);
				NewPlot->SoilMesh->SetRelativeScale3D(FVector::OneVector);
			}
		}
		if (UMaterialInterface* DryMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/MI_Crop_DirtDry.MI_Crop_DirtDry")))
		{
			NewPlot->DrySoilMaterial = DryMat;
			if (NewPlot->SoilMesh)
			{
				NewPlot->SoilMesh->SetMaterial(0, DryMat);
			}
		}
		if (UMaterialInterface* WetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Crops/Materials/MI_Crop_DirtWet.MI_Crop_DirtWet")))
		{
			NewPlot->WetSoilMaterial = WetMat;
		}
	}

	// Remove the consumed "Farm Plot" item from Tarkov Backpack
	if (AActor* OwnerActor = GetOwner())
	{
		if (UTarkovInventoryComponent* Inv = OwnerActor->FindComponentByClass<UTarkovInventoryComponent>())
		{
			Inv->RemoveItem(PendingPlotItemId);
		}
	}

	CancelPlotPlacement();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9125, 3.0f, FColor::Green, TEXT(">>> [FARM] New Farm Plot placed!"));
	}
	return true;
}

void UFarmingComponent::CancelPlotPlacement()
{
	bIsPlacingPlot = false;
	bPlacementValid = false;
	PendingPlotItemId.Invalidate();

	if (PlotPreviewActor)
	{
		PlotPreviewActor->Destroy();
		PlotPreviewActor = nullptr;
	}
	PlotPreviewMID = nullptr;
}

void UFarmingComponent::SetSelectedSeed(FName NewSeedID)
{
	SelectedSeedID = NewSeedID;
	const int32 FoundIndex = AvailableSeeds.Find(NewSeedID);
	if (FoundIndex != INDEX_NONE)
	{
		SelectedSeedIndex = FoundIndex;
	}

	if (ActiveFarmingHUD)
	{
		ActiveFarmingHUD->RefreshHUD();
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9123, 3.0f, FColor::Green,
			FString::Printf(TEXT(">>> [FARM SEED] Selected: %s"), *SelectedSeedID.ToString())
		);
	}
}

void UFarmingComponent::SetFarmingHUDVisible(bool bVisible)
{
	if (bVisible)
	{
		if (!ActiveFarmingHUD)
		{
			APawn* OwnerPawn = Cast<APawn>(GetOwner());
			APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
			if (PC && PC->IsLocalController())
			{
				TSubclassOf<UFarmingHUDWidget> ClassToSpawn = FarmingHUDClass;
				if (!ClassToSpawn)
				{
					ClassToSpawn = LoadClass<UFarmingHUDWidget>(nullptr, TEXT("/Game/Dev_Blueprint/UI/WBP_FarmingHUD.WBP_FarmingHUD_C"));
				}
				if (!ClassToSpawn)
				{
					ClassToSpawn = UFarmingHUDWidget::StaticClass();
				}

				ActiveFarmingHUD = CreateWidget<UFarmingHUDWidget>(PC, ClassToSpawn);
				if (ActiveFarmingHUD)
				{
					ActiveFarmingHUD->AddToViewport(5);
				}
			}
		}

		if (ActiveFarmingHUD)
		{
			ActiveFarmingHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ActiveFarmingHUD->RefreshHUD();
		}
	}
	else
	{
		if (ActiveFarmingHUD)
		{
			ActiveFarmingHUD->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
