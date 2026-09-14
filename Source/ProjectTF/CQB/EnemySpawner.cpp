// CQB Sample - drops a group of enemies into the level.

#include "EnemySpawner.h"
#include "EnemyCharacter.h"
#include "HealthComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Kismet/GameplayStatics.h"
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

	EnsureNavigationBuilt();

	if (bSpawnOnBeginPlay)
	{
		SpawnEnemies();
	}

	// Debug hook for watching the Man down callout and the role reassignment without a
	// controller in hand:  ProjectTF.exe -CQBKillEnemyAfter=12
	float KillAfter = 0.0f;
	FParse::Value(FCommandLine::Get(), TEXT("CQBKillEnemyAfter="), KillAfter);

	if (KillAfter > 0.0f)
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: killing one enemy in %.1fs"), KillAfter);
		GetWorld()->GetTimerManager().SetTimer(DebugKillTimerHandle, this, &AEnemySpawner::DebugKillOneEnemy, KillAfter, false);
	}

	// Screenshot hook, for checking the crosshair and the weapons without sitting at the machine:
	//   ProjectTF.exe -CQBScreenshotAfter=5
	float ScreenshotAfter = 0.0f;
	FParse::Value(FCommandLine::Get(), TEXT("CQBScreenshotAfter="), ScreenshotAfter);

	if (ScreenshotAfter > 0.0f)
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: screenshot in %.1fs"), ScreenshotAfter);
		GetWorld()->GetTimerManager().SetTimer(DebugScreenshotTimerHandle, this, &AEnemySpawner::DebugTakeScreenshot, ScreenshotAfter, false);
	}
}

void AEnemySpawner::DebugTakeScreenshot()
{
	UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: taking a screenshot"));
	FScreenshotRequest::RequestScreenshot(false);
}

void AEnemySpawner::EnsureNavigationBuilt()
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

	GetWorld()->GetTimerManager().SetTimer(NavReportTimer, this, &AEnemySpawner::ReportNavigationState, 4.0f, false);
}

void AEnemySpawner::ReportNavigationState()
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

void AEnemySpawner::DebugKillOneEnemy()
{
	// -CQBKillCount=N takes out several at once, for testing how isolation affects compliance.
	// -CQBKillDamage=<fraction of max health> wounds instead of killing, which is the other
	// half of what makes a suspect give up.
	int32 KillCount = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CQBKillCount="), KillCount);

	float DamageFraction = 10.0f;
	FParse::Value(FCommandLine::Get(), TEXT("CQBKillDamage="), DamageFraction);

	// the ally spawner shares this class, and killing a squad member is not what the flag asks for
	if (!SpawnedEnemies.IsEmpty() && IsValid(SpawnedEnemies[0])
		&& SpawnedEnemies[0]->GetFaction() != ECQBFaction::Enemy)
	{
		return;
	}

	for (const TObjectPtr<AEnemyCharacter>& Enemy : SpawnedEnemies)
	{
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		if (UHealthComponent* Health = Enemy->GetHealthComponent())
		{
			UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: hitting %s for %.0f%% of health"),
				*Enemy->GetName(), DamageFraction * 100.0f);
			Health->TakeDamage(Health->MaxHealth * DamageFraction, this, nullptr);

			if (--KillCount <= 0)
			{
				return;
			}
		}
	}

	UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: no living enemy to kill"));
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
