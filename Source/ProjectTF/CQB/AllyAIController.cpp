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
	Element = (Taken % 2 == 0) ? ESquadElement::Red : ESquadElement::Blue;

	SetDisplayName(FString::Printf(TEXT("%s_%d"),
		Element == ESquadElement::Red ? TEXT("RED") : TEXT("BLU"), (Taken / 2) + 1));

	// wear the element colour, so the player can tell Red from Blue at a glance in a corridor
	// element 색을 입힙니다. 복도에서 한눈에 Red와 Blue를 구분하기 위해서입니다
	if (ACQBCharacter* Body = Cast<ACQBCharacter>(InPawn))
	{
		Body->SetBodyTint(Element == ESquadElement::Red ? RedTint : BlueTint);
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
	if (!bHasGoal || FVector::Dist2D(Goal, CurrentGoal) > FollowRepathDistance)
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

	if (OrderedDoorway)
	{
		const FVector Point = OrderedDoorway->GetClearPoint();
		MoveToPoint(Point);
		OrderMarkerPoint = Point;
		OrderMarkerTime = OrderMarkerDuration;
	}
}

void AAllyAIController::UpdateClear(float DeltaTime)
{
	// a hostile in the room is the whole reason for going in
	// 방안의 적이무말로 들어가는 이유입니다
	if (bHasLineOfSight && IsValid(CurrentTarget))
	{
		RoomQuietTime = 0.0f;
		SetState(ECQBAIState::Engage);
		return;
	}

	if (!HasReachedGoal())
	{
		return;
	}

	StopMovement();

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
