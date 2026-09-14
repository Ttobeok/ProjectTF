// CQB Sample - the squad member brain. Takes orders, fights with the shared combat states.

#pragma once

#include "CoreMinimal.h"
#include "EnemyAIController.h"
#include "AllyAIController.generated.h"

class ADoorwayMarker;

/**
 *  A squad member under the player's command.
 *
 *  Everything about fighting - perception, Engage, Cover, Suppress, firing - is inherited from
 *  AEnemyAIController. Only the faction differs, so the same state machine that hunts the player
 *  hunts the enemies instead. On top of that sit four orders: Follow, Hold, Stack and Clear.
 */
UCLASS()
class PROJECTTF_API AAllyAIController : public AEnemyAIController
{
	GENERATED_BODY()

public:

	AAllyAIController();

	/** Falls in behind the player */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderFollow();

	/** Stops where it stands and covers */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderHold();

	/** Takes up a position beside the given doorway */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderStack(ADoorwayMarker* Doorway, EStackSide Side);

	/** Goes through the doorway and sweeps the room beyond */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderClear(ADoorwayMarker* Doorway);

	/** The order this squad member is currently carrying out, for the HUD */
	UFUNCTION(BlueprintPure, Category = "Squad Order")
	FString GetOrderName() const;

protected:

	virtual void OnPossess(APawn* InPawn) override;

	/** The player's squad is commanded directly, so it does not join the enemy squad manager */
	virtual bool ShouldJoinSquad() const override { return false; }

	//~ The four orders, layered on top of the inherited combat states
	virtual void EnterState(ECQBAIState State) override;
	virtual void UpdateState(ECQBAIState State, float DeltaTime) override;
	virtual void ExitState(ECQBAIState State) override;
	virtual void UpdateGlobalTransitions(float DeltaTime) override;

	void EnterFollow();
	void UpdateFollow(float DeltaTime);

	void EnterHold();
	void UpdateHold(float DeltaTime);

	void EnterStack();
	void UpdateStack(float DeltaTime);

	void EnterClear();
	void UpdateClear(float DeltaTime);

	/** Says something and prints it where the player can read it */
	void Say(ECalloutType Callout);

	/** The player pawn this squad member follows */
	AActor* GetLeader() const;

	/** How far behind the player to settle */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float FollowDistance = 300.0f;

	/** Only re-issue a follow move once the player has walked this far from the last goal */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float FollowRepathDistance = 150.0f;

	/** Seconds without a hostile in the room before calling it clear */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float RoomClearTime = 3.0f;

	/** Doorway the current order refers to */
	UPROPERTY(Transient)
	TObjectPtr<ADoorwayMarker> OrderedDoorway;

	/** Side of the doorway assigned by the player's order */
	EStackSide StackSide = EStackSide::Left;

	/** The order standing before combat interrupted it, returned to once the fight ends */
	ECQBAIState StandingOrder = ECQBAIState::Follow;

	/** Seconds the room has looked empty */
	float RoomQuietTime = 0.0f;

	/** True once the arrival callout has been said, so it is not repeated every frame */
	bool bAnnouncedArrival = false;
};
