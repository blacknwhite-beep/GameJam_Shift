// Copyright GameJam Shift 2026. All Rights Reserved.

#include "FarmingComponent.h"
#include "GameJam_Shift_2026.h"

UFarmingComponent::UFarmingComponent()
	: ActiveTool(EFarmTool::None)
	, SelectedSeedID(FName("Crop_Wheat"))
	, InteractionDistance(300.0f)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFarmingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UFarmingComponent::UseActiveTool()
{
	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmingComponent: Using active tool %s"), *UEnum::GetValueAsString(ActiveTool));
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
	UE_LOG(LogGameJam_Shift_2026, Log, TEXT("FarmingComponent: Equipped tool %s"), *UEnum::GetValueAsString(ActiveTool));
}
