// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "ProjectTFPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 *
 *  단순한 1인칭 플레이어 컨트롤러입니다.
 *  입력 매핑 컨텍스트를 관리하고, Player Camera Manager 클래스를 교체합니다.
 */
UCLASS(abstract, config="Game")
class PROJECTTF_API AProjectTFPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()
	
public:

	/**
	 *  Constructor
	 *  생성자
	 */
	AProjectTFPlayerController();

	/**
	 *  The player shares a team with the squad, so the AI sight sense treats both as one side
	 *  플레이어는 분대와 같은 팀입니다. 그래야 AI 시야 감각이 둘을 한편으로 취급합니다
	 */
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(1); }

protected:

	/**
	 *  Input Mapping Contexts
	 *  입력 매핑 컨텍스트
	 */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/**
	 *  Input Mapping Contexts
	 *  입력 매핑 컨텍스트
	 */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/**
	 *  Mobile controls widget to spawn
	 *  스폰할 모바일 컨트롤 위젯
	 */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/**
	 *  Pointer to the mobile controls widget
	 *  모바일 컨트롤 위젯 포인터
	 */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
