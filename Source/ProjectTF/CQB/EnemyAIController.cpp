// CQB Sample - hand written AI state machine. No Behavior Tree involved.
//
// This file holds the machine itself: senses in, state out. The states it dispatches to live in
// EnemyAIController_States.cpp, the things they do in _Actions.cpp, and the talking to the rest
// of the squad in _Squad.cpp.
//
// CQB 샘플 - 손으로 짠 AI 상태머신. Behavior Tree는 쓰지 않습니다.
//
// 이 파일은 머신 본체입니다 — 감각이 들어오고 상태가 나갑니다. 분기해 가는 상태들은
// EnemyAIController_States.cpp에, 그 상태들이 하는 일은 _Actions.cpp에, 분대와의 소통은
// _Squad.cpp에 있습니다.

#include "EnemyAIController.h"
#include "CQBTypes.h"
#include "CQBCharacter.h"
#include "SquadManager.h"
#include "WeaponComponent.h"
#include "HealthComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "ProjectTF.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AI Perception"));
	SetPerceptionComponent(*AIPerception);

	// sight: 2000 cm radius, 70 degree cone
	// 시야: 반경 2000cm, 70도 원뿔
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = SightRadius + 250.0f;
	SightConfig->PeripheralVisionAngleDegrees = SightAngle;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.0f;
	// no team setup in this sample, so everything is neutral and must still be detected
	// 이 샘플에는 팀 설정이 없어 모두 중립이므로, 그래도 탐지는 되어야 합니다
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// hearing: picks up the gunshot noise events reported by UWeaponComponent
	// 청각: UWeaponComponent가 보고하는 총성 노이즈 이벤트를 받습니다
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing Config"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->SetMaxAge(5.0f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// Affiliation is set on the shared struct rather than per flag, so a stale copy cannot leave
	// one of the three cleared. The sample has no use for filtering by side here: IsHostile
	// decides what to do with a contact after the sense has reported it.
	// 소속 필터를 플래그 하나씩이 아니라 공유 구조체로 통째 설정합니다. 그래야 묵은 사본이
	// 셋 중 하나를 꺼둔 채로 남기지 않습니다. 여기서 진영으로 거를 이유는 없습니다 —
	// 감각이 보고한 뒤 그 접촉을 어찌할지는 IsHostile이 정합니다.
	FAISenseAffiliationFilter DetectEverything;
	DetectEverything.bDetectEnemies = true;
	DetectEverything.bDetectNeutrals = true;
	DetectEverything.bDetectFriendlies = true;

	SightConfig->DetectionByAffiliation = DetectEverything;
	HearingConfig->DetectionByAffiliation = DetectEverything;

	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());

	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::OnPerceptionUpdated);
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// join the squad and take a name. The player's own squad is commanded directly instead.
	// 분대에 들어가고 이름을 받습니다. 플레이어 직속 분대는 대신 직접 지휘를 받습니다.
	if (ASquadManager* Squad = GetSquad())
	{
		Squad->RegisterEnemy(this);
	}

	if (UHealthComponent* Health = GetHealth())
	{
		Health->OnDeath.AddDynamic(this, &AEnemyAIController::OnPawnDied);
	}

	SetFacePlayerMode(false);

	CurrentState = ECQBAIState::Idle;
	EnterState(ECQBAIState::Idle);
}

void AEnemyAIController::OnUnPossess()
{
	SetFiring(false);

	Super::OnUnPossess();
}

UWeaponComponent* AEnemyAIController::GetWeapon() const
{
	APawn* MyPawn = GetPawn();
	return MyPawn ? MyPawn->FindComponentByClass<UWeaponComponent>() : nullptr;
}

UHealthComponent* AEnemyAIController::GetHealth() const
{
	APawn* MyPawn = GetPawn();
	return MyPawn ? MyPawn->FindComponentByClass<UHealthComponent>() : nullptr;
}

bool AEnemyAIController::IsInCombat() const
{
	return CurrentState == ECQBAIState::Engage
		|| CurrentState == ECQBAIState::Cover
		|| CurrentState == ECQBAIState::Flank
		|| CurrentState == ECQBAIState::Suppress;
}

FGenericTeamId AEnemyAIController::GetGenericTeamId() const
{
	return FGenericTeamId(Faction == ECQBFaction::Enemy ? 2 : 1);
}

bool AEnemyAIController::IsHostile(const AActor* Actor) const
{
	// hands up means out of the fight; the squad stops shooting at them
	// 손을 들었다면 전투에서 빠진 것입니다. 분대는 그에게 사격을 멈춥니다
	if (const ACQBCharacter* AsCharacter = Cast<const ACQBCharacter>(Actor))
	{
		if (AsCharacter->IsSurrendered())
		{
			return false;
		}
	}

	// a corpse is not a threat
	// 시체는 위협이 아닙니다
	if (const UHealthComponent* Health = UHealthComponent::FindHealthComponent(const_cast<AActor*>(Actor)))
	{
		if (Health->IsDead())
		{
			return false;
		}
	}

	return FCQBFactions::AreHostile(Faction, FCQBFactions::GetFaction(Actor));
}

