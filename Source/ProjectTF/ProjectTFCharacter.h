// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CQB/CQBTypes.h"
#include "Perception/AISightTargetInterface.h"
#include "ProjectTFCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UHealthComponent;
class UWeaponComponent;
class UWeaponVisualComponent;
class ADoorwayMarker;
class AAllyAIController;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character.
 *  Carries the CQB health and hitscan weapon components and handles fire / ADS / reload / lean input.
 *
 *  기본적인 1인칭 캐릭터입니다.
 *  CQB 체력·히트스캔 무기 컴포넌트를 들고, 사격 / 정조준 / 재장전 / 린 입력을 처리합니다.
 */
UCLASS(abstract)
class AProjectTFCharacter : public ACharacter, public ICQBFactionAgent, public IAISightTargetInterface
{
	GENERATED_BODY()

	/**
	 *  Pawn mesh: first person view (arms; seen only by self)
	 *  폰 메시: 1인칭 시점(팔. 본인에게만 보입니다)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/**
	 *  First person camera
	 *  1인칭 카메라
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/**
	 *  Health pool shared with the AI enemies
	 *  AI와 공유하는 체력
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UHealthComponent* HealthComponent;

	/**
	 *  Hitscan weapon shared with the AI enemies
	 *  AI와 공유하는 히트스캔 무기
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWeaponComponent* WeaponComponent;

	/**
	 *  The weapon you can see, as a first person view model
	 *  눈에 보이는 무기. 1인칭 뷰모델입니다
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWeaponVisualComponent* WeaponVisual;

protected:

	/**
	 *  Jump Input Action
	 *  점프 입력 액션
	 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/**
	 *  Move Input Action
	 *  이동 입력 액션
	 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/**
	 *  Look Input Action
	 *  시점 입력 액션
	 */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/**
	 *  Mouse Look Input Action
	 *  마우스 시점 입력 액션
	 */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/**
	 *  Fire Input Action. Optional: left mouse is bound directly when this is empty.
	 *  사격 입력 액션. 선택 사항입니다 — 비워두면 좌클릭이 직접 바인딩됩니다.
	 */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBFireAction;

	/**
	 *  Aim down sights Input Action. Optional: right mouse is bound directly when this is empty.
	 *  정조준 입력 액션. 선택 사항입니다 — 비워두면 우클릭이 직접 바인딩됩니다.
	 */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBADSAction;

	/**
	 *  Reload Input Action. Optional: R is bound directly when this is empty.
	 *  재장전 입력 액션. 선택 사항입니다 — 비워두면 R이 직접 바인딩됩니다.
	 */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBReloadAction;

	/**
	 *  Lean left Input Action. Optional: Q is bound directly when this is empty.
	 *  좌측 린 입력 액션. 선택 사항입니다 — 비워두면 Q가 직접 바인딩됩니다.
	 */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBLeanLeftAction;

	/**
	 *  Lean right Input Action. Optional: E is bound directly when this is empty.
	 *  우측 린 입력 액션. 선택 사항입니다 — 비워두면 E가 직접 바인딩됩니다.
	 */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBLeanRightAction;

	/**
	 *  Camera roll applied at full lean, in degrees
	 *  최대 린에서 적용되는 카메라 롤(도)
	 */
	UPROPERTY(EditAnywhere, Category ="Lean")
	float LeanRollAngle = 14.0f;

	/**
	 *  Sideways camera offset applied at full lean, in cm
	 *  최대 린에서 적용되는 카메라 측면 오프셋(cm)
	 */
	UPROPERTY(EditAnywhere, Category ="Lean")
	float LeanOffsetDistance = 45.0f;

	/**
	 *  How fast the lean blends in and out
	 *  린이 들어가고 빠지는 속도
	 */
	UPROPERTY(EditAnywhere, Category ="Lean")
	float LeanInterpSpeed = 8.0f;

public:
	AProjectTFCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 *  Current camera roll from leaning, in degrees. Read by the camera manager.
	 *  린으로 인한 현재 카메라 롤(도). 카메라 매니저가 읽습니다.
	 */
	UFUNCTION(BlueprintPure, Category="Lean")
	float GetLeanRoll() const { return CurrentLeanRoll; }

	/**
	 *  Current sideways camera offset from leaning, in cm. Read by the camera manager.
	 *  린으로 인한 현재 카메라 측면 오프셋(cm). 카메라 매니저가 읽습니다.
	 */
	UFUNCTION(BlueprintPure, Category="Lean")
	float GetLeanOffset() const { return CurrentLeanOffset; }

