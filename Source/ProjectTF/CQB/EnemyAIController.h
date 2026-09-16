// CQB Sample - hand written AI state machine. No Behavior Tree involved.
// CQB 샘플 - 손으로 짠 AI 상태머신. Behavior Tree는 쓰지 않습니다.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CQBTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UEnvQuery;
class UWeaponComponent;
class UHealthComponent;
class ASquadManager;
struct FAIStimulus;
struct FEnvQueryResult;

/**
 *  Enemy brain.
 *
 *  Perception feeds a plain C++ state machine: every state has Enter / Update / Exit,
 *  Tick runs the Update of the current state and SetState() drives transitions.
 *  Squad level decisions (who suppresses, who flanks, where to flank) come from ASquadManager.
 *
 *  The implementation is split across four files, by role rather than by size:
 *
 *    EnemyAIController.cpp          the machine - senses, Tick, transitions, dispatch
 *    EnemyAIController_States.cpp   the states  - one Enter/Update/Exit trio each
 *    EnemyAIController_Actions.cpp  the verbs   - firing, facing, moving, finding cover
 *    EnemyAIController_Squad.cpp    the talking - callouts, role reassignment, surrender
 *
 *  적 AI의 뇌.
 *
 *  인지가 평범한 C++ 상태머신으로 들어갑니다. 모든 상태에 Enter / Update / Exit가 있고,
 *  Tick이 현재 상태의 Update를 돌리며 SetState()가 전이를 일으킵니다.
 *  분대 단위 판단(누가 제압하고 누가 우회할지, 어디로 우회할지)은 ASquadManager가 냅니다.
 *
 *  구현은 크기가 아니라 역할 기준으로 네 파일에 나뉘어 있습니다:
 *
 *    EnemyAIController.cpp          머신 본체 - 감각, Tick, 전이, 디스패치
 *    EnemyAIController_States.cpp   상태들   - 상태마다 Enter/Update/Exit 한 벌
 *    EnemyAIController_Actions.cpp  동작들   - 사격, 조준, 이동, 엄폐 탐색
 *    EnemyAIController_Squad.cpp    소통     - 콜아웃, 역할 재배정, 항복
 */
UCLASS()
class PROJECTTF_API AEnemyAIController : public AAIController, public ICQBFactionAgent
{
	GENERATED_BODY()

public:

	AEnemyAIController();

	virtual void Tick(float DeltaTime) override;

	/**
	 *  Current state of the machine
	 *  머신의 현재 상태
	 */
	UFUNCTION(BlueprintPure, Category = "AI")
	ECQBAIState GetState() const { return CurrentState; }

	/**
	 *  Runs Exit on the old state and Enter on the new one. Re-entering the same state is ignored.
	 *  이전 상태의 Exit와 새 상태의 Enter를 돌립니다. 같은 상태로의 재진입은 무시됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetState(ECQBAIState NewState);

	/**
	 *  Role handed out by the squad manager
	 *  분대 매니저가 배정한 역할
	 */
	ESquadRole GetSquadRole() const { return SquadRole; }
	void SetSquadRole(ESquadRole NewRole) { SquadRole = NewRole; }

	/**
	 *  Name used by the debug draw and the callouts, e.g. Enemy_1
	 *  디버그 표시와 콜아웃에 쓰는 이름. 예: Enemy_1
	 */
	const FString& GetDisplayName() const { return DisplayName; }
	void SetDisplayName(const FString& InName) { DisplayName = InName; }

	/**
	 *  Last position this enemy actually saw the player at
	 *  이 적이 실제로 플레이어를 마지막으로 본 위치
	 */
	FVector GetLastKnownTargetLocation() const { return LastKnownTargetLocation; }

	/**
	 *  The player this enemy is tracking, if any
	 *  이 적이 추적 중인 플레이어. 없을 수도 있습니다
	 */
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	/**
	 *  True while this enemy is fighting rather than idling or searching
	 *  대기나 수색이 아니라 교전 중이면 true
	 */
	bool IsInCombat() const;

