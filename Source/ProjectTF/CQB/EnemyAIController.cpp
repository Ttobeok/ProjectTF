// CQB Sample - hand written AI state machine. No Behavior Tree involved.

#include "EnemyAIController.h"
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

	// join the squad and take a name
	if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
	{
		Squad->RegisterEnemy(this);
	}

	if (UHealthComponent* Health = GetHealth())
	{
		Health->OnDeath.AddDynamic(this, &AEnemyAIController::OnPawnDied);
	}

	SetFacePlayerMode(false);

	CurrentState = EEnemyState::Idle;
	EnterState(EEnemyState::Idle);
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
	return CurrentState == EEnemyState::Engage
		|| CurrentState == EEnemyState::Cover
		|| CurrentState == EEnemyState::Flank
		|| CurrentState == EEnemyState::Suppress;
}

bool AEnemyAIController::IsPlayerActor(const AActor* Actor) const
{
	const APawn* AsPawn = Cast<const APawn>(Actor);
	return AsPawn && AsPawn->GetController() && AsPawn->GetController()->IsA(APlayerController::StaticClass());
}

//~ Perception ---------------------------------------------------------------

void AEnemyAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// only the player matters in this sample
	if (!IsPlayerActor(Actor))
	{
		return;
	}

	const TSubclassOf<UAISense> SenseClass = UAIPerceptionSystem::GetSenseClassForStimulus(GetWorld(), Stimulus);

	if (SenseClass == UAISense_Sight::StaticClass())
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			PlayerTarget = Actor;
			bHasLineOfSight = true;
			TimeWithoutLineOfSight = 0.0f;
			LastKnownPlayerLocation = Actor->GetActorLocation();
			LastStimulusLocation = LastKnownPlayerLocation;

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
			PlayerTarget = Actor;
			LastStimulusLocation = Stimulus.StimulusLocation;

			if (CurrentState == EEnemyState::Idle)
			{
				SetState(EEnemyState::Investigate);
			}
		}
	}
}

