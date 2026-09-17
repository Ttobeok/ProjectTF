// CQB Sample - the squad member brain. Takes orders, fights with the shared combat states.
// CQB 샘플 - 분대원의 뇌. 명령을 받고, 공유하는 전투 상태로 싸욵니다.

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
 *
 *  플레이어의 지휘를 받는 분대원입니다.
 *
 *  싸우는 것과 관련된 전부 — 인지, Engage, Cover, Suppress, 사격 — 는 AEnemyAIController에서
 *  그대로 물려받습니다. 다른 것은 진영뿐이라, 플레이어를 쌓던 그 상태머신이 이번에는
 *  적을 쌓습니다. 그 위에 명령이 얹혀 있습니다: Follow, Hold, Stack, Clear, Watch.
 */
UCLASS()
class PROJECTTF_API AAllyAIController : public AEnemyAIController
{
	GENERATED_BODY()

public:

	AAllyAIController();

	/**
	 *  Falls in behind the player
	 *  플레이어 뒤로 붙습니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderFollow();

	/**
	 *  Stops where it stands and covers
	 *  서 있는 자리에 멈춰 경계합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderHold();

	/**
	 *  Takes up a position beside the given doorway
	 *  지정된 문 옆으로 붙습니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderStack(ADoorwayMarker* Doorway, EStackSide Side);

	/**
	 *  Goes through the doorway and sweeps the room beyond
	 *  문을 통과해 그 너머 방을 소타합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderClear(ADoorwayMarker* Doorway);

	/**
	 *  Holds where it stands and keeps eyes on a point
	 *  제자리에서 지정된 지점을 계속 주시합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Squad Order")
	void OrderWatch(const FVector& Point);

	/**
	 *  Red or Blue. Orders can be given to one element or to the whole squad.
	 *  Red 또는 Blue. 명령은 한 element에만 내리거나 분대 전체에 내릴 수 있습니다.
	 */
	ESquadElement GetElement() const { return Element; }
	void SetElement(ESquadElement InElement) { Element = InElement; }

	/**
	 *  The order this squad member is currently carrying out, for the HUD
	 *  이 분대원이 지금 수행 중인 명령. HUD용입니다
	 */
	UFUNCTION(BlueprintPure, Category = "Squad Order")
	FString GetOrderName() const;

protected:

	virtual void OnPossess(APawn* InPawn) override;

	/**
	 *  The player's squad is commanded directly, so it does not join the enemy squad manager
	 *  플레이어 분대는 직접 지휘받으므로 적 분대 매니저에 들어가지 않습니다
	 */
	virtual bool ShouldJoinSquad() const override { return false; }

	//~ The four orders, layered on top of the inherited combat states
	//~ 명령들. 물려받은 전투 상태 위에 얹혀 있습니다
	virtual void EnterState(ECQBAIState State) override;
	virtual void UpdateState(ECQBAIState State, float DeltaTime) override;
	virtual void ExitState(ECQBAIState State) override;
	virtual void UpdateGlobalTransitions(float DeltaTime) override;

	/**
	 *  A squad member whose target is dead has finished the fight, not the job: it picks the
	 *  player's last order back up rather than standing down into Idle, which is an enemy state
	 *  with no route back to an order.
	 *
	 *  대상이 죽은 분대원은 전투가 끝난 것이지 할 일이 끝난 게 아닙니다. 대기(Idle)로
	 *  물러나는 대신 플레이어의 마지막 명령을 다시 집어듭니다. Idle은 적 쪽 상태라
	 *  거기서 명령으로 돌아올 경로가 없습니다.
	 */
	virtual void OnTargetLost() override;

	void EnterFollow();
	void UpdateFollow(float DeltaTime);

	void EnterHold();
	void UpdateHold(float DeltaTime);

	void EnterStack();
	void UpdateStack(float DeltaTime);

	void EnterClear();
	void UpdateClear(float DeltaTime);

	void EnterWatch();
	void UpdateWatch(float DeltaTime);

	/**
	 *  Draws where the current order sent this squad member, for a few seconds after it lands
	 *  현재 명령이 이 분대원을 어디로 보냈는지를, 명령 직후 몇 초간 그립니다
	 */
	void DrawOrderMarker(float DeltaTime);

	/**
	 *  Says something and prints it where the player can read it
	 *  뭔가 말하고, 플레이어가 읽을 수 있는 곳에 출력합니다
	 */
	void Say(ECalloutType Callout);

	/**
	 *  The player pawn this squad member follows
	 *  이 분대원이 따라다니는 플레이어 폰
	 */
	AActor* GetLeader() const;

