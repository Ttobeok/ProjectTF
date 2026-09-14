// CQB Sample - drops a group of enemies into the level.

#include "EnemySpawner.h"
#include "EnemyCharacter.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "ProjectTF.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	RootComponent = ArrowComponent;
	ArrowComponent->ArrowColor = FColor::Red;
	ArrowComponent->ArrowSize = 2.0f;

	EnemyClass = AEnemyCharacter::StaticClass();

	// three enemies holding a room, spread out a little
	SpawnOffsets.Add(FVector(0.0f, 0.0f, 0.0f));
	SpawnOffsets.Add(FVector(-250.0f, 300.0f, 0.0f));
	SpawnOffsets.Add(FVector(-250.0f, -300.0f, 0.0f));
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnEnemies();
	}
}

void AEnemySpawner::SpawnEnemies()
{
	UWorld* World = GetWorld();
	if (!World || !EnemyClass)
	{
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	for (const FVector& Offset : SpawnOffsets)
	{
		FVector SpawnLocation = GetActorTransform().TransformPosition(Offset);

		// snap to the NavMesh so the enemy starts on walkable ground
		if (NavSys)
		{
			FNavLocation Projected;
			if (NavSys->ProjectPointToNavigation(SpawnLocation, Projected, FVector(500.0f, 500.0f, 500.0f)))
			{
				SpawnLocation = Projected.Location;
			}
		}

		// lift by half the capsule height so the capsule is not buried in the floor
		SpawnLocation.Z += 96.0f;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnParams.Owner = this;

		const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);

		if (AEnemyCharacter* Enemy = World->SpawnActor<AEnemyCharacter>(EnemyClass, SpawnTransform, SpawnParams))
		{
			SpawnedEnemies.Add(Enemy);
		}
	}

	UE_LOG(LogProjectTF, Log, TEXT("CQB: spawned %d enemies"), SpawnedEnemies.Num());
}
