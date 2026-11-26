// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Traps/DeployablePlacementComponent.h"
#include "Dimension/DimensionShiftSubsystem.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UDeployablePlacementComponent::UDeployablePlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetComponentTickEnabled(false);

	// Constructor-time hard references are required by FObjectFinder and ensure
	// the preview assets are included in cooked builds.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		PreviewCylinderMesh = CylinderFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewMaterialFinder(TEXT("/Game/Dev_Blueprint/Crops/Materials/M_CropBase.M_CropBase"));
	if (PreviewMaterialFinder.Succeeded())
	{
		PreviewMaterial = PreviewMaterialFinder.Object;
	}
}

void UDeployablePlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsPlacing)
	{
		UpdatePreviewTransform();
	}
}

void UDeployablePlacementComponent::StartPlacement(FGuid ItemId, TSubclassOf<AActor> PlaceableClass)
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor || !PlaceableClass)
	{
		return;
	}

	CancelPlacement();
	bIsPlacing = true;
	SetComponentTickEnabled(true);
	bPlacementValid = false;
	PendingItemId = ItemId;
	PendingPlaceableClass = PlaceableClass;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!PreviewActor)
	{
		CancelPlacement();
		return;
	}

	PreviewMesh = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("DeployablePreviewMesh"));
	PreviewMesh->RegisterComponent();
	PreviewActor->SetRootComponent(PreviewMesh);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->SetCanEverAffectNavigation(false);

	if (PreviewCylinderMesh)
	{
		PreviewMesh->SetStaticMesh(PreviewCylinderMesh);
	}
	PreviewMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.16f));
	PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));

	if (PreviewMaterial)
	{
		PreviewMID = UMaterialInstanceDynamic::Create(PreviewMaterial, PreviewActor);
		if (PreviewMID)
		{
			PreviewMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.95f, 0.30f, 1.0f));
			PreviewMID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.08f, 0.85f, 0.25f, 1.0f));
			PreviewMesh->SetMaterial(0, PreviewMID);
		}
	}

	UpdatePreviewTransform();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9180, 4.0f, FColor::Green,
			TEXT(">>> [TRAP PLACEMENT] Aim at the ground | Left Click to place | Right Click to cancel"));
	}
}

void UDeployablePlacementComponent::UpdatePreviewTransform()
{
	if (!bIsPlacing || !PreviewActor || !GetOwner() || !GetWorld())
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

	const FVector TraceEnd = ViewLoc + ViewRot.Vector() * MaxPlacementDistance;
	FHitResult GroundHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeployablePreviewGround), false);
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(PreviewActor);

	bool bHitGround = GetWorld()->LineTraceSingleByChannel(GroundHit, ViewLoc, TraceEnd, ECC_WorldStatic, Params);

	// If the crosshair is on the sky/horizon, drop a vertical trace from a point
	// ahead of the player so placement still feels predictable.
	if (!bHitGround)
	{
		const FVector Forward2D = FVector(ViewRot.Vector().X, ViewRot.Vector().Y, 0.0f).GetSafeNormal();
		const FVector DesiredXY = GetOwner()->GetActorLocation() + Forward2D * FMath::Min(300.0f, MaxPlacementDistance);
		bHitGround = GetWorld()->LineTraceSingleByChannel(
			GroundHit,
			FVector(DesiredXY.X, DesiredXY.Y, ViewLoc.Z + 300.0f),
			FVector(DesiredXY.X, DesiredXY.Y, ViewLoc.Z - 700.0f),
			ECC_WorldStatic,
			Params
		);
	}

	const bool bInFarm = GetOwner()->GetActorLocation().Z < -10000.0f;
	const bool bInCombat = GetOwner()->GetActorLocation().Z > -10000.0f;
	const bool bFlatGround = bHitGround && GroundHit.ImpactNormal.Z > 0.70f;
	bPlacementValid = (bInFarm || bInCombat) && bFlatGround;

	const FVector FinalLoc = bHitGround
		? GroundHit.ImpactPoint + GroundHit.ImpactNormal * 1.0f
		: FVector::ZeroVector;
	PreviewActor->SetActorLocationAndRotation(FinalLoc, FRotator(0.0f, ViewRot.Yaw, 0.0f));
	PreviewActor->SetActorHiddenInGame(!bHitGround);

	if (PreviewMID)
	{
		const FLinearColor Tint = bPlacementValid
			? FLinearColor(0.10f, 0.95f, 0.30f, 1.0f)
			: FLinearColor(0.95f, 0.15f, 0.15f, 1.0f);
		const FLinearColor Glow = bPlacementValid
			? FLinearColor(0.08f, 0.85f, 0.25f, 1.0f)
			: FLinearColor(0.85f, 0.10f, 0.10f, 1.0f);
		PreviewMID->SetVectorParameterValue(TEXT("Color"), Tint);
		PreviewMID->SetVectorParameterValue(TEXT("Emissive"), Glow);
	}
}

bool UDeployablePlacementComponent::ConfirmPlacement()
{
	if (!bIsPlacing || !bPlacementValid || !PreviewActor || !PendingPlaceableClass || !GetWorld())
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* PlacedActor = GetWorld()->SpawnActor<AActor>(
		PendingPlaceableClass,
		PreviewActor->GetActorLocation(),
		PreviewActor->GetActorRotation(),
		SpawnParams
	);

	if (!PlacedActor)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(9180, 3.0f, FColor::Red, TEXT("[TRAP] Placement failed. Your item was not consumed."));
		}
		return false;
	}

	AActor* OwnerActor = GetOwner();
	UTarkovInventoryComponent* Inv = OwnerActor ? OwnerActor->FindComponentByClass<UTarkovInventoryComponent>() : nullptr;
	if (!Inv || !Inv->RemoveItem(PendingItemId))
	{
		PlacedActor->Destroy();
		CancelPlacement();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(9180, 3.0f, FColor::Red,
				TEXT("[TRAP] The inventory item is no longer available. Placement was cancelled without consuming it."));
		}
		return false;
	}

	CancelPlacement();

	// Farm-world mines own a linked Combat-world mirror at FarmWorldOffset.
	if (PlacedActor->GetActorLocation().Z < -10000.0f)
	{
		if (UGameInstance* GI = GetWorld()->GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				Sub->SyncLandminesToCombat(GetWorld());
			}
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9180, 3.0f, FColor::Green, TEXT(">>> [TRAP] Landmine placed and arming!"));
	}
	return true;
}

void UDeployablePlacementComponent::CancelPlacement()
{
	SetComponentTickEnabled(false);
	bIsPlacing = false;
	bPlacementValid = false;
	PendingItemId.Invalidate();
	PendingPlaceableClass = nullptr;

	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	PreviewMesh = nullptr;
	PreviewMID = nullptr;
}
