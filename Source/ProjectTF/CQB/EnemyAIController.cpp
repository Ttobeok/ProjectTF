// CQB Sample - hand written AI state machine. No Behavior Tree involved.

#include "EnemyAIController.h"
#include "CQBTypes.h"
#include "EnemyCharacter.h"
#include "SquadManager.h"
#include "WeaponComponent.h"
#include "HealthComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
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

bool AEnemyAIController::IsHostile(const AActor* Actor) const
{
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

//~ Idle

void AEnemyAIController::EnterIdle()
{
	SetFiring(false);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	SetFacePlayerMode(false);
	bHasGoal = false;
}

void AEnemyAIController::UpdateIdle(float DeltaTime)
{
	// waiting for a sight or hearing stimulus, handled by the perception callbacks
}

void AEnemyAIController::ExitIdle()
{
}

//~ Investigate

void AEnemyAIController::EnterInvestigate()
{
	SetFiring(false);
	SetFacePlayerMode(false);
	ClearFocus(EAIFocusPriority::Gameplay);

	InvestigateWaitTime = 0.0f;

	MoveToPoint(LastStimulusLocation);
}

void AEnemyAIController::UpdateInvestigate(float DeltaTime)
{
	// reaching the spot and finding nothing for a few seconds means going back to idle
	if (HasReachedGoal())
	{
		StopMovement();
		InvestigateWaitTime += DeltaTime;

		if (InvestigateWaitTime >= InvestigateGiveUpTime)
		{
			SetState(ECQBAIState::Idle);
		}
	}
}

void AEnemyAIController::ExitInvestigate()
{
	InvestigateWaitTime = 0.0f;
}

//~ Engage

void AEnemyAIController::EnterEngage()
{
	SetFacePlayerMode(true);
	StopMovement();
	bHasGoal = false;

	if (CurrentTarget)
	{
		SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
	}

	SetFiring(true);

	// ask the squad what this enemy should be doing
	if (SquadRole == ESquadRole::None)
	{
		if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
		{
			SquadRole = Squad->RequestRole(this);
		}
	}
}

void AEnemyAIController::UpdateEngage(float DeltaTime)
{
	// shoot for a moment, then carry out the assigned role
	if (TimeInState < EngageMinDuration)
	{
		return;
	}

	const bool bIsFlanker = (SquadRole == ESquadRole::FlankerLeft || SquadRole == ESquadRole::FlankerRight);

	if (bIsFlanker && !bFlankCompleted)
	{
		SetState(ECQBAIState::Flank);
	}
	else
	{
		SetState(ECQBAIState::Cover);
	}
}

void AEnemyAIController::ExitEngage()
{
	SetFiring(false);
}

//~ Cover

void AEnemyAIController::EnterCover()
{
	SetFiring(false);
	SetFacePlayerMode(true);

	if (CurrentTarget)
	{
		SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
	}

	FindCoverPoint();
}

void AEnemyAIController::UpdateCover(float DeltaTime)
{
	// still waiting on the EQS result
	if (bCoverPointPending)
	{
		return;
	}

	if (HasReachedGoal())
	{
		StopMovement();
		SetState(ECQBAIState::Suppress);
	}
}

void AEnemyAIController::ExitCover()
{
	bCoverPointPending = false;
}

//~ Flank

void AEnemyAIController::EnterFlank()
{
	SetFiring(false);
	SetFacePlayerMode(true);

	if (CurrentTarget)
	{
		SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
	}

	ASquadManager* Squad = ASquadManager::GetSquadManager(this);
	if (!Squad || !CurrentTarget)
	{
		SetState(ECQBAIState::Cover);
		return;
	}

	const FVector FlankPoint = Squad->GetFlankPoint(this, CurrentTarget);
	MoveToPoint(FlankPoint);

	Squad->Broadcast(SquadRole == ESquadRole::FlankerLeft ? ECalloutType::FlankingLeft : ECalloutType::FlankingRight, this);
}

void AEnemyAIController::UpdateFlank(float DeltaTime)
{
	if (HasReachedGoal())
	{
		StopMovement();

		// one flank per fight, then hold and shoot
		bFlankCompleted = true;
		SetState(ECQBAIState::Engage);
	}
}

void AEnemyAIController::ExitFlank()
{
}

//~ Suppress

void AEnemyAIController::EnterSuppress()
{
	SetFacePlayerMode(true);
	StopMovement();
	bHasGoal = false;

	if (CurrentTarget)
	{
		SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
	}

	// start on a burst
	bSuppressBurstActive = true;
	SuppressPhaseTime = 0.0f;
	SetFiring(true);

	if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
	{
		Squad->Broadcast(ECalloutType::Suppressing, this);
	}
}

void AEnemyAIController::UpdateSuppress(float DeltaTime)
{
	SuppressPhaseTime += DeltaTime;

	// 2 seconds of fire, 1 second of pause
	const float PhaseDuration = bSuppressBurstActive ? SuppressFireDuration : SuppressRestDuration;

	if (SuppressPhaseTime >= PhaseDuration)
	{
		bSuppressBurstActive = !bSuppressBurstActive;
		SuppressPhaseTime = 0.0f;
		SetFiring(bSuppressBurstActive);
	}
}

void AEnemyAIController::ExitSuppress()
{
	SetFiring(false);
}

//~ Squad hooks --------------------------------------------------------------

void AEnemyAIController::OnCalloutReceived(ECalloutType Callout, AEnemyAIController* From)
{
	if (From == this || !From)
	{
		return;
	}

	// a squad mate spotting the player pulls idle enemies towards the contact
	if (Callout == ECalloutType::Contact && CurrentState == ECQBAIState::Idle)
	{
		LastStimulusLocation = From->GetLastKnownTargetLocation();
		SetState(ECQBAIState::Investigate);
	}
}

void AEnemyAIController::OnSquadRolesInvalidated()
{
	SquadRole = ESquadRole::None;
	bFlankCompleted = false;

	if (!IsInCombat())
	{
		return;
	}

	if (CurrentState == ECQBAIState::Engage)
	{
		// already engaging: just pick up the new role in place
		TimeInState = 0.0f;

		if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
		{
			SquadRole = Squad->RequestRole(this);
		}
	}
	else
	{
		SetState(ECQBAIState::Engage);
	}
}

void AEnemyAIController::OnPawnDied(AActor* DeadActor, AActor* Killer)
{
	SetFiring(false);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);

	// tell the squad before leaving it, so the others hear Man down and get new roles
	if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
	{
		Squad->NotifyEnemyDied(this);
	}

	UnPossess();
	Destroy();
}

