// CQB Sample - drops a group of enemies into the level.

#include "EnemySpawner.h"
#include "EnemyCharacter.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "EngineUtils.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "GameFramework/WorldSettings.h"
#include "ProjectTF.h"
#include "HealthComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
}

void AEnemySpawner::DebugKillOneEnemy()
{
	for (const TObjectPtr<AEnemyCharacter>& Enemy : SpawnedEnemies)
	{
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		if (UHealthComponent* Health = Enemy->GetHealthComponent())
		{
			UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: killing %s"), *Enemy->GetName());
			Health->TakeDamage(Health->MaxHealth * 10.0f, this, nullptr);
			return;
		}
	}
}

void AEnemySpawner::EnsureNavigationBuilt()
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB nav: no navigation system, the AI will not move"));
		return;
	}

	// A level that was opened and saved in the editor without running Build Paths carries a
	// RecastNavMesh actor holding no tiles. The navigation system treats that as valid data and
	// skips generation, so every AI move request fails and the enemies stand still. Detect that
	// by projecting a point we know is on the floor, and kick off a build when it comes back empty.
	FNavLocation Projected;
	if (NavSys->ProjectPointToNavigation(GetActorLocation(), Projected, FVector(300.0f, 300.0f, 500.0f)))
	{
		return;
	}

	UE_LOG(LogProjectTF, Warning, TEXT("CQB nav: no navmesh under the spawner, rebuilding at runtime"));

	if (const AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings())
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB nav: world settings config=%s"),
			*GetNameSafe(WorldSettings->GetNavigationSystemConfig()));
	}

	for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
	{
		const UBrushComponent* BrushComp = It->GetBrushComponent();
		UE_LOG(LogProjectTF, Warning, TEXT("CQB nav: volume %s brush=%s polys=%d"),
			*It->GetName(),
			BrushComp && BrushComp->Brush ? TEXT("yes") : TEXT("NONE"),
			BrushComp && BrushComp->Brush && BrushComp->Brush->Polys ? BrushComp->Brush->Polys->Element.Num() : -1);
	}

	// Report what the navmesh is set to. Static generation builds no generator at all, so
	// RebuildAll and Build() silently do nothing; the level must not ship an empty Static navmesh.
	for (const ANavigationData* NavData : NavSys->NavDataSet)
	{
		if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
		{
			UE_LOG(LogProjectTF, Warning, TEXT("CQB nav: %s RuntimeGeneration=%d"),
				*Recast->GetName(), static_cast<int32>(Recast->GetRuntimeGenerationMode()));
		}
	}

	// Navigation building starts out locked, and a locked system drops every build request on
	// the floor without touching a single tile. Clear the locks before asking for the build.
	NavSys->RemoveNavigationBuildLock(ENavigationBuildLock::InitialLock);
	NavSys->RemoveNavigationBuildLock(ENavigationBuildLock::NoUpdateInEditor);
	NavSys->RemoveNavigationBuildLock(ENavigationBuildLock::NoUpdateInPIE);
	NavSys->RemoveNavigationBuildLock(ENavigationBuildLock::Custom);
	UNavigationSystemV1::SetNavigationAutoUpdateEnabled(true, NavSys);

	// The navmesh is spawned during world init, which can happen before the bounds volumes
	// register, leaving the tile generator holding an empty set of bounds. Re-announcing the
	// volumes refreshes it and marks the tiles dirty so the build below has something to do.
	for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
	{
		NavSys->OnNavigationBoundsUpdated(*It);
	}

	NavSys->Build();
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

