// CQB Sample - drops a group of enemies into the level.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CQBSpawner.generated.h"

class ACQBCharacter;
class UArrowComponent;

/**
 *  Place one of these in the room the enemies should defend.
 *  It spawns one enemy per entry in SpawnOffsets on BeginPlay, each offset being relative
 *  to this actor and projected onto the NavMesh.
 */
UCLASS()
class PROJECTTF_API ACQBSpawner : public AActor
{
	GENERATED_BODY()

public:

	ACQBSpawner();

	/** Spawns the whole group immediately. Called from BeginPlay when bSpawnOnBeginPlay is set. */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SpawnCharacters();

	/**
	 *  Who to spawn. The level places two of these actors: one left on the default for the enemies
	 *  in room B, one set to AAllyCharacter for the player's squad. Nothing about the spawner
	 *  cares which side it is filling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<ACQBCharacter> CharacterClass;

	/** One character per entry, relative to this actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TArray<FVector> SpawnOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	bool bSpawnOnBeginPlay = true;

protected:

	virtual void BeginPlay() override;

	/** Rebuilds the navmesh at startup when the level carries an empty one */
	void EnsureNavigationBuilt();

	/** Reports whether the navmesh turned up, a few seconds after the level loads */
	void ReportNavigationState();

	FTimerHandle NavReportTimer;

	FTimerHandle DebugKillTimerHandle;
	FTimerHandle DebugScreenshotTimerHandle;

	/** Editor only arrow so the spawner can be found in the viewport */
	UPROPERTY(VisibleAnywhere, Category = "Spawner")
	UArrowComponent* ArrowComponent;

	/** Enemies spawned by this actor */
	UPROPERTY()
	TArray<TObjectPtr<ACQBCharacter>> SpawnedCharacters;
};
