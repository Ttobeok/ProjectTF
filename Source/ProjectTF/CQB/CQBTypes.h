// CQB Sample - shared enums and helpers.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CQBTypes.generated.h"

/** Who shoots at whom */
UENUM(BlueprintType)
enum class ECQBFaction : uint8
{
	Neutral	UMETA(DisplayName = "Neutral"),
	Player	UMETA(DisplayName = "Player"),
	Ally	UMETA(DisplayName = "Ally"),
	Enemy	UMETA(DisplayName = "Enemy")
};

/**
 *  States handled by the AI state machines.
 *  Idle through Suppress are the combat states, shared by both sides.
 *  Follow through Clear are orders only the player's squad takes.
 */
UENUM(BlueprintType)
enum class ECQBAIState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Investigate	UMETA(DisplayName = "Investigate"),
	Engage		UMETA(DisplayName = "Engage"),
	Cover		UMETA(DisplayName = "Cover"),
	Flank		UMETA(DisplayName = "Flank"),
	Suppress	UMETA(DisplayName = "Suppress"),

	Follow		UMETA(DisplayName = "Follow"),
	Hold		UMETA(DisplayName = "Hold"),
	Stack		UMETA(DisplayName = "Stack"),
	Clear		UMETA(DisplayName = "Clear")
};

/** Combat role handed out by the SquadManager */
UENUM(BlueprintType)
enum class ESquadRole : uint8
{
	None		UMETA(DisplayName = "None"),
	Suppressor	UMETA(DisplayName = "Suppressor"),
	FlankerLeft	UMETA(DisplayName = "Flanker Left"),
	FlankerRight UMETA(DisplayName = "Flanker Right")
};

/** Radio callouts broadcast to the squad */
UENUM(BlueprintType)
enum class ECalloutType : uint8
{
	Contact			UMETA(DisplayName = "Contact"),
	Suppressing		UMETA(DisplayName = "Suppressing"),
	FlankingLeft	UMETA(DisplayName = "Flanking Left"),
	FlankingRight	UMETA(DisplayName = "Flanking Right"),
	LostVisual		UMETA(DisplayName = "Lost Visual"),
	ManDown			UMETA(DisplayName = "Man Down"),

	InPosition		UMETA(DisplayName = "In Position"),
	RoomClear		UMETA(DisplayName = "Room Clear"),
	Moving			UMETA(DisplayName = "Moving"),
	Holding			UMETA(DisplayName = "Holding")
};

/** Which side of a doorway a squad member stacks on */
UENUM(BlueprintType)
enum class EStackSide : uint8
{
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right")
};

/**
 *  Implemented by anything that can be shot at, so the weapon and the AI can tell friend
 *  from foe without knowing the concrete class.
 */
UINTERFACE(MinimalAPI)
class UCQBFactionAgent : public UInterface
{
	GENERATED_BODY()
};

class PROJECTTF_API ICQBFactionAgent
{
	GENERATED_BODY()

public:

	virtual ECQBFaction GetFaction() const = 0;
};

/** Static string and faction helpers. Kept in one place so debug draw and callouts stay in sync. */
struct FCQBNames
{
	static FString StateToString(ECQBAIState State)
	{
		switch (State)
		{
		case ECQBAIState::Idle:			return TEXT("Idle");
		case ECQBAIState::Investigate:	return TEXT("Investigate");
		case ECQBAIState::Engage:		return TEXT("Engage");
		case ECQBAIState::Cover:		return TEXT("Cover");
		case ECQBAIState::Flank:		return TEXT("Flank");
		case ECQBAIState::Suppress:		return TEXT("Suppress");
		case ECQBAIState::Follow:		return TEXT("Follow");
		case ECQBAIState::Hold:			return TEXT("Hold");
		case ECQBAIState::Stack:		return TEXT("Stack");
		case ECQBAIState::Clear:		return TEXT("Clear");
		}
		return TEXT("Unknown");
	}

	static FString RoleToString(ESquadRole Role)
	{
		switch (Role)
		{
		case ESquadRole::Suppressor:	return TEXT("Suppressor");
		case ESquadRole::FlankerLeft:	return TEXT("Flanker(L)");
		case ESquadRole::FlankerRight:	return TEXT("Flanker(R)");
		default:						return TEXT("None");
		}
	}

	/** Spoken line for a callout, e.g. "Flanking left!" */
	static FString CalloutToString(ECalloutType Callout)
	{
		switch (Callout)
		{
		case ECalloutType::Contact:			return TEXT("Contact!");
		case ECalloutType::Suppressing:		return TEXT("Suppressing!");
		case ECalloutType::FlankingLeft:	return TEXT("Flanking left!");
		case ECalloutType::FlankingRight:	return TEXT("Flanking right!");
		case ECalloutType::LostVisual:		return TEXT("Lost visual");
		case ECalloutType::ManDown:			return TEXT("Man down!");
		case ECalloutType::InPosition:		return TEXT("In position");
		case ECalloutType::RoomClear:		return TEXT("Room clear!");
		case ECalloutType::Moving:			return TEXT("Moving!");
		case ECalloutType::Holding:			return TEXT("Holding");
		}
		return TEXT("...");
	}

	static FString FactionToString(ECQBFaction Faction)
	{
		switch (Faction)
		{
		case ECQBFaction::Player:	return TEXT("Player");
		case ECQBFaction::Ally:		return TEXT("Ally");
		case ECQBFaction::Enemy:	return TEXT("Enemy");
		default:					return TEXT("Neutral");
		}
	}
};

/** Faction lookups that work on any actor */
struct PROJECTTF_API FCQBFactions
{
	/** Faction of an actor, or Neutral when it does not declare one */
	static ECQBFaction GetFaction(const AActor* Actor);

	/** True when the two factions shoot at each other. Neutral fights nobody. */
	static bool AreHostile(ECQBFaction A, ECQBFaction B);

	/** Convenience: are these two actors on opposing sides */
	static bool AreHostile(const AActor* A, const AActor* B);
};
