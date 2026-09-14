// CQB Sample - drops a group of enemies into the level.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AEnemyCharacter;
class UArrowComponent;

/**
 *  Place one of these in the room the enemies should defend.
 *  It spawns one enemy per entry in SpawnOffsets on BeginPlay, each offset being relative
 *  to this actor and projected onto the NavMesh.
 */
UCLASS()
class PROJECTTF_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:

	AEnemySpawner();

	/** Spawns the enemies immediately. Called from BeginPlay when bSpawnOnBeginPlay is set. */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SpawnEnemies();

	/** Class to spawn. Defaults to AEnemyCharacter, so no Blueprint is required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemyCharacter> EnemyClass;

	/** One enemy per entry, relative to this actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TArray<FVector> SpawnOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	bool bSpawnOnBeginPlay = true;

protected:

	virtual void BeginPlay() override;

	/** Editor only arrow so the spawner can be found in the viewport */
	UPROPERTY(VisibleAnywhere, Category = "Spawner")
	UArrowComponent* ArrowComponent;

	/** Enemies spawned by this actor */
	UPROPERTY()
	TArray<TObjectPtr<AEnemyCharacter>> SpawnedEnemies;
};
