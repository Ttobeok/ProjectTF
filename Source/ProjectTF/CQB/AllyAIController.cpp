// CQB Sample - the squad member brain. Takes orders, fights with the shared combat states.
// CQB 샘플 - 분대원의 뇌. 명령을 받고, 공유하는 전투 상태로 싸욵니다.

#include "AllyAIController.h"
#include "CQBCharacter.h"
#include "DoorwayMarker.h"
#include "HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "ProjectTF.h"

AAllyAIController::AAllyAIController()
{
	Faction = ECQBFaction::Ally;
}

void AAllyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// The squad manager names the enemies as they register; this squad answers to the player
	// instead, so it numbers itself.
	// 적은 등록할 때 분대 매니저가 이름을 붙입니다. 이 분대는 플레이어에게 속하므로
	// 스스로 번호를 매깁니다.
	int32 Taken = 0;
	for (TActorIterator<AAllyAIController> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && *It != this && It->GetPawn())
		{
			++Taken;
		}
	}

	// even numbers go Red, odd go Blue, so a four member squad splits two and two
	// 짝수는 Red, 홀수는 Blue. 네 명이면 둘씩 나뉘게 됩니다
	SquadIndex = Taken;
	Element = (Taken % 2 == 0) ? ESquadElement::Red : ESquadElement::Blue;

	SetDisplayName(FString::Printf(TEXT("%s_%d"),
		Element == ESquadElement::Red ? TEXT("RED") : TEXT("BLU"), (Taken / 2) + 1));

	// wear the element colour, so the player can tell Red from Blue at a glance in a corridor
	// element 색을 입힙니다. 복도에서 한눈에 Red와 Blue를 구분하기 위해서입니다
	if (ACQBCharacter* Body = Cast<ACQBCharacter>(InPawn))
	{
		Body->SetBodyTint(Element == ESquadElement::Red ? RedTint : BlueTint);

		// hand the identity down to the pawn, which outlives this controller
		// 신원을 폰에 내려줍니다. 폰이 이 컨트롤러보다 오래 삽니다
		Body->CallSign = GetDisplayName();
		Body->Element = Element;
	}

	// squad members start at the player's shoulder
	// 분대원은 플레이어 어깨 근처에서 시작합니다
	OrderFollow();
}

AActor* AAllyAIController::GetLeader() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

FString AAllyAIController::GetOrderName() const
{
	return FCQBNames::StateToString(IsInCombat() ? StandingOrder : GetState());
}

void AAllyAIController::Say(ECalloutType Callout)
{
	const FString Line = FString::Printf(TEXT("[%s] %s"), *GetDisplayName(), *FCQBNames::CalloutToString(Callout));

	UE_LOG(LogProjectTF, Log, TEXT("CQB squad: %s"), *Line);

	if (GEngine)
	{
		// a key per squad member, so two of them do not overwrite each other
		// 분대원마다 키를 따로 씁니다. 둘이 서로를 덮어쓰지 않게 하려는 것입니다
		const int32 Key = 31000 + (GetUniqueID() % 16);
		GEngine->AddOnScreenDebugMessage(Key, 3.0f, FColor::Cyan, Line);
	}
}

//~ Orders ---------------------------------------------------------------------
//~ Orders / 명령 -------------------------------------------------------------

void AAllyAIController::OrderFollow()
{
	OrderedDoorway = nullptr;
	StandingOrder = ECQBAIState::Follow;
	SetState(ECQBAIState::Follow);
	Say(ECalloutType::Moving);
}

void AAllyAIController::OrderHold()
{
	OrderedDoorway = nullptr;
	StandingOrder = ECQBAIState::Hold;
	SetState(ECQBAIState::Hold);
	Say(ECalloutType::Holding);
}

void AAllyAIController::OrderStack(ADoorwayMarker* Doorway, EStackSide Side)
{
	if (!Doorway)
	{
		return;
	}

	OrderedDoorway = Doorway;
	StackSide = Side;
	StandingOrder = ECQBAIState::Stack;
	SetState(ECQBAIState::Stack);
	Say(ECalloutType::Moving);
}