	/**
	 *  Body colour for Red element. Kept pale so it reads as a uniform rather than a highlight.
	 *  Red element의 몸 색. 강조 표시가 아니라 제복처럼 읽히도록 연하게 유지합니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	FLinearColor RedTint = FLinearColor(0.85f, 0.32f, 0.30f);

	/**
	 *  Body colour for Blue element
	 *  Blue element의 몸 색
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	FLinearColor BlueTint = FLinearColor(0.34f, 0.46f, 0.88f);

	/**
	 *  How far behind the player to settle
	 *  플레이어 뒤 얼마만큼 떨어져 자리잡을지
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float FollowDistance = 300.0f;

	/**
	 *  Only re-issue a follow move once the player has walked this far from the last goal
	 *  플레이어가 직전 목표에서 이만큼 움직여야 따라가기 명령을 다시 냅니다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float FollowRepathDistance = 150.0f;

	/**
	 *  Seconds without a hostile in the room before calling it clear
	 *  방안에 적이 없는 상태가 이만큼 이어지면 소타 완료로 칩니다(초)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float RoomClearTime = 3.0f;

	/**
	 *  Seconds between squad members starting a Clear, by squad index.
	 *
	 *  The doorways are 2 m wide. Four members setting off together arrived at the same moment,
	 *  and whoever reached the frame first and saw a suspect stopped there - which left the rest
	 *  packed in behind it, unable to move and unable to shoot past it. Staggered, they come
	 *  through the door as a file instead of a crowd.
	 *
	 *  분대원들이 Clear를 시작하는 간격(초). 분대 순번 기준입니다.
	 *
	 *  문 폭은 2m입니다. 네 명이 동시에 출발하면 같은 순간에 도착하고, 먼저 문틀에 닿아 용의자를
	 *  본 한 명이 거기 멈추면 나머지가 뒤에 끼어 움직이지도 쏘지도 못했습니다. 간격을 두면
	 *  무리가 아니라 한 줄로 문을 지나갑니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float ClearEntryInterval = 0.8f;

	/**
	 *  Doorway the current order refers to
	 *  현재 명령이 가리키는 문
	 */
	UPROPERTY(Transient)
	TObjectPtr<ADoorwayMarker> OrderedDoorway;

	/**
	 *  Side of the doorway assigned by the player's order
	 *  플레이어 명령으로 배정받은 문의 좌우
	 */
	EStackSide StackSide = EStackSide::Left;

	/**
	 *  The order standing before combat interrupted it, returned to once the fight ends
	 *  전투가 끊기 전까지 유효하던 명령. 교전이 끝나면 여기로 돌아옵니다
	 */
	ECQBAIState StandingOrder = ECQBAIState::Follow;

	/**
	 *  Seconds the room has looked empty
	 *  방이 비어 보인 시간(초)
	 */
	float RoomQuietTime = 0.0f;

	/**
	 *  Seconds left before this member sets off on its Clear
	 *  이 분대원이 Clear를 출발하기까지 남은 시간(초)
	 */
	float ClearStartDelay = 0.0f;

	/**
	 *  True once the delayed Clear move has actually been issued
	 *  지연된 Clear 이동이 실제로 내려졌으면 true
	 */
	bool bClearMoveIssued = false;

	/**
	 *  The point this member is clearing towards
	 *  이 분대원이 소타하러 가는 지점
	 */
	FVector ClearPoint = FVector::ZeroVector;

	/** Issues the Clear move to ClearPoint / ClearPoint로 Clear 이동을 내립니다 */
	void IssueClearMove();

	/**
	 *  How close to its clear slot a member has to be before it counts as in the room, in cm
	 *  분대원이 방 안에 들어온 것으로 치려면 자기 소타 슬롯에 얼마나 가까워야 하는지(cm)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float ClearArrivalTolerance = 150.0f;

	/**
	 *  How many times a blocked Clear move is re-issued before the member gives the order up
	 *  막힌 Clear 이동을 몇 번 다시 내린 뒤 명령을 포기할지
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	int32 MaxClearRetries = 3;

	/**
	 *  Blocked Clear moves re-issued so far
	 *  지금까지 다시 내린 막힌 Clear 이동 수
	 */
	int32 ClearRetries = 0;

	/**
	 *  This member's place in the squad, 0 to 3. Decides its clear slot.
	 *  분대 안에서의 순번(0~3). 소타 슬롯을 정합니다.
	 */
	int32 SquadIndex = 0;

	/**
	 *  Which half of the squad this member belongs to
	 *  이 분대원이 어느 절반에 속하는지
	 */
	ESquadElement Element = ESquadElement::Red;

	/**
	 *  Point the watch order named
	 *  Watch 명령이 지정한 지점
	 */
	FVector WatchPoint = FVector::ZeroVector;

	/**
	 *  Where the last order sent this member, drawn on the floor while fresh
	 *  마지막 명령이 이 분대원을 보낸 위치. 신선할 동안 바닥에 그려집니다
	 */
	FVector OrderMarkerPoint = FVector::ZeroVector;

	/**
	 *  Seconds of order marker left
	 *  명령 마커가 남은 시간(초)
	 */
	float OrderMarkerTime = 0.0f;

	/**
	 *  How long an order marker stays on the floor
	 *  명령 마커가 바닥에 머무르는 시간
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Squad Order")
	float OrderMarkerDuration = 4.0f;

	/**
	 *  True once the arrival callout has been said, so it is not repeated every frame
	 *  도착 콜아웃을 이미 말했으면 true. 매 프레임 반복되지 않게 막습니다
	 */
	bool bAnnouncedArrival = false;
};
