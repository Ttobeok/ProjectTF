// CQB Sample - what this brain says to the rest of its side, and what it does when shouted at.

#include "EnemyAIController.h"
#include "CQBCharacter.h"
#include "SquadManager.h"
#include "WeaponComponent.h"
#include "HealthComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "ProjectTF.h"

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


//~ Compliance -----------------------------------------------------------------

float AEnemyAIController::EvaluateCompliance(const AActor* Challenger) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !Challenger)
	{
		return 0.0f;
	}

	float Pressure = 0.0f;

	// being hurt is the biggest single reason to stop
	if (const UHealthComponent* Health = GetHealth())
	{
		const float Percent = Health->GetHealthPercent();
		if (Percent < ComplianceHealthThreshold)
		{
			Pressure += (ComplianceHealthThreshold - Percent) / ComplianceHealthThreshold;
		}
	}

	// someone shouting in your face is more convincing than someone across the building
	const float Distance = FVector::Dist(MyPawn->GetActorLocation(), Challenger->GetActorLocation());
	if (Distance < ComplianceRange)
	{
		Pressure += ComplianceProximityWeight * (1.0f - Distance / ComplianceRange);
	}

	// being alone is worse than having the squad around
	int32 StandingMates = 0;
	for (TActorIterator<ACQBCharacter> It(GetWorld()); It; ++It)
	{
		const ACQBCharacter* Other = *It;
		if (Other && Other != MyPawn && !Other->IsDead() && !Other->IsSurrendered()
			&& FCQBFactions::GetFaction(Other) == Faction)
		{
			++StandingMates;
		}
	}

	if (StandingMates == 0)
	{
		Pressure += ComplianceIsolationWeight;
	}

	// caught out of cover with a weapon pointed at you
	if (!bHasLineOfSight)
	{
		Pressure -= ComplianceBlindPenalty;
	}

	return FMath::Max(0.0f, Pressure);
}

bool AEnemyAIController::ReceiveChallenge(AActor* Challenger, float Pressure)
{
	if (CurrentState == ECQBAIState::Surrender || !GetPawn())
	{
		return false;
	}

	const float Total = EvaluateCompliance(Challenger) + Pressure;

	ASquadManager* Squad = ASquadManager::GetSquadManager(this);

	if (Total < ComplianceThreshold)
	{
		// refused, and now it knows where the shouting came from
		if (Challenger)
		{
			CurrentTarget = Challenger;
			LastStimulusLocation = Challenger->GetActorLocation();

			if (CurrentState == ECQBAIState::Idle)
			{
				SetState(ECQBAIState::Investigate);
			}
		}

		if (Squad)
		{
			Squad->Broadcast(ECalloutType::Defiant, this);
		}

		UE_LOG(LogProjectTF, Log, TEXT("CQB: %s refused the challenge (%.2f of %.2f)"),
			*DisplayName, Total, ComplianceThreshold);
		return false;
	}

	SetState(ECQBAIState::Surrender);

	if (Squad)
	{
		Squad->Broadcast(ECalloutType::Surrendering, this);
	}

	UE_LOG(LogProjectTF, Log, TEXT("CQB: %s surrendered (%.2f of %.2f)"),
		*DisplayName, Total, ComplianceThreshold);
	return true;
}
