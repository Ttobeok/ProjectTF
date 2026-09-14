// CQB Sample - the squad member brain. Takes orders, fights with the shared combat states.

#include "AllyAIController.h"
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
	int32 Taken = 0;
	for (TActorIterator<AAllyAIController> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && *It != this && It->GetPawn())
		{
			++Taken;
		}
	}

	// even numbers go Red, odd go Blue, so a four member squad splits two and two
	Element = (Taken % 2 == 0) ? ESquadElement::Red : ESquadElement::Blue;

	SetDisplayName(FString::Printf(TEXT("%s_%d"),
		Element == ESquadElement::Red ? TEXT("RED") : TEXT("BLU"), (Taken / 2) + 1));

	// squad members start at the player's shoulder
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
		const int32 Key = 31000 + (GetUniqueID() % 16);
		GEngine->AddOnScreenDebugMessage(Key, 3.0f, FColor::Cyan, Line);
	}
}

//~ Orders ---------------------------------------------------------------------

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
	const bool bOnOrder = (CurrentState == ECQBAIState::Follow || CurrentState == ECQBAIState::Hold
		|| CurrentState == ECQBAIState::Stack || CurrentState == ECQBAIState::Clear
		|| CurrentState == ECQBAIState::Watch);

	if (bOnOrder)
	{
		// Clear does its own fighting: walking into the room is the point of the order
		if (bHasLineOfSight && CurrentState != ECQBAIState::Clear)
		{
			SetState(ECQBAIState::Engage);
		}

		return;
	}

	// combat is over, so go back to the last order the player gave
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
	if (!bHasGoal || FVector::Dist2D(Goal, CurrentGoal) > FollowRepathDistance)
	{
		MoveToPoint(Goal);
	}
}

//~ Hold -----------------------------------------------------------------------

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
}

//~ Stack ----------------------------------------------------------------------

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
	if (OrderedDoorway)
	{
		SetFocalPoint(OrderedDoorway->GetClearPoint(), EAIFocusPriority::Gameplay);
	}

	Say(ECalloutType::InPosition);
}

//~ Clear ----------------------------------------------------------------------

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
	RoomQuietTime += DeltaTime;

	if (RoomQuietTime >= RoomClearTime)
	{
		Say(ECalloutType::RoomClear);
		OrderFollow();
	}
}


//~ Watch ----------------------------------------------------------------------

void AAllyAIController::EnterWatch()
{
	StopMovement();
	bHasGoal = false;
	SetFiring(false);
	SetFacePlayerMode(true);

	// eyes on the point the player named, and stay there
	SetFocalPoint(WatchPoint, EAIFocusPriority::Gameplay);

	OrderMarkerPoint = WatchPoint;
	OrderMarkerTime = OrderMarkerDuration;
}

void AAllyAIController::UpdateWatch(float DeltaTime)
{
	// standing and watching. Contact drops into the inherited combat states.
}

//~ Order marker ----------------------------------------------------------------

void AAllyAIController::DrawOrderMarker(float DeltaTime)
{
	if (OrderMarkerTime <= 0.0f)
	{
		return;
	}

	OrderMarkerTime -= DeltaTime;

	// a ring on the floor where the order sent this member, so the player can see it land
	const FColor Colour = (Element == ESquadElement::Red) ? FColor(230, 60, 60) : FColor(60, 120, 230);
	DrawDebugCircle(GetWorld(), OrderMarkerPoint + FVector(0.0f, 0.0f, 4.0f), 45.0f, 24, Colour,
		false, DeltaTime, 0, 3.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);
}
