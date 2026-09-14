// Copyright Epic Games, Inc. All Rights Reserved.


#include "ProjectTFPlayerController.h"
#include "CQB/CQBHUD.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "ProjectTFCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "ProjectTF.h"
#include "Widgets/Input/SVirtualJoystick.h"

AProjectTFPlayerController::AProjectTFPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AProjectTFCameraManager::StaticClass();
}

void AProjectTFPlayerController::BeginPlay()
{
	// The game mode blueprint still carries the template HUD class, so claim the HUD here.
	// This is what puts the crosshair and the ammo readout on screen.
	if (!GetHUD() || !GetHUD()->IsA(ACQBHUD::StaticClass()))
	{
		ClientSetHUD(ACQBHUD::StaticClass());
	}

	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogProjectTF, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AProjectTFPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
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
	}
	
}

bool AProjectTFPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