//~ Helpers ------------------------------------------------------------------

void AEnemyAIController::SetFiring(bool bFire)
{
	if (UWeaponComponent* Weapon = GetWeapon())
	{
		if (bFire)
		{
			Weapon->StartFire();
		}
		else
		{
			Weapon->StopFire();
		}
	}
}

void AEnemyAIController::SetFacePlayerMode(bool bFacePlayer)
{
	ACharacter* MyCharacter = Cast<ACharacter>(GetPawn());
	if (!MyCharacter)
	{
		return;
	}

	// in combat the body follows the aim, otherwise it follows the path
	MyCharacter->bUseControllerRotationYaw = bFacePlayer;

	if (UCharacterMovementComponent* Movement = MyCharacter->GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = !bFacePlayer;
	}
}

void AEnemyAIController::MoveToPoint(const FVector& Goal)
{
	// a different goal starts its own retry budget
	if (!Goal.Equals(CurrentGoal, 1.0f))
	{
		MoveRetryCount = 0;
	}

	CurrentGoal = Goal;
	bHasGoal = true;

	TimeSinceMoveRequest = 0.0f;

	const EPathFollowingRequestResult::Type Result = MoveToLocation(Goal, 60.0f, true, true, true, true);
	bLastMoveFailed = (Result == EPathFollowingRequestResult::Failed);

	// Navmesh tiles are generated around the characters and take a moment on level load, so an
	// early request can fail on a goal that becomes reachable a second later. Tick retries it.
	if (bLastMoveFailed)
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB: %s could not path to %s (retry %d). Is the NavMesh built?"),
			*DisplayName, *Goal.ToCompactString(), MoveRetryCount);
	}
}

