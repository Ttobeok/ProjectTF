// CQB Sample - every debug hook, in one place and out of the gameplay classes.
// CQB 샘플 - 모든 디버그 훅을 한곳에, 게임플레이 클래스 바깥에.

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
 *    -CQBKillSide=<side>      enemy (default) or ally, for testing the down state on the HUD
 *    -CQBOrderAfter=<s>       issue a squad order after a delay
 *    -CQBOrder=<name>         follow | hold | stack | clear | watch | challenge
 *    -CQBOrderDoor=<n>        which doorway, ordered west to east
 *    -CQBScreenshotAfter=<s>  take a screenshot after a delay
 *    -CQBTraceSquad=<s>       log every squad member's position, state and path every <s> seconds
 *    -CQBPlayerAt=X,Y,Z       put the player there on the first tick, so a test can be run from
 *                             a spot the level does not start at without moving the PlayerStart
 *
 *  키보드 없이도 샘플을 돌려볼 수 있도록 커맨드라인에서 구동합니다.
 *
 *  레벨이 로드되면 스스로 스폰되고, 요청받지 않으면 아무것도 하지 않습니다. 여기 있는 것은
 *  전부 테스트용이며 가이드의 검증 로그를 남기기 위한 것입니다. 게임플레이 클래스는 이런 것을
 *  하나도 들고 있지 않고, 그게 이 액터를 따로 둔 이유입니다.
 *
 *    -CQBKillEnemyAfter=<초>  N초 뒤 용의자 타격
 *    -CQBKillCount=<수>       몇 명. 기본 1
 *    -CQBKillDamage=<비율>    최대 체력 대비. 기본은 즉사
 *    -CQBKillSide=<진영>      enemy(기본) 또는 ally. HUD의 전사 표시 검증용
 *    -CQBOrderAfter=<초>      N초 뒤 분대 명령
 *    -CQBOrder=<이름>         follow | hold | stack | clear | watch | challenge
 *    -CQBOrderDoor=<n>        어느 문인지. 서에서 동 순서
 *    -CQBScreenshotAfter=<초> N초 뒤 스크린샷
 *    -CQBPlayerAt=X,Y,Z       첫 틱에 플레이어를 그 자리로. PlayerStart를 건드리지 않고
 *                             레벨 시작 지점이 아닌 곳에서 테스트할 수 있습니다
 */
UCLASS()
class PROJECTTF_API ACQBDebugDirector : public AActor
{
	GENERATED_BODY()

public:

	ACQBDebugDirector();

	/**
	 *  Spawns the director if the level has none. Called on BeginPlay by the spawner.
	 *  레벨에 없으면 디렉터를 스폰합니다. 스포너가 BeginPlay에서 부릅니다.
	 */
	static void EnsureExists(const UObject* WorldContextObject);

protected:

	virtual void BeginPlay() override;

	/**
	 *  Reads the command line and arms whichever hooks were asked for
	 *  커맨드라인을 읽고 요청된 훅만 장전합니다
	 */
	void ArmFromCommandLine();

	/**
	 *  Moves the player to the spot named on the command line, if one was
	 *  커맨드라인에 지정된 자리가 있으면 플레이어를 그리로 옮깁니다
	 */
	void PlacePlayer();

	void RunDamage();
	void RunOrder();
	void RunScreenshot();

	/**
	 *  Logs where each squad member is, what it is doing, and the path it is on
	 *  각 분대원의 위치, 상태, 따라가는 경로를 로그로 남깁니다
	 */
	void RunTraceSquad();

	/**
	 *  Suspects, nearest the player first
	 *  용의자들. 플레이어에게 가까운 순서
	 */
	TArray<ACQBCharacter*> GatherSuspects() const;

	/**
	 *  Doorways in a stable order, west to east
	 *  문들을 일정한 순서로. 서에서 동으로
	 */
	TArray<ADoorwayMarker*> GatherDoorways() const;

	FTimerHandle PlaceTimer;
	FTimerHandle DamageTimer;
	FTimerHandle OrderTimer;
	FTimerHandle ScreenshotTimer;
	FTimerHandle TraceSquadTimer;

	int32 DamageCount = 1;
	float DamageFraction = 10.0f;

	FString OrderName;

	/**
	 *  Which side the damage hook hits. Enemy unless the command line says otherwise.
	 *  타격 훅이 어느 편을 때릴지. 커맨드라인이 달리 말하지 않으면 적입니다.
	 */
	ECQBFaction DamageSide = ECQBFaction::Enemy;
	int32 OrderDoorIndex = 0;
};
