// CQB Sample - what this brain says to the rest of its side, and what it does when shouted at.
// CQB 샘플 - 이 뇌가 같은 편에게 무엇을 말하고, 외침을 들으면 어떻게 하는가.

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
//~ Squad hooks / 분대 훅 ------------------------------------------------------

void AEnemyAIController::OnCalloutReceived(ECalloutType Callout, AEnemyAIController* From)
{
	if (From == this || !From)
	{
		return;
	}

	// a squad mate spotting the player pulls idle enemies towards the contact
	// 분대원이 플레이어를 발견하면 대기 중인 적들이 그 접촉 지점으로 끌려갑니다
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
		// 이미 교전 중입니다. 그 자리에서 새 역할만 받아들입니다
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
	// 분대를 떠나기 전에 알립니다. 그래야 나머지가 Man down을 듣고 새 역할을 받습니다
	if (ASquadManager* Squad = ASquadManager::GetSquadManager(this))
	{
		Squad->NotifyEnemyDied(this);
	}

	UnPossess();
	Destroy();
}


//~ Compliance -----------------------------------------------------------------
//~ Compliance / 순응 ---------------------------------------------------------

float AEnemyAIController::EvaluateCompliance(const AActor* Challenger) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !Challenger)
	{
		return 0.0f;
	}

	float Pressure = 0.0f;

	// being hurt is the biggest single reason to stop
	// 다친 것이 그만둡 단일 이유로는 가장 큽니다
	if (const UHealthComponent* Health = GetHealth())
	{
		const float Percent = Health->GetHealthPercent();
		if (Percent < ComplianceHealthThreshold)
		{
			Pressure += (ComplianceHealthThreshold - Percent) / ComplianceHealthThreshold;
		}
	}

	// someone shouting in your face is more convincing than someone across the building
	// 코앉에서 외치는 사람이 건물 건너편에서 외치는 사람보다 설득력 있습니다
	const float Distance = FVector::Dist(MyPawn->GetActorLocation(), Challenger->GetActorLocation());
	if (Distance < ComplianceRange)
	{
		Pressure += ComplianceProximityWeight * (1.0f - Distance / ComplianceRange);
	}

	// being alone is worse than having the squad around
	// 혼자인 것은 분대가 주변에 있는 것보다 나쁜 상황입니다
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
	// 엄폐 밖에서 총구를 들이밀린 상황
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
		// 거부했고, 이제 그 외침이 어디서 왔는지를 압니다
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
