// CQB Sample - hand written AI state machine. No Behavior Tree involved.
//
// This file holds the machine itself: senses in, state out. The states it dispatches to live in
// EnemyAIController_States.cpp, the things they do in _Actions.cpp, and the talking to the rest
// of the squad in _Squad.cpp.

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
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = SightRadius + 250.0f;
	SightConfig->PeripheralVisionAngleDegrees = SightAngle;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.0f;
	// no team setup in this sample, so everything is neutral and must still be detected
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// hearing: picks up the gunshot noise events reported by UWeaponComponent
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing Config"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->SetMaxAge(5.0f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// Affiliation is set on the shared struct rather than per flag, so a stale copy cannot leave
	// one of the three cleared. The sample has no use for filtering by side here: IsHostile
	// decides what to do with a contact after the sense has reported it.
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
	if (ShouldJoinSquad())
	{
		if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
		{
			Squad->RegisterEnemy(this);
		}
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
	if (const ACQBCharacter* AsCharacter = Cast<const ACQBCharacter>(Actor))
	{
		if (AsCharacter->IsSurrendered())
		{
			return false;
		}
	}

	// a corpse is not a threat
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

void AEnemyAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// Verbose: turn on with "log LogProjectTF Verbose" when a contact is not being picked up
	UE_LOG(LogProjectTF, Verbose, TEXT("CQB perc: %s sensed %s (mine=%s theirs=%s hostile=%d)"),
		*DisplayName, *GetNameSafe(Actor),
		*FCQBNames::FactionToString(Faction),
		*FCQBNames::FactionToString(FCQBFactions::GetFaction(Actor)),
		IsHostile(Actor) ? 1 : 0);

	// friendly contacts are not worth reacting to
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
			if (!bHasSeenPlayerOnce)
			{
				bHasSeenPlayerOnce = true;

				if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
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

void AEnemyAIController::UpdateGlobalTransitions(float DeltaTime)
{
	// giving up is final for the rest of the encounter
	if (CurrentState == ECQBAIState::Surrender)
	{
		return;
	}

	// a dead target ends the fight
	if (CurrentTarget)
	{
		if (const UHealthComponent* PlayerHealth = UHealthComponent::FindHealthComponent(CurrentTarget))
		{
			if (PlayerHealth->IsDead())
			{
				if (CurrentState != ECQBAIState::Idle)
				{
					SetState(ECQBAIState::Idle);
				}
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
	if (IsInCombat() && CurrentState != ECQBAIState::Flank)
	{
		TimeWithoutLineOfSight += DeltaTime;

		if (TimeWithoutLineOfSight >= LoseSightGraceTime)
		{
			if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
			{
				Squad->Broadcast(ECalloutType::LostVisual, this);
			}

			LastStimulusLocation = LastKnownTargetLocation;
			SetState(ECQBAIState::Investigate);
		}
	}
}

//~ State machine ------------------------------------------------------------

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