	//~Begin ICQBFactionAgent
	virtual ECQBFaction GetFaction() const override { return Faction; }
	//~End ICQBFactionAgent

	/**
	 *  Team id derived from the faction.
	 *
	 *  The sight sense filters what it reports by team attitude, so leaving every controller on
	 *  the default team makes cross side detection unreliable. Player and allies share a team,
	 *  enemies get their own.
	 *
	 *  진영에서 유도되는 팀 ID입니다.
	 *
	 *  시야 감각이 팀 태도로 보고 대상을 거르기 때문에, 모든 컨트롤러를 기본 팀에 두면
	 *  진영 간 탐지가 불안정해집니다. 플레이어와 아군이 한 팀, 적이 따로 한 팀입니다.
	 */
	virtual FGenericTeamId GetGenericTeamId() const override;

	/**
	 *  Someone shouted at this AI to give up.
	 *
	 *  Pressure is how convincing the demand is, 0 to 1, and it is weighed against how much
	 *  fight the AI has left. Returns true when it complies.
	 *
	 *  누군가 이 AI에게 항복하라고 외쳤습니다.
	 *
	 *  Pressure는 그 요구가 얼마나 설득력 있는지를 0~1로 나타내며, AI에게 남은 전의와
	 *  저울질됩니다. 순응하면 true를 돌려줍니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool ReceiveChallenge(AActor* Challenger, float Pressure);

	/**
	 *  True once this AI has given up
	 *  이 AI가 항복했으면 true
	 */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool HasSurrendered() const { return CurrentState == ECQBAIState::Surrender; }

	/**
	 *  A squad mate said something. Idle enemies move to investigate a contact.
	 *  분대원이 뭔가 말했습니다. 대기 중인 적은 접촉 보고를 확인하러 움직입니다.
	 */
	void OnCalloutReceived(ECalloutType Callout, AEnemyAIController* From);

	/**
	 *  The squad dropped every role, so ask for a new one
	 *  분대가 모든 역할을 비웠으니 새로 요청합니다
	 */
	void OnSquadRolesInvalidated();

public:

	/**
	 *  Issues a move order and remembers the goal for the arrival test
	 *  이동 명령을 내리고, 도착 판정을 위해 목표를 기억합니다
	 */
	void MoveToPoint(const FVector& Goal);

	/**
	 *  True once the pawn is close enough to the goal, or the path following gave up
	 *  폰이 목표에 충분히 가까워졌거나 경로 추종이 포기했으면 true
	 */
	bool HasReachedGoal(float Tolerance = 140.0f) const;

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void BeginPlay() override;

	//~ State machine

	/**
	 *  Dispatches to the per state Enter function
	 *  상태별 Enter 함수로 분기
	 */
	virtual void EnterState(ECQBAIState State);

	/**
	 *  Dispatches to the per state Update function
	 *  상태별 Update 함수로 분기
	 */
	virtual void UpdateState(ECQBAIState State, float DeltaTime);

	/**
	 *  Dispatches to the per state Exit function
	 *  상태별 Exit 함수로 분기
	 */
	virtual void ExitState(ECQBAIState State);

	void EnterIdle();
	void UpdateIdle(float DeltaTime);
	void ExitIdle();

	void EnterInvestigate();
	void UpdateInvestigate(float DeltaTime);
	void ExitInvestigate();

	void EnterEngage();
	void UpdateEngage(float DeltaTime);
	void ExitEngage();

	void EnterCover();
	void UpdateCover(float DeltaTime);
	void ExitCover();

	void EnterFlank();
	void UpdateFlank(float DeltaTime);
	void ExitFlank();

	void EnterSuppress();
	void UpdateSuppress(float DeltaTime);
	void ExitSuppress();

	void EnterSurrender();
	void UpdateSurrender(float DeltaTime);

	/**
	 *  How close this AI is to giving up, 0 to 1. Hurt, alone and cornered all push it up.
	 *  이 AI가 항복에 얼마나 가까운지, 0~1. 다치고 고립되고 몰릴수록 올라갑니다.
	 */
	float EvaluateCompliance(const AActor* Challenger) const;