void AAllyAIController::OrderWatch(const FVector& Point)
{
	OrderedDoorway = nullptr;
	WatchPoint = Point;
	StandingOrder = ECQBAIState::Watch;
	SetState(ECQBAIState::Watch);
	Say(ECalloutType::Watching);
}

void AAllyAIController::OrderClear(ADoorwayMarker* Doorway)
{
	if (!Doorway)
	{
		return;
	}

	OrderedDoorway = Doorway;
	StandingOrder = ECQBAIState::Clear;
	SetState(ECQBAIState::Clear);
	Say(ECalloutType::Moving);
}

//~ State dispatch -------------------------------------------------------------
//~ State dispatch / 상태 분기 ----------------------------------------------------

void AAllyAIController::EnterState(ECQBAIState State)
{
	switch (State)
	{
	case ECQBAIState::Follow:	EnterFollow(); break;
	case ECQBAIState::Hold:		EnterHold(); break;
	case ECQBAIState::Stack:	EnterStack(); break;
	case ECQBAIState::Clear:	EnterClear(); break;
	case ECQBAIState::Watch:	EnterWatch(); break;
	default:					Super::EnterState(State); break;
	}
}

void AAllyAIController::UpdateState(ECQBAIState State, float DeltaTime)
{
	switch (State)
	{
	case ECQBAIState::Follow:	UpdateFollow(DeltaTime); break;
	case ECQBAIState::Hold:		UpdateHold(DeltaTime); break;
	case ECQBAIState::Stack:	UpdateStack(DeltaTime); break;
	case ECQBAIState::Clear:	UpdateClear(DeltaTime); break;
	case ECQBAIState::Watch:	UpdateWatch(DeltaTime); break;
	default:					Super::UpdateState(State, DeltaTime); break;
	}
}

void AAllyAIController::ExitState(ECQBAIState State)
{
	switch (State)
	{
	case ECQBAIState::Follow:
	case ECQBAIState::Hold:
	case ECQBAIState::Stack:
	case ECQBAIState::Clear:
	case ECQBAIState::Watch:
		SetFiring(false);
		break;

	default:
		Super::ExitState(State);
		break;
	}
}

void AAllyAIController::OnTargetLost()
{
	SetState(StandingOrder);
}

void AAllyAIController::UpdateGlobalTransitions(float DeltaTime)
{
	DrawOrderMarker(DeltaTime);

	// An order state that spots a hostile drops into the inherited combat states, and the order
	// is remembered so the squad member can pick it back up once the shooting stops.
	// 명령 상태에서 적을 발견하면 물려받은 전투 상태로 떨어집니다. 그 명령을 기억해두어
	// 사격이 멈추면 분대원이 다시 집어들 수 있게 합니다.
	const bool bOnOrder = (CurrentState == ECQBAIState::Follow || CurrentState == ECQBAIState::Hold
		|| CurrentState == ECQBAIState::Stack || CurrentState == ECQBAIState::Clear
		|| CurrentState == ECQBAIState::Watch);

	if (bOnOrder)
	{
		// Clear does its own fighting: walking into the room is the point of the order
		// Clear는 자기 자리에서 싸웁니다. 방으로 들어가는 것 자체가 명령이기 때문입니다
		if (bHasLineOfSight && CurrentState != ECQBAIState::Clear)
		{
			// the order path never reaches Super, which is where this is normally cleared, so a
			// stale value left over from the last disengagement would expire the grace instantly
			// 명령 경로는 Super에 도달하지 않는데 이 값은 거기서 초기화됩니다. 지난번 교전
			// 이탈 때 남은 값을 그대로 두면 유예가 즉시 만료됩니다
			TimeWithoutLineOfSight = 0.0f;
			SetState(ECQBAIState::Engage);
		}

		return;
	}

	// combat is over, so go back to the last order the player gave
	// 교전이 끝났으니 플레이어가 마지막으로 내린 명령으로 돌아갑니다
	if (!bHasLineOfSight && IsInCombat())
	{
		TimeWithoutLineOfSight += DeltaTime;

		if (TimeWithoutLineOfSight >= LoseSightGraceTime)
		{
			SetState(StandingOrder);
		}

		return;
	}

	Super::UpdateGlobalTransitions(DeltaTime);
}

