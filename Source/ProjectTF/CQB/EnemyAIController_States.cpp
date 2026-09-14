// CQB Sample - the states themselves. One Enter/Update/Exit trio each, dispatched from the
// switches in EnemyAIController.cpp.

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

//~ Surrender ------------------------------------------------------------------

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
	if (SquadRole != ESquadRole::None)
	{
		if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
		{
			Squad->NotifyEnemyDied(this);
		}
	}
}

void AEnemyAIController::UpdateSurrender(float DeltaTime)
{
	// kneeling, and staying that way
}