//~ Perception ---------------------------------------------------------------
//~ Perception / 인지 -------------------------------------------------------

void AEnemyAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// Verbose: turn on with "log LogProjectTF Verbose" when a contact is not being picked up
	// Verbose: 접촉이 잡힐 때 "log LogProjectTF Verbose"로 켭니다
	UE_LOG(LogProjectTF, Verbose, TEXT("CQB perc: %s sensed %s (mine=%s theirs=%s hostile=%d)"),
		*DisplayName, *GetNameSafe(Actor),
		*FCQBNames::FactionToString(Faction),
		*FCQBNames::FactionToString(FCQBFactions::GetFaction(Actor)),
		IsHostile(Actor) ? 1 : 0);

	// friendly contacts are not worth reacting to
	// 아군 접촉은 반응할 가치가 없습니다
	if (!IsHostile(Actor))
	{
		return;
	}

	const TSubclassOf<UAISense> SenseClass = UAIPerceptionSystem::GetSenseClassForStimulus(GetWorld(), Stimulus);

	if (SenseClass == UAISense_Sight::StaticClass())
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			CurrentTarget = Actor;
			bHasLineOfSight = true;
			TimeWithoutLineOfSight = 0.0f;
			LastKnownTargetLocation = Actor->GetActorLocation();
			LastStimulusLocation = LastKnownTargetLocation;

			// first time anyone sees the player
			// 누군가 플레이어를 처음 본 순간
			if (!bHasSeenPlayerOnce)
			{
				bHasSeenPlayerOnce = true;

				if (ASquadManager* Squad = GetSquad())
				{
					Squad->Broadcast(ECalloutType::Contact, this);
				}
			}
		}
		else
		{
			bHasLineOfSight = false;
		}
	}
	else if (SenseClass == UAISense_Hearing::StaticClass())
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			// a gunshot is a reason to go and look, but it is not a sighting
			// 총성은 가보러 갈 이유는 되지만, 본 것은 아닙니다
			CurrentTarget = Actor;
			LastStimulusLocation = Stimulus.StimulusLocation;

			if (CurrentState == ECQBAIState::Idle)
			{
				SetState(ECQBAIState::Investigate);
			}
		}
	}
}

void AEnemyAIController::UpdateSenses(float DeltaTime)
{
	if (!CurrentTarget)
	{
		bHasLineOfSight = false;
		return;
	}

	// perception drives acquisition through the vision cone; this confirms the sight line
	// every frame so ducking behind cover registers immediately instead of on the next update
	// 획득 자체는 시야 원뿔로 인지가 합니다. 여기서는 매 프레임 시선을 확인해,
	// 엄폐물 뒤로 숨는 것이 다음 갱신이 아니라 즉시 반영되게 합니다
	if (bHasLineOfSight && !LineOfSightTo(CurrentTarget))
	{
		bHasLineOfSight = false;
	}

	if (bHasLineOfSight)
	{
		LastKnownTargetLocation = CurrentTarget->GetActorLocation();
	}
}

//~ Tick ---------------------------------------------------------------------
//~ Tick ---------------------------------------------------------------------

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GetPawn())
	{
		return;
	}

	if (const UHealthComponent* MyHealth = GetHealth())
	{
		if (MyHealth->IsDead())
		{
			return;
		}
	}

	TimeInState += DeltaTime;

	UpdateSenses(DeltaTime);
	RetryFailedMove(DeltaTime);
	UpdateGlobalTransitions(DeltaTime);
	UpdateState(CurrentState, DeltaTime);

	DrawStateDebug(DeltaTime);
}

ASquadManager* AEnemyAIController::GetSquad() const
{
	if (!ShouldJoinSquad())
	{
		return nullptr;
	}

	if (!CachedSquad.IsValid())
	{
		// the one place that may call the static lookup directly
		// 정적 조회를 직접 불러도 되는 유일한 자리입니다
		CachedSquad = ASquadManager::GetSquadManager(this);
	}

	return CachedSquad.Get();
}

void AEnemyAIController::OnTargetLost()
{
	if (CurrentState != ECQBAIState::Idle)
	{
		SetState(ECQBAIState::Idle);
	}
}