//~ Follow ---------------------------------------------------------------------
//~ Follow / 따라가기 ----------------------------------------------------------

void AAllyAIController::EnterFollow()
{
	SetFiring(false);
	SetFacePlayerMode(false);
	ClearFocus(EAIFocusPriority::Gameplay);
	bAnnouncedArrival = false;
}

void AAllyAIController::UpdateFollow(float DeltaTime)
{
	const AActor* Leader = GetLeader();
	const APawn* MyPawn = GetPawn();
	if (!Leader || !MyPawn)
	{
		return;
	}

	// Stand off behind the player rather than on top of them. Indoors that point is often
	// inside a wall - a player with their back to one puts it straight through - so it is
	// pulled onto the navmesh, and failing that the squad just closes on the player.
	// 플레이어 위에 올라서지 말고 뒤에 떨어져 섭니다. 실내에서 그 지점은 벽 속인 경우가
	// 흔합니다 — 플레이어가 벽을 등지면 항상 그렇습니다 — 그래서 navmesh로 끌어오고,
	// 그것도 실패하면 그냥 플레이어 쪽으로 붙습니다.
	const FVector Desired = Leader->GetActorLocation() - Leader->GetActorForwardVector() * FollowDistance;
	FVector Goal = Leader->GetActorLocation();

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Desired, Projected, FVector(FollowDistance, FollowDistance, 300.0f)))
		{
			Goal = Projected.Location;
		}
	}
	else
	{
		Goal = Desired;
	}

	// only re-path once the player has actually moved somewhere else
	// 플레이어가 실제로 다른 곳으로 움직였을 때만 경로를 다시 잡습니다
	// Re-path when the player has moved, and also when this member stopped short of where it
	// was going. A move that ends against a squad mate in a doorway finishes without arriving;
	// with the player standing still the goal never changes, so nothing asked again and the
	// member stood in the door for the rest of the level.
	//
	// 플레이어가 움직였을 때 재경로를 잡고, 이 분대원이 목적지에 못 미친 채 멈췄을 때도 잡습니다.
	// 문간에서 동료에게 막혀 끝난 이동은 도착하지 않은 채 종료됩니다. 플레이어가 가만히 있으면
	// 목표가 안 바뀌니 아무도 다시 요청하지 않았고, 그 분대원은 레벨이 끝날 때까지 문에 서
	// 있었습니다.
	const bool bLeaderMoved = !bHasGoal || FVector::Dist2D(Goal, CurrentGoal) > FollowRepathDistance;
	const bool bStoppedShort = GetMoveStatus() == EPathFollowingStatus::Idle
		&& FVector::Dist2D(MyPawn->GetActorLocation(), Goal) > FollowRepathDistance;

	if (bLeaderMoved || bStoppedShort)
	{
		MoveToPoint(Goal);
	}
}

//~ Hold -----------------------------------------------------------------------
//~ Hold / 정지 ------------------------------------------------------------

void AAllyAIController::EnterHold()
{
	StopMovement();
	bHasGoal = false;
	SetFiring(false);
	SetFacePlayerMode(false);
	ClearFocus(EAIFocusPriority::Gameplay);
	bAnnouncedArrival = false;
}

void AAllyAIController::UpdateHold(float DeltaTime)
{
	// standing still, watching. UpdateGlobalTransitions takes it into Engage on contact.
	// 가만히 서서 지켜봅니다. 접촉이 생기면 UpdateGlobalTransitions가 Engage로 넘깁니다.
}

//~ Stack ----------------------------------------------------------------------
//~ Stack / 문 옆 대기 -------------------------------------------------------

