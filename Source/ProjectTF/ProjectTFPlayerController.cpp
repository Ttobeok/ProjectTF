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
	// 플레이어 카메라 매니저 클래스를 지정합니다
	PlayerCameraManagerClass = AProjectTFCameraManager::StaticClass();
}

void AProjectTFPlayerController::BeginPlay()
{
	// The game mode blueprint still carries the template HUD class, so claim the HUD here.
	// This is what puts the crosshair and the ammo readout on screen.
	// 게임 모드 블루프린트에는 아직 템플릿 HUD 클래스가 들어 있어서, 여기서 HUD를 가져옵니다.
	// 크로스헤어와 탄약 표시를 화면에 올리는 것이 바로 이 줄입니다.
	if (!GetHUD() || !GetHUD()->IsA(ACQBHUD::StaticClass()))
	{
		ClientSetHUD(ACQBHUD::StaticClass());
	}

	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	// 터치 컨트롤은 로컬 플레이어 컨트롤러에서만 스폰합니다
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		// 모바일 컨트롤 위젯을 스폰합니다
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			// 컨트롤을 플레이어 화면에 붙입니다
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
	// IMC는 로컬 플레이어 컨트롤러에만 추가합니다
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		// 입력 매핑 컨텍스트를 추가합니다
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			// 모바일 터치 입력을 쓰지 않을 때만 이 IMC들을 추가합니다
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
	// 모바일 플랫폼인가? 터치를 강제해야 하는가?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
