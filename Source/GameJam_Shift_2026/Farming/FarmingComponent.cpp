// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmingComponent.h"
#include "FarmPlotBase.h"
#include "GameJam_Shift_2026.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"

UFarmingComponent::UFarmingComponent()
	: ActiveTool(EFarmTool::None)
	, SelectedSeedID(FName("Seed_ShotgunAmmo"))
	, SelectedSeedIndex(0)
	, InteractionDistance(400.0f)
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default prototype seed catalog
	AvailableSeeds.Add(FName("Seed_ShotgunAmmo"));
	AvailableSeeds.Add(FName("Seed_RifleAmmo"));
	AvailableSeeds.Add(FName("Seed_PistolAmmo"));
	AvailableSeeds.Add(FName("Seed_Medkit"));
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
			if (Plot->PlantSeed(SelectedSeedID))
			{
				OnSeedPlanted.Broadcast(Plot, SelectedSeedID);
			}
		}
		else if (Plot->CanHarvest())
		{
			FName HarvestedItem;
			int32 HarvestedQty = 0;
			if (Plot->Harvest(HarvestedItem, HarvestedQty))
			{
				OnCropHarvested.Broadcast(HarvestedItem, HarvestedQty);
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

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9123, 3.0f, FColor::Green,
			FString::Printf(TEXT(">>> [FARM SEED] Selected: %s (Press T to cycle)"), *SelectedSeedID.ToString())
		);
	}

	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmingComponent: Selected seed %s"), *SelectedSeedID.ToString());
}

void UFarmingComponent::SetSelectedSeed(FName NewSeedID)
{
	SelectedSeedID = NewSeedID;
	const int32 FoundIndex = AvailableSeeds.Find(NewSeedID);
	if (FoundIndex != INDEX_NONE)
	{
		SelectedSeedIndex = FoundIndex;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9123, 3.0f, FColor::Green,
			FString::Printf(TEXT(">>> [FARM SEED] Selected: %s"), *SelectedSeedID.ToString())
		);
	}
}