	/**
	 *  Transitions that apply no matter which state is running, such as losing the player
	 *  어느 상태든 상관없이 적용되는 전이. 플레이어를 놓치는 경우 등
	 */
	virtual void UpdateGlobalTransitions(float DeltaTime);

	//~ Perception

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/**
	 *  The squad this controller answers to, or null for anyone who does not join one.
	 *
	 *  Every squad call goes through here rather than ASquadManager::GetSquadManager, because an
	 *  ally reaching into the enemy squad hands the opposition a free role reshuffle every time a
	 *  squad member dies. Looked up once; the lookup walks every actor in the world.
	 *
	 *  이 컨트롤러가 속한 분대. 분대에 들어가지 않는 쪽에는 null입니다.
	 *
	 *  분대 호출은 전부 여기를 거칩니다. 아군이 적 분대를 건드리면 아군이 죽을 때마다
	 *  적이 공짜로 역할을 재편성하게 됩니다. 조회는 한 번만 합니다 — 월드 전체를 훑기 때문입니다.
	 */
	ASquadManager* GetSquad() const;

	/**
	 *  The current target died. An enemy stands down; an ally goes back to its standing order.
	 *  현재 대상이 죽었습니다. 적은 대기로, 아군은 받아둔 명령으로 돌아갑니다.
	 */
	virtual void OnTargetLost();

	/**
	 *  True when the actor belongs to a faction this one shoots at
	 *  그 액터가 이쪽이 쏘는 진영에 속하면 true
	 */
	bool IsHostile(const AActor* Actor) const;

	/**
	 *  Squad members register with ASquadManager; the player's own squad does not
	 *  분대원은 ASquadManager에 등록합니다. 플레이어 직속 분대는 등록하지 않습니다
	 */
	virtual bool ShouldJoinSquad() const { return true; }

	/**
	 *  Refreshes bHasLineOfSight and the last known location
	 *  bHasLineOfSight와 마지막으로 알던 위치를 갱신
	 */
	void UpdateSenses(float DeltaTime);

	//~ Helpers

	/**
	 *  Starts or stops the weapon trigger
	 *  무기 방아쇠를 당기거나 놓습니다
	 */
	void SetFiring(bool bFire);

	/**
	 *  Faces the target while fighting, faces the movement direction otherwise
	 *  교전 중에는 대상을, 아니면 이동 방향을 바라봅니다
	 */
	void SetFacePlayerMode(bool bFacePlayer);

	/**
	 *  Re-issues a move order that could not be pathed, while the navmesh finishes generating
	 *  navmesh가 아직 생성 중일 때, 경로를 못 찾은 이동 명령을 다시 냅니다
	 */
	void RetryFailedMove(float DeltaTime);

	/**
	 *  Runs the cover EQS when one is assigned, otherwise samples the NavMesh in C++
	 *  EQS가 지정돼 있으면 그걸 돌리고, 아니면 C++로 NavMesh를 샘플링합니다
	 */
	void FindCoverPoint();

	/**
	 *  EQS result handler
	 *  EQS 결과 처리
	 */
	void OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	/**
	 *  NavMesh sampling used when no EQS asset is assigned: points the player cannot trace to
	 *  EQS 에셋이 없을 때 쓰는 NavMesh 샘플링. 플레이어가 트레이스로 닿지 못하는 지점들
	 */
	bool FindCoverPointFallback(FVector& OutPoint) const;

	/**
	 *  Weapon of the possessed pawn
	 *  빙의한 폰의 무기
	 */
	UWeaponComponent* GetWeapon() const;

	/**
	 *  Health of the possessed pawn
	 *  빙의한 폰의 체력
	 */
	UHealthComponent* GetHealth() const;

	/**
	 *  Pawn death hook: calls out Man down, leaves the squad and destroys the controller
	 *  폰 사망 훅: Man down을 외치고, 분대에서 빠지고, 컨트롤러를 파괴합니다
	 */
	UFUNCTION()
	void OnPawnDied(AActor* DeadActor, AActor* Killer);

