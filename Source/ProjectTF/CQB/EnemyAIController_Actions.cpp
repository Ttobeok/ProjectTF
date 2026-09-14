// CQB Sample - the verbs the states call: firing, facing, moving, and finding cover.

#include "EnemyAIController.h"
#include "CQBCharacter.h"
#include "SquadManager.h"
#include "WeaponComponent.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "ProjectTF.h"

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
