// CQB Sample - the verbs the states call: firing, facing, moving, and finding cover.
// CQB 샘플 - 상태가 불러 쓰는 동작들: 사격, 조준, 이동, 엄폐 탐색.

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
//~ Helpers / 헬퍼 ----------------------------------------------------------

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
	// 교전 중에는 몸이 조준을, 아니면 이동 경로를 따릅니다
	MyCharacter->bUseControllerRotationYaw = bFacePlayer;

	if (UCharacterMovementComponent* Movement = MyCharacter->GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = !bFacePlayer;
	}
}

void AEnemyAIController::MoveToPoint(const FVector& Goal)
{
	// a different goal starts its own retry budget
	// 목표가 바뀌면 재시도 예산도 새로 시작합니다
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
	// navmesh 타일은 캐릭터 주변에 생성되고 레벨 로드 직후에는 시간이 걸립니다.
	// 그래서 이른 요청은 1초 뒤면 닿을 목표에도 실패합니다. Tick이 재시도합니다.
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
		// 조용히 포기합니다. 상태머신은 도착한 것으로 치고 다음으로 넘어갑니다
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
	// 아직 이 목표로의 경로를 찾는 중이므로, 도착한 것은 확실히 아닙니다
	if (bLastMoveFailed && MoveRetryCount < MaxMoveRetries)
	{
		return false;
	}

	// path following gave up or never found a path: treat it as arrived so the state can move on
	// 경로 추종이 포기했거나 애초에 경로가 없었습니다. 상태가 진행되도록 도착으로 칩니다
	return TimeInState > 0.35f && GetMoveStatus() == EPathFollowingStatus::Idle;
}

//~ Cover search -------------------------------------------------------------
//~ Cover search / 엄폐 탐색 ---------------------------------------------------

void AEnemyAIController::FindCoverPoint()
{
	bHasGoal = false;

	// EQS path, used when a query asset is assigned on the controller
	// EQS 경로. 컨트롤러에 쿼리 에셋이 지정돈을 때 쓵니다
	if (CoverQuery)
	{
		bCoverPointPending = true;

		FEnvQueryRequest Request(CoverQuery, this);
		Request.Execute(EEnvQueryRunMode::SingleResult, this, &AEnemyAIController::OnCoverQueryFinished);
		return;
	}

	// built in search: sample the NavMesh for a spot the player cannot see
	// 내장 탐색: 플레이어가 볼 수 없는 지점을 NavMesh에서 샘플링합니다
	FVector CoverPoint;
	if (FindCoverPointFallback(CoverPoint))
	{
		MoveToPoint(CoverPoint);
	}
	else
	{
		// nowhere to hide, so shoot from where we stand
		// 숨을 데가 없으니 서 있는 자리에서 쓴다
		SetState(ECQBAIState::Suppress);
	}
}

void AEnemyAIController::OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	bCoverPointPending = false;

	// the state may have moved on while the query was running
	// 쿼리가 돌아가는 동안 상태가 바뀌었을 수 있습니다
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
	// 주변의 이동 가능 공간을 샘플링해, 플레이어가 트레이스로 닿지 못하는 가장 가까운 지점을 골라냅니다
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
		// 후보 지점의 가슴 높이
		const FVector TestPoint = Candidate.Location + FVector(0.0f, 0.0f, 80.0f);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBCoverTrace), false);
		Params.AddIgnoredActor(MyPawn);
		Params.AddIgnoredActor(CurrentTarget);

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, PlayerEye, TestPoint, ECC_Visibility, Params);

		// no blocker means the spot is exposed
		// 막히는 게 없다면 그 자리는 노출된 것입니다
		if (!bBlocked)
		{
			continue;
		}

		// prefer cover that is close by, so the enemy does not run across the room
		// 가까운 엄폐를 선호합니다. 그래야 적이 방을 가로질러 뛰지 않습니다
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
