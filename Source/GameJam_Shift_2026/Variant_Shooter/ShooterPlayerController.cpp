// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterCharacter.h"
#include "ShooterBulletCounterUI.h"
#include "DimensionShiftSubsystem.h"
#include "InterWorld/GoldHUDWidget.h"
#include "InterWorld/StartMenuWidget.h"
#include "Farming/BarnShopWidget.h"
#include "Inventory/TarkovInventoryComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameJam_Shift_2026.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				// add the controls to the player screen
				MobileControlsWidget->AddToPlayerScreen(0);

			} else {

				UE_LOG(LogGameJam_Shift_2026, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}

		// create the bullet counter widget and add it to the screen
		BulletCounterUI = CreateWidget<UShooterBulletCounterUI>(this, BulletCounterUIClass);

		if (BulletCounterUI)
		{
			BulletCounterUI->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogGameJam_Shift_2026, Error, TEXT("Could not spawn bullet counter widget."));

		}

		// Create the persistent Gold economy HUD widget
		GoldHUDWidget = CreateWidget<UGoldHUDWidget>(this, UGoldHUDWidget::StaticClass());
		if (GoldHUDWidget)
		{
			GoldHUDWidget->AddToViewport(20);
			GoldHUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		// Show the horizontally split Dual-Dimension Start Menu at the start of the session
		bool bShouldShowStartMenu = true;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
			{
				if (Sub->bHasDismissedStartMenu)
				{
					bShouldShowStartMenu = false;
					Sub->bIsStartMenuActive = false;
				}
				else
				{
					Sub->bIsStartMenuActive = true;
				}
			}
		}

		if (bShouldShowStartMenu)
		{
			StartMenuWidget = CreateWidget<UStartMenuWidget>(this, UStartMenuWidget::StaticClass());
			if (StartMenuWidget)
			{
				StartMenuWidget->AddToViewport(100);
			}
		}
		
	}
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		// Escape/P opens the paused settings overlay. P remains as an alias.
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AShooterPlayerController::TogglePauseMenu);
			InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AShooterPlayerController::TogglePauseMenu);
		}
	}
}

void AShooterPlayerController::ToggleSettingsMenu()
{
	TogglePauseMenu();
}

void AShooterPlayerController::TogglePauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (StartMenuWidget && StartMenuWidget->IsInViewport() && StartMenuWidget->IsPauseMenuMode())
	{
		StartMenuWidget->OnStartGameClicked();
		return;
	}

	// Never pause over the real main menu.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			if (Sub->bIsStartMenuActive)
			{
				return;
			}
		}
	}

	if (AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterChar->DoStopFiring();
	}

	CloseTransientOverlays();

	StartMenuWidget = CreateWidget<UStartMenuWidget>(this, UStartMenuWidget::StaticClass());
	if (StartMenuWidget)
	{
		StartMenuWidget->OpenPauseMenu();
		StartMenuWidget->AddToViewport(100);
		UGameplayStatics::SetGamePaused(this, true);
	}
}

void AShooterPlayerController::QuitToMainMenu()
{
	CloseTransientOverlays();
	UGameplayStatics::SetGamePaused(this, false);

	if (StartMenuWidget && StartMenuWidget->IsInViewport())
	{
		StartMenuWidget->RemoveFromParent();
	}
	StartMenuWidget = nullptr;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDimensionShiftSubsystem* Sub = GI->GetSubsystem<UDimensionShiftSubsystem>())
		{
			Sub->ResetSession();
		}
	}

	// Reload the run so inventory, crops, enemies, landmines and player state are fresh.
	// PlayerController::BeginPlay will recreate the real main menu from the reset flags.
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Dev_Blueprint/Maps/Lvl_Combat")), true, TEXT(""));
}

void AShooterPlayerController::CloseTransientOverlays()
{
	if (AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(GetPawn()))
	{
		if (ShooterChar->IsWeaponWheelOpen())
		{
			ShooterChar->OnTabReleased();
		}
		if (UTarkovInventoryComponent* Inv = ShooterChar->FindComponentByClass<UTarkovInventoryComponent>())
		{
			if (Inv->IsInventoryOpen())
			{
				Inv->ToggleInventory();
			}
		}
	}

	TArray<UUserWidget*> ShopWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, ShopWidgets, UBarnShopWidget::StaticClass());
	for (UUserWidget* ShopWidget : ShopWidgets)
	{
		if (ShopWidget)
		{
			ShopWidget->RemoveFromParent();
		}
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	InPawn->OnDestroyed.AddDynamic(this, &AShooterPlayerController::OnPawnDestroyed);

	// is this a shooter character?
	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn))
	{
		// add the player tag
		ShooterCharacter->Tags.Add(PlayerPawnTag);

		// subscribe to the pawn's delegates
		ShooterCharacter->OnBulletCountUpdated.AddDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
		ShooterCharacter->OnDamaged.AddDynamic(this, &AShooterPlayerController::OnPawnDamaged);

		// force update the life bar
		ShooterCharacter->OnDamaged.Broadcast(1.0f);
	}
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// reset the bullet counter HUD
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_UpdateBulletCounter(0, 0);
	}

	// find the player start
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		// select a random player start
		AActor* RandomPlayerStart = ActorList[FMath::RandRange(0, ActorList.Num() - 1)];

		// spawn a character at the player start
		const FTransform SpawnTransform = RandomPlayerStart->GetActorTransform();

		if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(CharacterClass, SpawnTransform))
		{
			// possess the character
			Possess(RespawnedCharacter);
		}
	}
}

void AShooterPlayerController::OnBulletCountUpdated(int32 MagazineSize, int32 Bullets)
{
	// update the UI
	if (BulletCounterUI)
	{
		BulletCounterUI->BP_UpdateBulletCounter(MagazineSize, Bullets);
	}
}

void AShooterPlayerController::OnPawnDamaged(float LifePercent)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_Damaged(LifePercent);
		BulletCounterUI->TriggerBloodDamageEffect(LifePercent);
	}
}

bool AShooterPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
