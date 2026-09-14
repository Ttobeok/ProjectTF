// CQB Sample - hand written AI state machine. No Behavior Tree involved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CQBTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UEnvQuery;
class UWeaponComponent;
class UHealthComponent;
struct FAIStimulus;
struct FEnvQueryResult;

/**
 *  Enemy brain.
 *
 *  Perception feeds a plain C++ state machine: every state has Enter / Update / Exit,
 *  Tick runs the Update of the current state and SetState() drives transitions.
 *  Squad level decisions (who suppresses, who flanks, where to flank) come from ASquadManager.
 */
UCLASS()
class PROJECTTF_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:

	AEnemyAIController();

	virtual void Tick(float DeltaTime) override;

	/** Current state of the machine */
	UFUNCTION(BlueprintPure, Category = "AI")
	EEnemyState GetState() const { return CurrentState; }

	/** Runs Exit on the old state and Enter on the new one. Re-entering the same state is ignored. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetState(EEnemyState NewState);

	/** Role handed out by the squad manager */
	ESquadRole GetSquadRole() const { return SquadRole; }
	void SetSquadRole(ESquadRole NewRole) { SquadRole = NewRole; }

	/** Name used by the debug draw and the callouts, e.g. Enemy_1 */
	const FString& GetDisplayName() const { return DisplayName; }
	void SetDisplayName(const FString& InName) { DisplayName = InName; }

	/** Last position this enemy actually saw the player at */
	FVector GetLastKnownPlayerLocation() const { return LastKnownPlayerLocation; }

	/** The player this enemy is tracking, if any */
	AActor* GetPlayerTarget() const { return PlayerTarget; }

	/** True while this enemy is fighting rather than idling or searching */
	bool IsInCombat() const;

	/** A squad mate said something. Idle enemies move to investigate a contact. */
	void OnCalloutReceived(ECalloutType Callout, AEnemyAIController* From);

	/** The squad dropped every role, so ask for a new one */
	void OnSquadRolesInvalidated();

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void BeginPlay() override;

	//~ State machine

	/** Dispatches to the per state Enter function */
	void EnterState(EEnemyState State);

	/** Dispatches to the per state Update function */
	void UpdateState(EEnemyState State, float DeltaTime);

	/** Dispatches to the per state Exit function */
	void ExitState(EEnemyState State);

	void EnterIdle();
	void UpdateIdle(float DeltaTime);
	void ExitIdle();

	void EnterInvestigate();
	void UpdateInvestigate(float DeltaTime);
	void ExitInvestigate();

	void EnterEngage();
	void UpdateEngage(float DeltaTime);
	void ExitEngage();

	void EnterCover();
	void UpdateCover(float DeltaTime);
	void ExitCover();

	void EnterFlank();
	void UpdateFlank(float DeltaTime);
	void ExitFlank();

	void EnterSuppress();
	void UpdateSuppress(float DeltaTime);
	void ExitSuppress();

	/** Transitions that apply no matter which state is running, such as losing the player */
	void UpdateGlobalTransitions(float DeltaTime);

	//~ Perception

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** True when the actor is the pawn of a player controller */
	bool IsPlayerActor(const AActor* Actor) const;

	/** Refreshes bHasLineOfSight and the last known location */
	void UpdateSenses(float DeltaTime);

	//~ Helpers

	/** Starts or stops the weapon trigger */
	void SetFiring(bool bFire);

	/** Faces the player while fighting, faces the movement direction otherwise */
	void SetFacePlayerMode(bool bFacePlayer);

	/** Issues a move order and remembers the goal for the arrival test */
	void MoveToPoint(const FVector& Goal);

	/** True once the pawn is close enough to the goal, or the path following gave up */
	bool HasReachedGoal(float Tolerance = 140.0f) const;

	/** Runs the cover EQS when one is assigned, otherwise samples the NavMesh in C++ */
	void FindCoverPoint();

	/** EQS result handler */
	void OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	/** NavMesh sampling used when no EQS asset is assigned: points the player cannot trace to */
	bool FindCoverPointFallback(FVector& OutPoint) const;

	/** Weapon of the possessed pawn */
	UWeaponComponent* GetWeapon() const;

	/** Health of the possessed pawn */
	UHealthComponent* GetHealth() const;

	/** Pawn death hook: calls out Man down, leaves the squad and destroys the controller */
	UFUNCTION()
	void OnPawnDied(AActor* DeadActor, AActor* Killer);

	/** Draws the state name over the pawn's head */
	void DrawStateDebug(float DeltaTime) const;

	//~ Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UAIPerceptionComponent* AIPerception;

	UPROPERTY()
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY()
	UAISenseConfig_Hearing* HearingConfig;

	//~ Tuning

	/** Sight radius in cm */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightRadius = 2000.0f;

	/** Half angle of the vision cone in degrees */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightAngle = 70.0f;

	/** How far gunshots are heard, in cm */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float HearingRange = 3000.0f;

	/** Seconds without line of sight before the enemy goes back to searching */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float LoseSightGraceTime = 3.0f;

	/** Seconds spent looking around an investigation point before giving up */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float InvestigateGiveUpTime = 3.0f;

	/** Minimum time spent shooting in Engage before taking up a role */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float EngageMinDuration = 1.2f;

	/** Seconds of suppressing fire per burst */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float SuppressFireDuration = 2.0f;

	/** Seconds of pause between suppressing bursts */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float SuppressRestDuration = 1.0f;

	/** Radius searched for a cover position, in cm */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float CoverSearchRadius = 800.0f;

	/** Enemies refuse cover positions closer than this to the player, in cm */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float MinCoverDistanceFromPlayer = 350.0f;

	/**
	 *  Optional EQS query used to pick a cover point.
	 *  Leave empty to use the built in NavMesh search; assign a query to use EQS instead.
	 *  Use UEnvQueryContext_CQBPlayer as the query context for the player.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	TObjectPtr<UEnvQuery> CoverQuery;

	/** Draw the state name over the pawn */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
	bool bDrawStateDebug = true;

	//~ Runtime state

	EEnemyState CurrentState = EEnemyState::Idle;

	ESquadRole SquadRole = ESquadRole::None;

	FString DisplayName = TEXT("Enemy");

	/** The player pawn, once perceived */
	UPROPERTY()
	TObjectPtr<AActor> PlayerTarget;

	FVector LastKnownPlayerLocation = FVector::ZeroVector;

	/** Where the last sight or hearing stimulus came from */
	FVector LastStimulusLocation = FVector::ZeroVector;

	/** Current move goal, used for the arrival test */
	FVector CurrentGoal = FVector::ZeroVector;

	bool bHasGoal = false;
	bool bHasLineOfSight = false;
	bool bHasSeenPlayerOnce = false;
	bool bFlankCompleted = false;
	bool bCoverPointPending = false;
	bool bSuppressBurstActive = true;

	float TimeWithoutLineOfSight = 0.0f;
	float TimeInState = 0.0f;
	float InvestigateWaitTime = 0.0f;
	float SuppressPhaseTime = 0.0f;
};