void AEnemyAIController::UpdateGlobalTransitions(float DeltaTime)
{
	// giving up is final for the rest of the encounter
	// 항복은 이번 교전이 끝날 때까지 되돌릴 수 없습니다
	if (CurrentState == ECQBAIState::Surrender)
	{
		return;
	}

	// A dead target ends the fight. Forgetting it matters as much as the state change: bodies
	// stay where they fall, so a target that is still referenced keeps this branch true on every
	// tick from now on, and the transitions below it are never reached again.
	//
	// 대상이 죽으면 전투도 끝납니다. 상태를 바꾸는 것만큼이나 그 대상을 잊는 것이 중요합니다.
	// 시체는 그 자리에 남으므로, 참조를 들고 있으면 이 분기가 매 틱 참이 되어 아래의 전이
	// 규칙에 다시는 도달하지 못합니다.
	if (CurrentTarget)
	{
		if (const UHealthComponent* TargetHealth = UHealthComponent::FindHealthComponent(CurrentTarget))
		{
			if (TargetHealth->IsDead())
			{
				CurrentTarget = nullptr;
				bHasLineOfSight = false;
				TimeWithoutLineOfSight = 0.0f;

				OnTargetLost();
				return;
			}
		}
	}

	if (bHasLineOfSight)
	{
		TimeWithoutLineOfSight = 0.0f;

		if (CurrentState == ECQBAIState::Idle || CurrentState == ECQBAIState::Investigate)
		{
			SetState(ECQBAIState::Engage);
		}

		return;
	}

	// lost the player for too long: fall back to searching the last known position.
	// Flank is exempt: breaking the sight line is the whole point of going around,
	// and the flanking route is long enough that this rule would cancel every flank.
	// 플레이어를 너무 오래 놓쳐습니다. 마지막으로 알던 위치를 수색하는 쪽으로 돌아갑니다.
	// Flank는 예외입니다 — 시선을 끊는 것이 우회의 목적 그 자체이고,
	// 우회로가 충분히 길어서 이 규칙을 그대로 적용하면 모든 우회가 취소됩니다.
	if (IsInCombat() && CurrentState != ECQBAIState::Flank)
	{
		TimeWithoutLineOfSight += DeltaTime;

		if (TimeWithoutLineOfSight >= LoseSightGraceTime)
		{
			if (ASquadManager* Squad = GetSquad())
			{
				Squad->Broadcast(ECalloutType::LostVisual, this);
			}

			LastStimulusLocation = LastKnownTargetLocation;
			SetState(ECQBAIState::Investigate);
		}
	}
}

//~ State machine ------------------------------------------------------------
//~ State machine / 상태머신 -------------------------------------------------

void AEnemyAIController::SetState(ECQBAIState NewState)
{
	if (NewState == CurrentState)
	{
		return;
	}

	UE_LOG(LogProjectTF, Log, TEXT("CQB: %s  %s -> %s"), *DisplayName,
		*FCQBNames::StateToString(CurrentState), *FCQBNames::StateToString(NewState));

	ExitState(CurrentState);
	CurrentState = NewState;
	TimeInState = 0.0f;
	EnterState(CurrentState);
}

void AEnemyAIController::EnterState(ECQBAIState State)
{
	switch (State)
	{
	case ECQBAIState::Idle:			EnterIdle(); break;
	case ECQBAIState::Investigate:	EnterInvestigate(); break;
	case ECQBAIState::Engage:		EnterEngage(); break;
	case ECQBAIState::Cover:		EnterCover(); break;
	case ECQBAIState::Flank:		EnterFlank(); break;
	case ECQBAIState::Suppress:		EnterSuppress(); break;
	case ECQBAIState::Surrender:	EnterSurrender(); break;
	}
}

void AEnemyAIController::UpdateState(ECQBAIState State, float DeltaTime)
{
	switch (State)
	{
	case ECQBAIState::Idle:			UpdateIdle(DeltaTime); break;
	case ECQBAIState::Investigate:	UpdateInvestigate(DeltaTime); break;
	case ECQBAIState::Engage:		UpdateEngage(DeltaTime); break;
	case ECQBAIState::Cover:		UpdateCover(DeltaTime); break;
	case ECQBAIState::Flank:		UpdateFlank(DeltaTime); break;
	case ECQBAIState::Suppress:		UpdateSuppress(DeltaTime); break;
	case ECQBAIState::Surrender:	UpdateSurrender(DeltaTime); break;
	}
}

void AEnemyAIController::ExitState(ECQBAIState State)
{
	switch (State)
	{
	case ECQBAIState::Idle:			ExitIdle(); break;
	case ECQBAIState::Investigate:	ExitInvestigate(); break;
	case ECQBAIState::Engage:		ExitEngage(); break;
	case ECQBAIState::Cover:		ExitCover(); break;
	case ECQBAIState::Flank:		ExitFlank(); break;
	case ECQBAIState::Suppress:		ExitSuppress(); break;
	}
}


//~ Debug --------------------------------------------------------------------
//~ Debug / 디버그 -----------------------------------------------------------

void AEnemyAIController::DrawStateDebug(float DeltaTime) const
{
	const APawn* MyPawn = GetPawn();
	if (!bDrawStateDebug || !MyPawn)
	{
		return;
	}

	const FString Text = FString::Printf(TEXT("%s  [%s]  %s%s"),
		*DisplayName,
		*FCQBNames::StateToString(CurrentState),
		*FCQBNames::RoleToString(SquadRole),
		bHasLineOfSight ? TEXT("  LOS") : TEXT(""));

	const FColor Color = IsInCombat() ? FColor::Red : (CurrentState == ECQBAIState::Investigate ? FColor::Yellow : FColor::White);

	DrawDebugString(GetWorld(), MyPawn->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), Text, nullptr, Color, 0.0f, true);
}
