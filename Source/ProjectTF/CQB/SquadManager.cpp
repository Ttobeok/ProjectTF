// CQB Sample - squad level coordination for the enemy AI.

#include "SquadManager.h"
#include "EnemyAIController.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "ProjectTF.h"

ASquadManager::ASquadManager()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

ASquadManager* ASquadManager::GetSquadManager(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	// use the one placed in the level if there is one
	for (TActorIterator<ASquadManager> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	// otherwise make one on demand, so no level setup is required
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	return World->SpawnActor<ASquadManager>(ASquadManager::StaticClass(), FTransform::Identity, SpawnParams);
}

void ASquadManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Enemies.Empty();

	Super::EndPlay(EndPlayReason);
}

void ASquadManager::RegisterEnemy(AEnemyAIController* Enemy)
{
	if (!Enemy || Enemies.Contains(Enemy))
	{
		return;
	}

	Enemies.Add(Enemy);
	Enemy->SetDisplayName(FString::Printf(TEXT("Enemy_%d"), NextEnemyIndex++));
}

void ASquadManager::UnregisterEnemy(AEnemyAIController* Enemy)
{
	Enemies.Remove(Enemy);
	Enemies.RemoveAll([](const TObjectPtr<AEnemyAIController>& Entry) { return !IsValid(Entry); });
}

ESquadRole ASquadManager::RequestRole(AEnemyAIController* Enemy)
{
	if (!Enemy)
	{
		return ESquadRole::None;
	}

	// how many squad mates are already holding a role
	int32 AssignedCount = 0;
	for (const TObjectPtr<AEnemyAIController>& Other : Enemies)
	{
		if (IsValid(Other) && Other != Enemy && Other->GetSquadRole() != ESquadRole::None)
		{
			++AssignedCount;
		}
	}

	ESquadRole NewRole = ESquadRole::Suppressor;

	// first in suppresses, second flanks, third suppresses again, and so on
	if (AssignedCount % 2 == 1)
	{
		NewRole = bNextFlankLeft ? ESquadRole::FlankerLeft : ESquadRole::FlankerRight;
		bNextFlankLeft = !bNextFlankLeft;
	}

	Enemy->SetSquadRole(NewRole);

	return NewRole;
}

FVector ASquadManager::GetFlankPoint(AEnemyAIController* Enemy, AActor* Player) const
{
	if (!Enemy || !Player)
	{
		return FVector::ZeroVector;
	}

	const APawn* EnemyPawn = Enemy->GetPawn();
	const FVector PlayerLocation = Player->GetActorLocation();

	// direction the enemy is coming from, flattened
	FVector PlayerToEnemy = (EnemyPawn ? EnemyPawn->GetActorLocation() : PlayerLocation + Player->GetActorForwardVector()) - PlayerLocation;
	PlayerToEnemy.Z = 0.0f;

	if (!PlayerToEnemy.Normalize())
	{
		PlayerToEnemy = Player->GetActorForwardVector();
	}

	// rotate that direction 90 degrees around the player, to the assigned side
	const float PreferredSide = (Enemy->GetSquadRole() == ESquadRole::FlankerLeft) ? -1.0f : 1.0f;
	const FVector SideDirection = FVector::CrossProduct(FVector::UpVector, PlayerToEnemy).GetSafeNormal();

	const FVector PreferredPoint = PlayerLocation + SideDirection * PreferredSide * FlankDistance;

	// drop it onto the NavMesh so the move order can actually be pathed
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		const FVector ProjectionExtent(500.0f, 500.0f, 500.0f);
		FNavLocation Projected;

		if (NavSys->ProjectPointToNavigation(PreferredPoint, Projected, ProjectionExtent))
		{
			return Projected.Location;
		}

		// indoor maps often only have a route around one side, so take the other one
		const FVector OppositePoint = PlayerLocation - SideDirection * PreferredSide * FlankDistance;
		if (NavSys->ProjectPointToNavigation(OppositePoint, Projected, ProjectionExtent))
		{
			return Projected.Location;
		}

		// neither side is navigable, settle for something near the player
		if (NavSys->GetRandomReachablePointInRadius(PlayerLocation, FlankDistance, Projected))
		{
			return Projected.Location;
		}
	}

	return PreferredPoint;
}

void ASquadManager::Broadcast(ECalloutType Callout, AEnemyAIController* Enemy)
{
	const FString SpeakerName = Enemy ? Enemy->GetDisplayName() : TEXT("Squad");
	const FString Line = FString::Printf(TEXT("[%s] %s"), *SpeakerName, *FCQBNames::CalloutToString(Callout));

	UE_LOG(LogProjectTF, Log, TEXT("CQB callout: %s"), *Line);

	if (GEngine)
	{
		FColor Color = FColor::White;
		switch (Callout)
		{
		case ECalloutType::Contact:			Color = FColor::Orange; break;
		case ECalloutType::Suppressing:		Color = FColor::Red; break;
		case ECalloutType::FlankingLeft:
		case ECalloutType::FlankingRight:	Color = FColor::Magenta; break;
		case ECalloutType::LostVisual:		Color = FColor::Silver; break;
		case ECalloutType::ManDown:			Color = FColor::Green; break;
		}

		// rotate through a handful of keys so several callouts can sit on screen at once
		GEngine->AddOnScreenDebugMessage(CalloutMessageKey, CalloutDisplayTime, Color, Line);
		CalloutMessageKey = 20000 + ((CalloutMessageKey - 20000 + 1) % 8);
	}

	// relay to the rest of the squad
	for (const TObjectPtr<AEnemyAIController>& Other : Enemies)
	{
		if (IsValid(Other) && Other != Enemy)
		{
			Other->OnCalloutReceived(Callout, Enemy);
		}
	}
}

void ASquadManager::NotifyEnemyDied(AEnemyAIController* Enemy)
{
	Broadcast(ECalloutType::ManDown, Enemy);

	UnregisterEnemy(Enemy);

	ReassignRoles();
}

void ASquadManager::ReassignRoles()
{
	bNextFlankLeft = true;

	// clear everything first, so the count each enemy sees while re-requesting is correct
	for (const TObjectPtr<AEnemyAIController>& Other : Enemies)
	{
		if (IsValid(Other))
		{
			Other->SetSquadRole(ESquadRole::None);
		}
	}

	// copy the list: handing out roles can change states, which can touch the squad
	TArray<TObjectPtr<AEnemyAIController>> Survivors = Enemies;
	for (const TObjectPtr<AEnemyAIController>& Other : Survivors)
	{
		if (IsValid(Other))
		{
			Other->OnSquadRolesInvalidated();
		}
	}
}
