// Copyright GameJam Shift 2026. All Rights Reserved.

#include "Farming/BarnShopActor.h"
#include "Farming/BarnShopWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

ABarnShopActor::ABarnShopActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	CounterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CounterMesh"));
	CounterMesh->SetupAttachment(RootComponent);
	CounterMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetBoxExtent(FVector(280.0f, 220.0f, 160.0f));
	InteractionBox->SetRelativeLocation(FVector(0.0f, 120.0f, 90.0f));
	InteractionBox->SetCollisionProfileName(TEXT("Trigger"));

	SignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignText"));
	SignText->SetupAttachment(RootComponent);
	SignText->SetRelativeLocation(FVector(0.0f, 20.0f, 195.0f));
	SignText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	SignText->SetText(FText::FromString(TEXT("BARN SEED & PLOT SHOP\n[Press B or Click Counter]")));
	SignText->SetTextRenderColor(FColor(255, 220, 95));
	SignText->SetWorldSize(26.0f);
	SignText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	SignText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABarnShopActor::BeginPlay()
{
	Super::BeginPlay();

	if (CounterMesh && !CounterMesh->GetStaticMesh())
	{
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			CounterMesh->SetStaticMesh(Cube);
			CounterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
			CounterMesh->SetRelativeScale3D(FVector(2.4f, 0.9f, 0.9f));
		}
		if (UMaterialInterface* WoodMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev_Blueprint/Maps/Farm_Environment/MI_Farm_Wood.MI_Farm_Wood")))
		{
			CounterMesh->SetMaterial(0, WoodMat);
		}
	}

	if (InteractionBox)
	{
		InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ABarnShopActor::OnOverlapBegin);
		InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ABarnShopActor::OnOverlapEnd);
	}
}

void ABarnShopActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (Pawn->IsPlayerControlled())
		{
			bPlayerInRange = true;
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(9140, 4.0f, FColor::Yellow, TEXT(">>> [BARN SHOP] Press [B] or Left Click Counter to buy Seeds & Farm Plots!"));
			}
		}
	}
}

void ABarnShopActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (Pawn->IsPlayerControlled())
		{
			bPlayerInRange = false;
		}
	}
}

void ABarnShopActor::OpenBarnShop(APlayerController* PC)
{
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (!ActiveShopWidget)
	{
		ActiveShopWidget = CreateWidget<UBarnShopWidget>(PC, UBarnShopWidget::StaticClass());
	}

	if (ActiveShopWidget)
	{
		if (!ActiveShopWidget->IsInViewport())
		{
			ActiveShopWidget->AddToViewport(45);
		}
		ActiveShopWidget->SetVisibility(ESlateVisibility::Visible);
		PC->SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetWidgetToFocus(ActiveShopWidget->TakeWidget());
		PC->SetInputMode(Mode);
	}
}