	/**
	 *  Draws the state name over the pawn's head
	 *  폰 머리 위에 상태 이름을 그립니다
	 */
	void DrawStateDebug(float DeltaTime) const;

	//~ Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UAIPerceptionComponent* AIPerception;

	UPROPERTY()
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY()
	UAISenseConfig_Hearing* HearingConfig;

	//~ Tuning

	/**
	 *  Copies the tuning properties below into the sense configs. Called from BeginPlay, because
	 *  a Blueprint subclass has not applied its overrides yet when the constructor runs.
	 *
	 *  아래 튜닝 값들을 감각 설정에 복사합니다. BeginPlay에서 부릅니다. 생성자가 도는 시점에는
	 *  블루프린트 하위 클래스의 오버라이드가 아직 적용되지 않았기 때문입니다.
	 */
	void ApplySenseTuning();

	/**
	 *  How far past SightRadius a target has to get before it is dropped, in cm
	 *  이미 본 대상을 놓치기까지 SightRadius를 얼마나 넘어가야 하는지(cm)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float LoseSightMargin = 250.0f;

	/**
	 *  Sight radius in cm
	 *  시야 반경(cm)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightRadius = 2000.0f;

	/**
	 *  Half angle of the vision cone in degrees
	 *  시야 원뿔의 반각(도)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightAngle = 70.0f;

	/**
	 *  How far gunshots are heard, in cm
	 *  총성이 들리는 거리(cm)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float HearingRange = 3000.0f;

	/**
	 *  Seconds without line of sight before the enemy goes back to searching
	 *  시야를 잃고 수색으로 돌아가기까지의 시간(초)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float LoseSightGraceTime = 3.0f;

	/**
	 *  Seconds spent looking around an investigation point before giving up
	 *  조사 지점 주변을 둘러보다 포기하기까지의 시간(초)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float InvestigateGiveUpTime = 3.0f;

	/**
	 *  Minimum time spent shooting in Engage before taking up a role
	 *  Engage에서 역할을 맡기 전에 최소한 쓰는 시간
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float EngageMinDuration = 1.2f;

	/**
	 *  Seconds of suppressing fire per burst
	 *  제압 사격 한 번의 지속 시간(초)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float SuppressFireDuration = 2.0f;

	/**
	 *  Seconds of pause between suppressing bursts
	 *  제압 사격 사이의 쉬는 시간(초)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float SuppressRestDuration = 1.0f;

	/**
	 *  Radius searched for a cover position, in cm
	 *  엄폐 지점을 찾는 반경(cm)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float CoverSearchRadius = 800.0f;

	/**
	 *  Enemies refuse cover positions closer than this to the player, in cm
	 *  플레이어에게 이보다 가까운 엄폐 지점은 거부합니다(cm)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float MinCoverDistanceFromPlayer = 350.0f;

	/**
	 *  Optional EQS query used to pick a cover point.
	 *  Leave empty to use the built in NavMesh search; assign a query to use EQS instead.
	 *  Use UEnvQueryContext_CQBPlayer as the query context for the player.
	 *
	 *  엄폐 지점을 고를 때 쓰는 선택적 EQS 쿼리입니다.
	 *  비워두면 내장 NavMesh 탐색을 쓰고, 쿼리를 지정하면 EQS를 씁니다.
	 *  플레이어를 가리키는 쿼리 컨텍스트로는 UEnvQueryContext_CQBPlayer를 쓰세요.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	TObjectPtr<UEnvQuery> CoverQuery;

	/**
	 *  Seconds between retries of a move request that could not be pathed
	 *  경로를 못 찾은 이동 요청을 재시도하는 간격(초)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float MoveRetryInterval = 0.5f;

	/**
	 *  How many times a move request is retried before the goal is abandoned
	 *  목표를 포기하기까지 이동 요청을 재시도하는 횟수
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	int32 MaxMoveRetries = 12;

	/**
	 *  Side this controller fights for
	 *  이 컨트롤러가 싸우는 편
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	ECQBFaction Faction = ECQBFaction::Enemy;

	/**
	 *  Wounded AI gives up more readily. Below this share of health it is already wavering.
	 *  다친 AI는 더 쉽게 포기합니다. 체력이 이 비율 아래면 이미 흔들리고 있는 상태입니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Compliance")
	float ComplianceHealthThreshold = 0.6f;

	/**
	 *  A demand shouted from further than this carries no weight
	 *  이보다 멀리서 외친 요구는 아무 무게도 없습니다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Compliance")
	float ComplianceRange = 1200.0f;

	/**
	 *  Total pressure needed before the AI gives up
	 *  AI가 포기하기까지 필요한 총 압력
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Compliance")
	float ComplianceThreshold = 1.0f;

	/**
	 *  Pressure from a demand shouted right in their face, falling off to nothing at ComplianceRange
	 *  코앉에서 외쳤을 때의 압력. ComplianceRange에서 0으로 떨어집니다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Compliance")
	float ComplianceProximityWeight = 0.5f;

	/**
	 *  Pressure from having nobody else left standing on your side
	 *  같은 편이 아무도 남지 않았을 때의 압력
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Compliance")
	float ComplianceIsolationWeight = 0.4f;

	/**
	 *  Pressure lost when the AI cannot actually see who is shouting
	 *  외치는 사람이 실제로 보이지 않을 때 긎이는 압력
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Compliance")
	float ComplianceBlindPenalty = 0.3f;

	/**
	 *  Draw the state name over the pawn
	 *  폰 위에 상태 이름을 그릴지
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
	bool bDrawStateDebug = true;

	//~ Runtime state

	ECQBAIState CurrentState = ECQBAIState::Idle;

	ESquadRole SquadRole = ESquadRole::None;

	FString DisplayName = TEXT("Enemy");

	/**
	 *  Squad manager, looked up once. GetSquadManager walks every actor in the world.
	 *  분대 매니저. 한 번만 찾습니다. GetSquadManager는 월드의 모든 액터를 훑습니다.
	 */
	mutable TWeakObjectPtr<ASquadManager> CachedSquad;