void AAllyAIController::EnterStack()
{
	SetFiring(false);
	SetFacePlayerMode(false);
	bAnnouncedArrival = false;

	if (OrderedDoorway)
	{
		const FVector Point = OrderedDoorway->GetStackPoint(StackSide);
		MoveToPoint(Point);
		OrderMarkerPoint = Point;
		OrderMarkerTime = OrderMarkerDuration;
	}
}

void AAllyAIController::IssueClearMove()
{
	bClearMoveIssued = true;
	MoveToPoint(ClearPoint);
}

void AAllyAIController::UpdateStack(float DeltaTime)
{
	if (!HasReachedGoal() || bAnnouncedArrival)
	{
		return;
	}

	StopMovement();
	bAnnouncedArrival = true;

	// face the doorway, ready to go through
	// 문을 바라보며 통과할 준비를 합니다
	if (OrderedDoorway)
	{
		SetFocalPoint(OrderedDoorway->GetClearPoint(), EAIFocusPriority::Gameplay);
	}

	Say(ECalloutType::InPosition);
}

//~ Clear ----------------------------------------------------------------------
//~ Clear / 방 소타 ---------------------------------------------------------

void AAllyAIController::EnterClear()
{
	SetFiring(false);
	SetFacePlayerMode(false);
	ClearFocus(EAIFocusPriority::Gameplay);

	RoomQuietTime = 0.0f;
	bAnnouncedArrival = false;
	bClearMoveIssued = false;
	ClearRetries = 0;

	if (!OrderedDoorway)
	{
		return;
	}

	// Each member takes its own slot, pulled onto the navmesh: a slot that lands inside cover or
	// against a wall falls back to the room's centre point rather than failing the move.
	// 각자 자기 슬롯으로 가되 navmesh 위로 끌어옵니다. 슬롯이 엄폐물 속이나 벽에 걸리면 이동을
	// 실패시키는 대신 방 중앙 지점으로 대체합니다.
	ClearPoint = OrderedDoorway->GetClearPoint(SquadIndex);

	FNavLocation Projected;
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Nav && Nav->ProjectPointToNavigation(ClearPoint, Projected, FVector(60.0f, 60.0f, 200.0f)))
	{
		ClearPoint = Projected.Location;
	}
	else
	{
		ClearPoint = OrderedDoorway->GetClearPoint();
	}

	OrderMarkerPoint = ClearPoint;
	OrderMarkerTime = OrderMarkerDuration;

	// wait our turn rather than all hitting the doorway at once
	// 한꺼번에 문으로 몰리지 않도록 자기 차례를 기다립니다
	ClearStartDelay = ClearEntryInterval * static_cast<float>(SquadIndex);

	if (ClearStartDelay <= 0.0f)
	{
		IssueClearMove();
	}
	else
	{
		StopMovement();
	}
}