	UFUNCTION(BlueprintPure, Category="Components")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category="Components")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	//~Begin ICQBFactionAgent
	virtual ECQBFaction GetFaction() const override { return ECQBFaction::Player; }
	//~End ICQBFactionAgent

	//~Begin IAISightTargetInterface
	virtual UAISense_Sight::EVisibilityResult CanBeSeenFrom(const FCanBeSeenFromContext& Context,
		FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested,
		float& OutSightStrength, int32* UserData = nullptr,
		const FOnPendingVisibilityQueryProcessedDelegate* Delegate = nullptr) override;
	//~End IAISightTargetInterface

	/**
	 *  Doorway the player is currently aiming at, or null. Read by the HUD for the order hint.
	 *  플레이어가 지금 조준 중인 문. 없으면 null. HUD가 명령 힌트에 씁니다.
	 */
	UFUNCTION(BlueprintPure, Category="Squad")
	ADoorwayMarker* GetAimedDoorway() const { return AimedDoorway; }

	/**
	 *  Every living squad member, for the HUD
	 *  살아 있는 분대원 전원. HUD용입니다
	 */
	TArray<AAllyAIController*> GetSquad() const;

	/**
	 *  Every squad pawn, the fallen included, for the HUD roster.
	 *
	 *  Built from pawns rather than controllers on purpose: a controller destroys itself when
	 *  its pawn dies, so a roster built from controllers cannot show anyone as down.
	 *
	 *  전사자를 포함한 모든 분대 폰. HUD 명부용입니다.
	 *
	 *  일부러 컨트롤러가 아니라 폰에서 만듭니다. 컨트롤러는 자기 폰이 죽으면 스스로를
	 *  파괴하므로, 컨트롤러로 만든 명부는 전사자를 표시할 수가 없습니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Squad")
	TArray<ACQBCharacter*> GetSquadPawns() const;

	/**
	 *  Squad members the current element selection would take an order
	 *  현재 선택된 element 기준으로 명령을 받을 분대원들
	 */
	TArray<AAllyAIController*> GetSelectedSquad() const;

	/**
	 *  Which element the player is commanding right now
	 *  플레이어가 지금 지휘 중인 element
	 */
	UFUNCTION(BlueprintPure, Category="Squad")
	ESquadElement GetSelectedElement() const { return SelectedElement; }

	/**
	 *  Suspect under the crosshair that could be shouted at, or null
	 *  크로스헤어에 잡힌, 외칠 수 있는 용의자. 없으면 null
	 */
	UFUNCTION(BlueprintPure, Category="Squad")
	AActor* GetChallengeTarget() const { return ChallengeTarget; }

	/**
	 *  Issues a named order to the selected element. Used by the input bindings and by tests.
	 *  선택된 element에 이름으로 지정한 명령을 내립니다. 입력 바인딩과 테스트가 함께 씁니다.
	 */
	UFUNCTION(BlueprintCallable, Category="Squad")
	void IssueSquadOrder(const FString& OrderName, ADoorwayMarker* Doorway);

	/**
	 *  Shouts at a specific suspect, whatever the crosshair is on
	 *  크로스헤어가 어디를 보든, 지정한 용의자에게 외칩니다
	 */
	UFUNCTION(BlueprintCallable, Category="Squad")
	void IssueChallenge(AActor* Suspect);