void AEnemyAIController::RetryFailedMove(float DeltaTime)
{
	if (!bHasGoal || !bLastMoveFailed)
	{
		return;
	}

	TimeSinceMoveRequest += DeltaTime;

	if (TimeSinceMoveRequest < MoveRetryInterval)
	{
		return;
	}

	if (MoveRetryCount >= MaxMoveRetries)
	{
		// give up quietly; the state machine treats the goal as reached and moves on
		bLastMoveFailed = false;
		return;
	}

	++MoveRetryCount;
	MoveToPoint(CurrentGoal);
}

bool AEnemyAIController::HasReachedGoal(float Tolerance) const
{
	const APawn* MyPawn = GetPawn();
	if (!bHasGoal || !MyPawn)
	{
		return true;
	}

	if (FVector::Dist2D(MyPawn->GetActorLocation(), CurrentGoal) <= Tolerance)
	{
		return true;
	}

	// still trying to get a path to this goal, so we have definitely not arrived
	if (bLastMoveFailed && MoveRetryCount < MaxMoveRetries)
	{
		return false;
	}

	// path following gave up or never found a path: treat it as arrived so the state can move on
	return TimeInState > 0.35f && GetMoveStatus() == EPathFollowingStatus::Idle;
}

//~ Cover search -------------------------------------------------------------

void AEnemyAIController::FindCoverPoint()
{
	bHasGoal = false;

	// EQS path, used when a query asset is assigned on the controller
	if (CoverQuery)
	{
		bCoverPointPending = true;

		FEnvQueryRequest Request(CoverQuery, this);
		Request.Execute(EEnvQueryRunMode::SingleResult, this, &AEnemyAIController::OnCoverQueryFinished);
		return;
	}

	// built in search: sample the NavMesh for a spot the player cannot see
	FVector CoverPoint;
	if (FindCoverPointFallback(CoverPoint))
	{
		MoveToPoint(CoverPoint);
	}
	else
	{
		// nowhere to hide, so shoot from where we stand
		SetState(ECQBAIState::Suppress);
	}
}

void AEnemyAIController::OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	bCoverPointPending = false;

	// the state may have moved on while the query was running
	if (CurrentState != ECQBAIState::Cover)
	{
		return;
	}

	if (Result.IsValid() && Result->IsSuccessful() && Result->Items.Num() > 0)
	{
		MoveToPoint(Result->GetItemAsLocation(0));
		return;
	}

	FVector CoverPoint;
	if (FindCoverPointFallback(CoverPoint))
	{
		MoveToPoint(CoverPoint);
	}
	else
	{
		SetState(ECQBAIState::Suppress);
	}
}

bool AEnemyAIController::FindCoverPointFallback(FVector& OutPoint) const
{
	const APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (!MyPawn || !World || !NavSys || !CurrentTarget)
	{
		return false;
	}

	const FVector PlayerEye = LastKnownTargetLocation + FVector(0.0f, 0.0f, 60.0f);
	const FVector MyLocation = MyPawn->GetActorLocation();

	float BestScore = -FLT_MAX;
	bool bFound = false;

	// sample the navigable space around us and keep the closest point the player cannot trace to
	for (int32 Attempt = 0; Attempt < 24; ++Attempt)
	{
		FNavLocation Candidate;
		if (!NavSys->GetRandomReachablePointInRadius(MyLocation, CoverSearchRadius, Candidate))
		{
			continue;
		}

		if (FVector::Dist(Candidate.Location, LastKnownTargetLocation) < MinCoverDistanceFromPlayer)
		{
			continue;
		}

		// chest height at the candidate position
		const FVector TestPoint = Candidate.Location + FVector(0.0f, 0.0f, 80.0f);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBCoverTrace), false);
		Params.AddIgnoredActor(MyPawn);
		Params.AddIgnoredActor(CurrentTarget);

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, PlayerEye, TestPoint, ECC_Visibility, Params);

		// no blocker means the spot is exposed
		if (!bBlocked)
		{
			continue;
		}

		// prefer cover that is close by, so the enemy does not run across the room
		const float Score = -FVector::Dist(Candidate.Location, MyLocation);
		if (Score > BestScore)
		{
			BestScore = Score;
			OutPoint = Candidate.Location;
			bFound = true;
		}
	}

	return bFound;
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