void AAllyAIController::UpdateClear(float DeltaTime)
{
	// a hostile in the room is the whole reason for going in
	// 방안의 적이무말로 들어가는 이유입니다
	if (!bClearMoveIssued)
	{
		ClearStartDelay -= DeltaTime;
		if (ClearStartDelay > 0.0f)
		{
			return;
		}

		IssueClearMove();
	}

	// Fight on the move. Clearing a room means going in, so a member who sees a suspect keeps
	// walking to its slot and shoots while it does, rather than dropping into Engage.
	//
	// Engage looks for cover, and at the moment of contact the only cover is behind: the
	// member turned round in the door frame to walk back out, into the squad mates coming in
	// behind it, and the whole squad stopped in the doorway. The guide always said Clear does
	// its own fighting; the code did not.
	//
	// 이동하면서 싸웁니다. 방을 소타한다는 건 들어간다는 뜻이므로, 용의자를 본 분대원은
	// Engage로 떨어지지 않고 자기 슬롯으로 계속 걸어가며 사격합니다.
	//
	// Engage는 엄폐물을 찾는데, 접촉 순간 엄폐물은 뒤쪽에만 있습니다. 그래서 분대원이 문틀에서
	// 몸을 돌려 도로 나가려다, 뒤따라 들어오던 동료들과 부딪혀 분대 전체가 문간에 멈췄습니다.
	// 가이드는 처음부터 "Clear는 자기 자리에서 싸운다"고 했는데 코드는 그렇지 않았습니다.
	const bool bContact = bHasLineOfSight && IsValid(CurrentTarget);

	if (bContact)
	{
		RoomQuietTime = 0.0f;
		SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
		SetFiring(true);
	}
	else
	{
		SetFiring(false);
	}

	if (!HasReachedGoal())
	{
		return;
	}

	// HasReachedGoal also returns true when path following gave up, which is what happens when a
	// member is wedged against another pawn - the navmesh routes through characters as if they
	// were not there. Treating that as arrival had members standing in the previous room calling
	// "Room clear!" about a room they never entered. Check where the member actually is.
	//
	// HasReachedGoal은 경로 추종이 포기해도 true를 돌려주는데, 분대원이 다른 폰에 끼면 바로 그렇게
	// 됩니다. navmesh는 캐릭터가 없는 것처럼 그 사이로 경로를 짜기 때문입니다. 이걸 도착으로 치니
	// 분대원이 이전 방에 선 채로, 들어가 보지도 않은 방에 대해 "Room clear!"를 외쳤습니다. 실제
	// 위치를 확인합니다.
	const APawn* MyPawn = GetPawn();
	const bool bInRoom = MyPawn
		&& FVector::Dist2D(MyPawn->GetActorLocation(), ClearPoint) <= ClearArrivalTolerance;

	if (!bInRoom)
	{
		if (ClearRetries < MaxClearRetries)
		{
			++ClearRetries;
			IssueClearMove();
			return;
		}

		// could not get in; go back to the player quietly rather than report a room it never saw
		// 들어가지 못했습니다. 보지도 못한 방을 보고하는 대신 조용히 플레이어에게 돌아갑니다
		SetFiring(false);
		OrderFollow();
		return;
	}

	StopMovement();

	if (bContact)
	{
		// arrived with someone still in the room: hold the slot and keep shooting
		// 방에 아직 누가 있는데 도착했습니다. 슬롯을 지키며 계속 쏩니다
		return;
	}

	// nothing in here for a few seconds running, so call it
	// 몇 초간 연속으로 아무것도 없으니 선언합니다
	RoomQuietTime += DeltaTime;

	if (RoomQuietTime >= RoomClearTime)
	{
		Say(ECalloutType::RoomClear);
		OrderFollow();
	}
}


//~ Watch ----------------------------------------------------------------------
//~ Watch / 주시 ------------------------------------------------------------

void AAllyAIController::EnterWatch()
{
	StopMovement();
	bHasGoal = false;
	SetFiring(false);
	SetFacePlayerMode(true);

	// eyes on the point the player named, and stay there
	// 플레이어가 지정한 지점에 시선을 두고 그 자리를 지킵니다
	SetFocalPoint(WatchPoint, EAIFocusPriority::Gameplay);

	OrderMarkerPoint = WatchPoint;
	OrderMarkerTime = OrderMarkerDuration;
}

void AAllyAIController::UpdateWatch(float DeltaTime)
{
	// standing and watching. Contact drops into the inherited combat states.
	// 서서 지켜봅니다. 접촉이 생기면 물려받은 전투 상태로 떨어집니다.
}

//~ Order marker ----------------------------------------------------------------
//~ Order marker / 명령 마커 ----------------------------------------------------

void AAllyAIController::DrawOrderMarker(float DeltaTime)
{
	if (OrderMarkerTime <= 0.0f)
	{
		return;
	}

	OrderMarkerTime -= DeltaTime;

	// a ring on the floor where the order sent this member, so the player can see it land
	// 명령이 이 분대원을 보낸 자리에 바닥 링을 그립니다. 명령이 먹힌 것을 보여주려는 것입니다
	const FColor Colour = (Element == ESquadElement::Red) ? FColor(230, 60, 60) : FColor(60, 120, 230);
	DrawDebugCircle(GetWorld(), OrderMarkerPoint + FVector(0.0f, 0.0f, 4.0f), 45.0f, 24, Colour,
		false, DeltaTime, 0, 3.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);
}
