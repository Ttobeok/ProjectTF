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

	// The navmesh covers the whole bounds volume and is generated at runtime; there are no
	// navigation invokers involved, and bGenerateNavigationOnlyAroundNavigationInvokers is off.
	//
	// The level ships without baked navigation data, because a commandlet cannot produce any -
	// it holds the navigation build lock. So on the first BeginPlay the octree exists and the
	// bounds are registered, but nothing has asked for a build. If the spawner cannot project
	// its own position onto the navmesh, that is what has happened: drop the initial build lock,
	// re-announce every bounds volume and ask for the build. It takes about a second, and move
	// orders issued in the meantime are retried by the AI controller until the tiles arrive.
	//
	// navmesh는 bounds 볼륨 전체를 덮으며 런타임에 생성됩니다. 내비게이션 인보커는 관여하지
	// 않고, bGenerateNavigationOnlyAroundNavigationInvokers도 꺼져 있습니다.
	//
	// 레벨에는 구워진 내비게이션 데이터가 없습니다. 커맨드릿이 내비게이션 빌드 잠금을 쥐고
	// 있어서 만들 수가 없기 때문입니다. 그래서 첫 BeginPlay 시점에는 옥트리와 bounds는 있지만
	// 아무도 빌드를 요청하지 않은 상태입니다. 스포너가 자기 위치를 navmesh에 투영하지 못하면
	// 그 상황이라는 뜻이니, 초기 빌드 잠금을 풀고 bounds 볼륨을 다시 알린 뒤 빌드를 요청합니다.
	// 약 1초 걸리며, 그 사이에 나간 이동 명령은 AI 컨트롤러가 재시도합니다.
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
