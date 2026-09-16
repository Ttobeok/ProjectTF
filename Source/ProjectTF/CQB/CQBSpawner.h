// CQB Sample - drops a group of enemies into the level.
// CQB 샘플 - 레벨에 한 무리를 배치합니다.

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
 *
 *  적이 지켜야 할 방에 하나 배치하세요.
 *  BeginPlay에서 SpawnOffsets 항목당 하나씩 스포하며, 각 오프셋은 이 액터 기준이고
 *  NavMesh에 투영됩니다.
 */
UCLASS()
class PROJECTTF_API ACQBSpawner : public AActor
{
	GENERATED_BODY()

public:

	ACQBSpawner();

	/**
	 *  Spawns the whole group immediately. Called from BeginPlay when bSpawnOnBeginPlay is set.
	 *  무리 전체를 즉시 스포합니다. bSpawnOnBeginPlay가 켜져 있으면 BeginPlay에서 불립니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SpawnCharacters();

	/**
	 *  Who to spawn. The level places two of these actors: one left on the default for the enemies
	 *  in room B, one set to AAllyCharacter for the player's squad. Nothing about the spawner
	 *  cares which side it is filling.
	 *
	 *  누구를 스포할지. 레벨에는 이 액터가 둘 배치돼 있습니다 — 하나는 방 B의 적을 위해
	 *  기본값 그대로, 하나는 플레이어 분대를 위해 AAllyCharacter로. 스포너는 자신이
	 *  어느 편을 채우는지 알지 못합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<ACQBCharacter> CharacterClass;

	/**
	 *  One character per entry, relative to this actor
	 *  항목당 한 명. 이 액터 기준 상대 좌표입니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TArray<FVector> SpawnOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	bool bSpawnOnBeginPlay = true;

protected:

	virtual void BeginPlay() override;

	/**
	 *  Rebuilds the navmesh at startup when the level carries an empty one
	 *  레벨에 빈 navmesh가 들어 있을 때 시작 시 다시 생성합니다
	 */
	void EnsureNavigationBuilt();

	/**
	 *  Reports whether the navmesh turned up, a few seconds after the level loads
	 *  레벨 로드 몇 초 뒤에 navmesh가 생겼는지 보고합니다
	 */
	void ReportNavigationState();

	FTimerHandle NavReportTimer;

	FTimerHandle DebugKillTimerHandle;
	FTimerHandle DebugScreenshotTimerHandle;

	/**
	 *  Editor only arrow so the spawner can be found in the viewport
	 *  뷰포트에서 스포너를 찾기 위한 에디터 전용 화살표
	 */
	UPROPERTY(VisibleAnywhere, Category = "Spawner")
	UArrowComponent* ArrowComponent;

	/**
	 *  Enemies spawned by this actor
	 *  이 액터가 스포한 대상들
	 */
	UPROPERTY()
	TArray<TObjectPtr<ACQBCharacter>> SpawnedCharacters;
};
