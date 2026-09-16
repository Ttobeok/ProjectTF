// CQB Sample - the states themselves. One Enter/Update/Exit trio each, dispatched from the
// switches in EnemyAIController.cpp.
// CQB 샘플 - 상태 본체. 상태마다 Enter/Update/Exit 한 벌이며,
// EnemyAIController.cpp의 switch에서 분기해 옵니다.

#include "EnemyAIController.h"
#include "CQBCharacter.h"
#include "SquadManager.h"
#include "WeaponComponent.h"
#include "HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "ProjectTF.h"

//~ Idle
//~ Idle / 대기

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
	// 시각·청각 자극을 기다립니다. 처리는 인지 콜백이 합니다
}

void AEnemyAIController::ExitIdle()
{
}

//~ Investigate
//~ Investigate / 조사

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
	// 그 자리에 도착해 몇 초간 아무것도 없으면 대기로 돌아갑니다
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
//~ Engage / 교전

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
	// 이 적이 뭔 해야 하는지 분대에 묻습니다
	if (SquadRole == ESquadRole::None)
	{
		if (ASquadManager* Squad = GetSquad())
		{
			SquadRole = Squad->RequestRole(this);
		}
	}
}

void AEnemyAIController::UpdateEngage(float DeltaTime)
{
	// shoot for a moment, then carry out the assigned role
	// 잠시 쓏다가, 배정받은 역할을 수행합니다
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
//~ Cover / 엄폐

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
	// 아직 EQS 결과를 기다리는 중
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
//~ Flank / 측면 우회

void AEnemyAIController::EnterFlank()
{
	SetFiring(false);
	SetFacePlayerMode(true);

	if (CurrentTarget)
	{
		SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
	}

	ASquadManager* Squad = GetSquad();
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
		// 교전당 우회는 한 번. 그 뒤에는 자리를 잡고 쓰당니다
		bFlankCompleted = true;
		SetState(ECQBAIState::Engage);
	}
}

void AEnemyAIController::ExitFlank()
{
}

//~ Suppress
//~ Suppress / 제압

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
	// 사격으로 시작합니다
	bSuppressBurstActive = true;
	SuppressPhaseTime = 0.0f;
	SetFiring(true);

	if (ASquadManager* Squad = GetSquad())
	{
		Squad->Broadcast(ECalloutType::Suppressing, this);
	}
}

void AEnemyAIController::UpdateSuppress(float DeltaTime)
{
	SuppressPhaseTime += DeltaTime;

	// 2 seconds of fire, 1 second of pause
	// 2초 사격, 1초 휴지
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

//~ Surrender ------------------------------------------------------------------
//~ Surrender / 항복 --------------------------------------------------------------

void AEnemyAIController::EnterSurrender()
{
	SetFiring(false);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	SetFacePlayerMode(false);

	bHasGoal = false;

	if (ACQBCharacter* MyCharacter = Cast<ACQBCharacter>(GetPawn()))
	{
		MyCharacter->SetSurrendered(true);
	}

	// a squad member who gave up frees the role they were holding
	// 항복한 분대원은 들고 있던 역할을 내놓습니다
	if (SquadRole != ESquadRole::None)
	{
		if (ASquadManager* Squad = GetSquad())
		{
			Squad->NotifyEnemyDied(this);
		}
	}
}

void AEnemyAIController::UpdateSurrender(float DeltaTime)
{
	// kneeling, and staying that way
	// 무릎을 꿇고, 그 상태로 머물러 있습니다
}