protected:

	virtual void BeginPlay() override;

	/**
	 *  Called from Input Actions for movement input
	 *  이동 입력을 위해 Input Action에서 불립니다
	 */
	void MoveInput(const FInputActionValue& Value);

	/**
	 *  Called from Input Actions for looking input
	 *  시점 입력을 위해 Input Action에서 불립니다
	 */
	void LookInput(const FInputActionValue& Value);

	/**
	 *  Handles aim inputs from either controls or UI interfaces
	 *  컨트롤 또는 UI 인터페이스에서 오는 조준 입력을 처리합니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/**
	 *  Handles move inputs from either controls or UI interfaces
	 *  컨트롤 또는 UI 인터페이스에서 오는 이동 입력을 처리합니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/**
	 *  Handles jump start inputs from either controls or UI interfaces
	 *  컨트롤 또는 UI 인터페이스에서 오는 점프 시작 입력을 처리합니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/**
	 *  Handles jump end inputs from either controls or UI interfaces
	 *  컨트롤 또는 UI 인터페이스에서 오는 점프 종료 입력을 처리합니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/**
	 *  Pulls the trigger
	 *  방아쇠를 당깁니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoFireStart();

	/**
	 *  Releases the trigger
	 *  방아쇠를 놓습니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoFireStop();

	/**
	 *  Starts aiming down sights
	 *  정조준을 시작합니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoADSStart();

	/**
	 *  Stops aiming down sights
	 *  정조준을 해제합니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoADSStop();

	/**
	 *  Requests a reload
	 *  재장전을 요청합니다
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoReload();

	/**
	 *  Sets the lean target. -1 leans left, 1 leans right, 0 straightens up.
	 *  린 목표를 설정합니다. -1은 왼쪽, 1은 오른쪽, 0은 원위치입니다.
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoLean(float Direction);

	//~ Squad commands. Z falls them in, H holds them, and aiming at a doorway turns 1 and 2
	//~ into stack and clear orders for that doorway.
	//~ 분대 명령. Z는 대형 복귀, H는 정지이고, 문을 조준한 상태에서 1과 2는 그 문에 대한
	//~ 대기·소타 명령이 됩니다.
	void CommandFollow();
	void CommandHold();
	void CommandStackOrOne();
	void CommandClearOrTwo();

	/**
	 *  Orders the selected element to watch the point under the crosshair
	 *  선택된 element에게 크로스헤어 아래 지점을 주시하라고 명령합니다
	 */
	void CommandWatch();

	/**
	 *  Shouts at the suspect under the crosshair to give up
	 *  크로스헤어에 잡힌 용의자에게 항복하라고 외칩니다
	 */
	void CommandChallenge();

	/**
	 *  Mouse wheel cycles Red, Blue and the whole squad
	 *  마우스 휠로 Red, Blue, 분대 전체를 순환합니다
	 */
	void CycleElementUp();
	void CycleElementDown();

	/**
	 *  Doorways in the level, collected once. They are placed by hand and never spawned, and
	 *  walking every actor in the world twice a frame to find them is not worth a text hint.
	 *
	 *  레벨의 문들. 한 번만 모읍니다. 손으로 배치되며 스폰되지 않는데, 텍스트 힌트 하나를
	 *  띄우자고 매 프레임 월드 전체를 두 번 훑을 이유가 없습니다.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADoorwayMarker>> LevelDoorways;

	/**
	 *  Squad list, rebuilt when it changes rather than rebuilt every frame by the HUD.
	 *  분대 목록. HUD가 매 프레임 다시 만들지 않고, 바뀔 때만 다시 만듭니다.
	 */
	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<AAllyAIController>> CachedSquad;

	/**
	 *  Trace params for the order and challenge traces: ignores the player and the squad.
	 *  명령·외침 트레이스용 파라미터. 플레이어와 분대원을 무시합니다.
	 */
	void BuildOrderTraceParams(struct FCollisionQueryParams& OutParams) const;

	/**
	 *  Traces from the camera for a doorway the player might be ordering against
	 *  플레이어가 명령을 내릴 만한 문을 카메라에서 찾습니다
	 */
	void UpdateAimedDoorway();

	/**
	 *  Traces for a suspect the player could shout at
	 *  플레이어가 외칠 수 있는 용의자를 찾습니다
	 */
	void UpdateChallengeTarget();

	/**
	 *  Point under the crosshair, for move and watch orders. Returns false when nothing is hit.
	 *  크로스헤어 아래 지점. 이동·주시 명령에 씁니다. 아무것도 맞지 않으면 false입니다.
	 */
	bool GetAimedPoint(FVector& OutPoint) const;



	/**
	 *  How far the player can be from a doorway and still give orders about it
	 *  플레이어가 문에서 얼마나 떨어져도 그 문에 명령을 내릴 수 있는지
	 */
	UPROPERTY(EditDefaultsOnly, Category="Squad")
	float DoorwayAimRange = 1200.0f;

	/**
	 *  How far off centre the crosshair can be and still count as aiming at a doorway
	 *  크로스헤어가 중심에서 얼마나 벗어나도 문을 조준한 것으로 치는지
	 */
	UPROPERTY(EditDefaultsOnly, Category="Squad")
	float DoorwayAimAngle = 18.0f;

	/**
	 *  Doorway under the crosshair this frame
	 *  이번 프레임에 크로스헤어에 잡힌 문
	 */
	UPROPERTY(Transient)
	TObjectPtr<ADoorwayMarker> AimedDoorway;

	/**
	 *  Suspect under the crosshair this frame, if one is close enough to shout at
	 *  이번 프레임에 크로스헤어에 잡힌 용의자. 외칠 수 있을 만큼 가까울 때만
	 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> ChallengeTarget;

	/**
	 *  Element the orders go to
	 *  명령이 향하는 element
	 */
	ESquadElement SelectedElement = ESquadElement::All;

	/** How far a shouted demand carries */
	UPROPERTY(EditDefaultsOnly, Category="Squad")
	float ChallengeRange = 1500.0f;

	void LeanLeftStart() { DoLean(-1.0f); }
	void LeanRightStart() { DoLean(1.0f); }
	void LeanStop() { DoLean(0.0f); }

	/** Prints the damage taken and stops the player from acting once dead */
	UFUNCTION()
	void OnHealthChanged(UHealthComponent* HealthComp, float NewHealth, float Delta, AActor* Causer);

	UFUNCTION()
	void OnPlayerDeath(AActor* DeadActor, AActor* Killer);

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	/** Where the lean is heading: -1, 0 or 1 */
	float LeanTarget = 0.0f;

	/** Current interpolated lean roll, in degrees */
	float CurrentLeanRoll = 0.0f;

	/** Current interpolated lean offset, in cm */
	float CurrentLeanOffset = 0.0f;

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Returns the visible weapon **/
	UWeaponVisualComponent* GetWeaponVisual() const { return WeaponVisual; }

};
