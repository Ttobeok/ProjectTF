// CQB Sample - every debug hook, in one place and out of the gameplay classes.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CQBTypes.h"
#include "CQBDebugDirector.generated.h"

class ACQBCharacter;
class ADoorwayMarker;

/**
 *  Drives the sample from the command line so it can be exercised without a keyboard.
 *
 *  It spawns itself when the level loads and does nothing unless asked. Everything here is for
 *  testing and for recording the verification logs in the guide; the gameplay classes carry none
 *  of it, which is the point of keeping it in its own actor.
 *
 *    -CQBKillEnemyAfter=<s>   hit suspects after a delay
 *    -CQBKillCount=<n>        how many of them, default 1
 *    -CQBKillDamage=<f>       fraction of max health, default lethal
 *    -CQBOrderAfter=<s>       issue a squad order after a delay
 *    -CQBOrder=<name>         follow | hold | stack | clear | watch | challenge
 *    -CQBOrderDoor=<n>        which doorway, ordered west to east
 *    -CQBScreenshotAfter=<s>  take a screenshot after a delay
 *    -CQBPlayerAt=X,Y,Z       put the player there on the first tick, so a test can be run from
 *                             a spot the level does not start at without moving the PlayerStart
 */
UCLASS()
class PROJECTTF_API ACQBDebugDirector : public AActor
{
	GENERATED_BODY()

public:

	ACQBDebugDirector();

	/** Spawns the director if the level has none. Called on BeginPlay by the spawner. */
	static void EnsureExists(const UObject* WorldContextObject);

protected:

	virtual void BeginPlay() override;

	/** Reads the command line and arms whichever hooks were asked for */
	void ArmFromCommandLine();

	/** Moves the player to the spot named on the command line, if one was */
	void PlacePlayer();

	void RunDamage();
	void RunOrder();
	void RunScreenshot();

	/** Suspects, nearest the player first */
	TArray<ACQBCharacter*> GatherSuspects() const;

	/** Doorways in a stable order, west to east */
	TArray<ADoorwayMarker*> GatherDoorways() const;

	FTimerHandle PlaceTimer;
	FTimerHandle DamageTimer;
	FTimerHandle OrderTimer;
	FTimerHandle ScreenshotTimer;

	int32 DamageCount = 1;
	float DamageFraction = 10.0f;

	FString OrderName;
	int32 OrderDoorIndex = 0;
};