	/**
	 *  The player pawn, once perceived
	 *  인지한 뒤의 플레이어 폰
	 */
	UPROPERTY()
	TObjectPtr<AActor> CurrentTarget;

	FVector LastKnownTargetLocation = FVector::ZeroVector;

	/**
	 *  Where the last sight or hearing stimulus came from
	 *  마지막 시각/청각 자극이 온 위치
	 */
	FVector LastStimulusLocation = FVector::ZeroVector;

	/**
	 *  Current move goal, used for the arrival test
	 *  현재 이동 목표. 도착 판정에 씁니다
	 */
	FVector CurrentGoal = FVector::ZeroVector;

	bool bHasGoal = false;

	/**
	 *  True when the last move request could not be pathed, usually a navmesh that is still building
	 *  마지막 이동 요청이 경로를 못 찾았으면 true. 보통 navmesh가 아직 생성 중일 때입니다
	 */
	bool bLastMoveFailed = false;

	/**
	 *  Seconds since the last move request, used to pace retries
	 *  마지막 이동 요청 이후 경과 시간. 재시도 간격 조절에 씁니다
	 */
	float TimeSinceMoveRequest = 0.0f;

	/**
	 *  How many times the current goal has been retried
	 *  현재 목표를 몇 번 재시도했는지
	 */
	int32 MoveRetryCount = 0;
	bool bHasLineOfSight = false;
	bool bHasSeenPlayerOnce = false;
	bool bFlankCompleted = false;
	bool bCoverPointPending = false;
	bool bSuppressBurstActive = true;

	float TimeWithoutLineOfSight = 0.0f;
	float TimeInState = 0.0f;
	float InvestigateWaitTime = 0.0f;
	float SuppressPhaseTime = 0.0f;
};
