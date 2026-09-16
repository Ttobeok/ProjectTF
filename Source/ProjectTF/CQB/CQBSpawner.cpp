// CQB Sample - drops a group of enemies into the level.
// CQB 샘플 - 레벨에 한 무리를 배치합니다.

#include "CQBSpawner.h"
#include "CQBCharacter.h"
#include "EnemyCharacter.h"
#include "HealthComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "CQBDebugDirector.h"
#include "ProjectTF.h"

ACQBSpawner::ACQBSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	RootComponent = ArrowComponent;
	ArrowComponent->ArrowColor = FColor::Red;
	ArrowComponent->ArrowSize = 2.0f;

	CharacterClass = AEnemyCharacter::StaticClass();

	// three enemies holding a room, spread out a little
	// 방 하나를 지키는 적 세 명. 조금씩 벌려 놓습니다
	SpawnOffsets.Add(FVector(0.0f, 0.0f, 0.0f));
	SpawnOffsets.Add(FVector(-250.0f, 300.0f, 0.0f));
	SpawnOffsets.Add(FVector(-250.0f, -300.0f, 0.0f));
}

void ACQBSpawner::BeginPlay()
{
	Super::BeginPlay();

	EnsureNavigationBuilt();

	if (bSpawnOnBeginPlay)
	{
		SpawnCharacters();
	}

	// all the command line driven test hooks live in one actor, not in here
	// 커맨드라인으로 돌리는 테스트 훅은 여기가 아니라 한 액터에 모여 있습니다
	ACQBDebugDirector::EnsureExists(this);
}

void ACQBSpawner::EnsureNavigationBuilt()
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB nav: no navigation system, the AI will not move"));
		return;
	}

	// Tiles are generated around the navigation invokers the characters carry, which takes about
	// a second after the level loads.
	//
	// Do not call Build() here. With invoker driven generation a full rebuild wipes the tiles the
	// invokers have produced and starts over, so asking for one on BeginPlay - before any invoker
	// has had a chance to run - leaves the level with no navmesh at all. Move orders issued in the
	// meantime are retried by the AI controller until the tiles arrive.
	// Invoker driven generation still needs one kick to produce the first tiles: the octree
	// exists, the bounds are registered, but nothing has asked for a build yet.
	FNavLocation Projected;
	if (!NavSys->ProjectPointToNavigation(GetActorLocation(), Projected, FVector(300.0f, 300.0f, 500.0f)))
	{
		NavSys->RemoveNavigationBuildLock(ENavigationBuildLock::InitialLock);

		for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
		{
			NavSys->OnNavigationBoundsUpdated(*It);
		}

		NavSys->Build();
	}

	GetWorld()->GetTimerManager().SetTimer(NavReportTimer, this, &ACQBSpawner::ReportNavigationState, 4.0f, false);
}

void ACQBSpawner::ReportNavigationState()
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return;
	}

	FNavLocation Projected;
	if (!NavSys->ProjectPointToNavigation(GetActorLocation(), Projected, FVector(400.0f, 400.0f, 500.0f)))
	{
		// worth shouting about: without this the AI cannot move anywhere at all
		UE_LOG(LogProjectTF, Warning, TEXT("CQB nav [%s]: still no navmesh here. navdata=%d bounds=%d"),
			*GetName(), NavSys->NavDataSet.Num(), NavSys->GetNavigationBounds().Num());
	}
}


void ACQBSpawner::SpawnCharacters()
{
	UWorld* World = GetWorld();
	if (!World || !CharacterClass)
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

		if (ACQBCharacter* Spawned = World->SpawnActor<ACQBCharacter>(CharacterClass, SpawnTransform, SpawnParams))
		{
			SpawnedCharacters.Add(Spawned);
		}
	}

	const ECQBFaction Side = SpawnedCharacters.Num() > 0 ? SpawnedCharacters[0]->GetFaction() : ECQBFaction::Neutral;
	UE_LOG(LogProjectTF, Log, TEXT("CQB: spawned %d %s"), SpawnedCharacters.Num(), *FCQBNames::FactionToString(Side));
}
