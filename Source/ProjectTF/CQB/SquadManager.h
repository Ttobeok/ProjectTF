// CQB Sample - squad level coordination for the enemy AI.

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
 */
UCLASS()
class PROJECTTF_API ASquadManager : public AActor
{
	GENERATED_BODY()

public:

	ASquadManager();

	/** Returns the squad manager for this world, spawning one the first time it is asked for. */
	UFUNCTION(BlueprintCallable, Category = "Squad", meta = (WorldContext = "WorldContextObject"))
	static ASquadManager* GetSquadManager(const UObject* WorldContextObject);

	/** Adds an enemy to the squad and gives it its display name (Enemy_1, Enemy_2, ...) */
	void RegisterEnemy(AEnemyAIController* Enemy);

	/** Removes an enemy from the squad */
	void UnregisterEnemy(AEnemyAIController* Enemy);

	/**
	 *  Assigns a combat role to an enemy entering Engage.
	 *  First enemy in the fight suppresses, the second flanks, the third suppresses again,
	 *  and flankers alternate between the left and right side.
	 */
	ESquadRole RequestRole(AEnemyAIController* Enemy);

	/** Side point next to the player for this enemy's flanking role, projected onto the NavMesh */
	FVector GetFlankPoint(AEnemyAIController* Enemy, AActor* Player) const;

	/** Prints the callout on screen and relays it to every other enemy in the squad */
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