void AEnemyAIController::UpdateSenses(float DeltaTime)
{
	if (!PlayerTarget)
	{
		bHasLineOfSight = false;
		return;
	}

	// perception drives acquisition through the vision cone; this confirms the sight line
	// every frame so ducking behind cover registers immediately instead of on the next update
	if (bHasLineOfSight && !LineOfSightTo(PlayerTarget))
	{
		bHasLineOfSight = false;
	}

	if (bHasLineOfSight)
	{
		LastKnownPlayerLocation = PlayerTarget->GetActorLocation();
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
	UpdateGlobalTransitions(DeltaTime);
	UpdateState(CurrentState, DeltaTime);

	DrawStateDebug(DeltaTime);
}

void AEnemyAIController::UpdateGlobalTransitions(float DeltaTime)
{
	// a dead player ends the fight
	if (PlayerTarget)
	{
		if (const UHealthComponent* PlayerHealth = UHealthComponent::FindHealthComponent(PlayerTarget))
		{
			if (PlayerHealth->IsDead())
			{
				if (CurrentState != EEnemyState::Idle)
				{
					SetState(EEnemyState::Idle);
				}
				return;
			}
		}
	}

	if (bHasLineOfSight)
	{
		TimeWithoutLineOfSight = 0.0f;

		if (CurrentState == EEnemyState::Idle || CurrentState == EEnemyState::Investigate)
		{
			SetState(EEnemyState::Engage);
		}

		return;
	}

	// lost the player for too long: fall back to searching the last known position
	if (IsInCombat())
	{
		TimeWithoutLineOfSight += DeltaTime;

		if (TimeWithoutLineOfSight >= LoseSightGraceTime)
		{
			if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
			{
				Squad->Broadcast(ECalloutType::LostVisual, this);
			}

			LastStimulusLocation = LastKnownPlayerLocation;
			SetState(EEnemyState::Investigate);
		}
	}
}

//~ State machine ------------------------------------------------------------

void AEnemyAIController::SetState(EEnemyState NewState)
{
	if (NewState == CurrentState)
	{
		return;
	}

	ExitState(CurrentState);
	CurrentState = NewState;
	TimeInState = 0.0f;
	EnterState(CurrentState);
}

void AEnemyAIController::EnterState(EEnemyState State)
{
	switch (State)
	{
	case EEnemyState::Idle:			EnterIdle(); break;
	case EEnemyState::Investigate:	EnterInvestigate(); break;
	case EEnemyState::Engage:		EnterEngage(); break;
	case EEnemyState::Cover:		EnterCover(); break;
	case EEnemyState::Flank:		EnterFlank(); break;
	case EEnemyState::Suppress:		EnterSuppress(); break;
	}
}

void AEnemyAIController::UpdateState(EEnemyState State, float DeltaTime)
{
	switch (State)
	{
	case EEnemyState::Idle:			UpdateIdle(DeltaTime); break;
	case EEnemyState::Investigate:	UpdateInvestigate(DeltaTime); break;
	case EEnemyState::Engage:		UpdateEngage(DeltaTime); break;
	case EEnemyState::Cover:		UpdateCover(DeltaTime); break;
	case EEnemyState::Flank:		UpdateFlank(DeltaTime); break;
	case EEnemyState::Suppress:		UpdateSuppress(DeltaTime); break;
	}
}

void AEnemyAIController::ExitState(EEnemyState State)
{
	switch (State)
	{
	case EEnemyState::Idle:			ExitIdle(); break;
	case EEnemyState::Investigate:	ExitInvestigate(); break;
	case EEnemyState::Engage:		ExitEngage(); break;
	case EEnemyState::Cover:		ExitCover(); break;
	case EEnemyState::Flank:		ExitFlank(); break;
	case EEnemyState::Suppress:		ExitSuppress(); break;
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
			SetState(EEnemyState::Idle);
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

	if (PlayerTarget)
	{
		SetFocus(PlayerTarget, EAIFocusPriority::Gameplay);
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
		SetState(EEnemyState::Flank);
	}
	else
	{
		SetState(EEnemyState::Cover);
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

	if (PlayerTarget)
	{
		SetFocus(PlayerTarget, EAIFocusPriority::Gameplay);
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
		SetState(EEnemyState::Suppress);
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

	if (PlayerTarget)
	{
		SetFocus(PlayerTarget, EAIFocusPriority::Gameplay);
	}

	ASquadManager* Squad = ASquadManager::GetSquadManager(this);
	if (!Squad || !PlayerTarget)
	{
		SetState(EEnemyState::Cover);
		return;
	}

	const FVector FlankPoint = Squad->GetFlankPoint(this, PlayerTarget);
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
		SetState(EEnemyState::Engage);
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

	if (PlayerTarget)
	{
		SetFocus(PlayerTarget, EAIFocusPriority::Gameplay);
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
	if (Callout == ECalloutType::Contact && CurrentState == EEnemyState::Idle)
	{
		LastStimulusLocation = From->GetLastKnownPlayerLocation();
		SetState(EEnemyState::Investigate);
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

	if (CurrentState == EEnemyState::Engage)
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
		SetState(EEnemyState::Engage);
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
	CurrentGoal = Goal;
	bHasGoal = true;

	MoveToLocation(Goal, 60.0f, true, true, true, true);
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
		SetState(EEnemyState::Suppress);
	}
}

void AEnemyAIController::OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	bCoverPointPending = false;

	// the state may have moved on while the query was running
	if (CurrentState != EEnemyState::Cover)
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
		SetState(EEnemyState::Suppress);
	}
}

bool AEnemyAIController::FindCoverPointFallback(FVector& OutPoint) const
{
	const APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (!MyPawn || !World || !NavSys || !PlayerTarget)
	{
		return false;
	}

	const FVector PlayerEye = LastKnownPlayerLocation + FVector(0.0f, 0.0f, 60.0f);
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

		if (FVector::Dist(Candidate.Location, LastKnownPlayerLocation) < MinCoverDistanceFromPlayer)
		{
			continue;
		}

		// chest height at the candidate position
		const FVector TestPoint = Candidate.Location + FVector(0.0f, 0.0f, 80.0f);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBCoverTrace), false);
		Params.AddIgnoredActor(MyPawn);
		Params.AddIgnoredActor(PlayerTarget);

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

	const FColor Color = IsInCombat() ? FColor::Red : (CurrentState == EEnemyState::Investigate ? FColor::Yellow : FColor::White);

	DrawDebugString(GetWorld(), MyPawn->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), Text, nullptr, Color, 0.0f, true);
}
