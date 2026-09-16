// CQB Sample - squad level coordination for the enemy AI.
// CQB 샘플 - 적 AI의 분대 단위 조율.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CQBTypes.h"
#include "SquadManager.generated.h"

class AEnemyAIController;

/**
 *  One per world. Hands out combat roles, computes flanking positions and relays callouts.
 *  Enemies fetch it with GetSquadManager(); the first caller spawns it if the level has none,
 *  so nothing needs to be placed by hand.
 *
 *  월드당 하나입니다. 전투 역할을 배정하고, 우회 지점을 계산하며, 콜아웃을 중계합니다.
 *  적들은 GetSquadManager()로 가져오고, 레벨에 없으면 첫 호출자가 스포합니다.
 *  그래서 손으로 배치할 것은 없습니다.
 */
UCLASS()
class PROJECTTF_API ASquadManager : public AActor
{
	GENERATED_BODY()

public:

	ASquadManager();

	/**
	 *  Returns the squad manager for this world, spawning one the first time it is asked for.
	 *  이 월드의 분대 매니저를 돌려줍니다. 처음 요청될 때 없으면 하나 스포합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Squad", meta = (WorldContext = "WorldContextObject"))
	static ASquadManager* GetSquadManager(const UObject* WorldContextObject);

	/**
	 *  Adds an enemy to the squad and gives it its display name (Enemy_1, Enemy_2, ...)
	 *  적을 분대에 등록하고 표시 이름을 줍니다 (Enemy_1, Enemy_2, ...)
	 */
	void RegisterEnemy(AEnemyAIController* Enemy);

	/**
	 *  Removes an enemy from the squad
	 *  적을 분대에서 제거합니다
	 */
	void UnregisterEnemy(AEnemyAIController* Enemy);

	/**
	 *  Assigns a combat role to an enemy entering Engage.
	 *  First enemy in the fight suppresses, the second flanks, the third suppresses again,
	 *  and flankers alternate between the left and right side.
	 *
	 *  Engage에 들어온 적에게 전투 역할을 배정합니다.
	 *  첫 번째는 제압, 두 번째는 우회, 세 번째는 다시 제압이고,
	 *  우회는 좌우를 번갈아 맡습니다.
	 */
	ESquadRole RequestRole(AEnemyAIController* Enemy);

	/**
	 *  Side point next to the player for this enemy's flanking role, projected onto the NavMesh
	 *  이 적의 우회 역할에 맞는 플레이어 측면 지점. NavMesh에 투영됩니다
	 */
	FVector GetFlankPoint(AEnemyAIController* Enemy, AActor* Player) const;

	/**
	 *  Prints the callout on screen and relays it to every other enemy in the squad
	 *  콜아웃을 화면에 출력하고, 분대의 다른 모든 적에게 중계합니다
	 */
	void Broadcast(ECalloutType Callout, AEnemyAIController* Enemy);

	/** Called when a squad member dies. Frees its role and reshuffles the survivors. */
	void NotifyEnemyDied(AEnemyAIController* Enemy);

	/** Enemies currently registered */
	UFUNCTION(BlueprintPure, Category = "Squad")
	int32 GetSquadSize() const { return Enemies.Num(); }

	/** Distance from the player at which flank points are generated */
	UPROPERTY(EditAnywhere, Category = "Squad")
	float FlankDistance = 600.0f;

	/** Seconds a callout stays on screen */
	UPROPERTY(EditAnywhere, Category = "Squad")
	float CalloutDisplayTime = 3.0f;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Drops every role and sends enemies still fighting back through Engage to get a new one */
	void ReassignRoles();

	/** Everyone in the squad, alive or dying */
	UPROPERTY()
	TArray<TObjectPtr<AEnemyAIController>> Enemies;

	/** Running counter used to name enemies as they register */
	int32 NextEnemyIndex = 1;

	/** Flip flops so consecutive flankers go around opposite sides */
	bool bNextFlankLeft = true;

	/** Rolling key so callouts stack instead of overwriting each other on screen */
	int32 CalloutMessageKey = 20000;
};
