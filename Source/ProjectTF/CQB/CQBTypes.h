// CQB Sample - shared enums and helpers.
// CQB 샘플 - 공용 enum과 헬퍼.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CQBTypes.generated.h"

/**
 *  Who shoots at whom
 *  누가 누구를 쏘는가
 */
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
 *
 *  AI 상태머신이 다루는 상태들입니다.
 *  Idle~Suppress는 전투 상태로 양 진영이 공유합니다.
 *  Follow~Clear는 플레이어 분대만 받는 명령입니다.
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
	Clear		UMETA(DisplayName = "Clear"),
	Watch		UMETA(DisplayName = "Watch"),

	/**
	 *  Gave up. Weapon down, hands up, no longer a threat to anyone.
	 *  포기. 무기를 내리고 손을 들며, 더 이상 누구에게도 위협이 아닙니다.
	 */
	Surrender	UMETA(DisplayName = "Surrender")
};

/**
 *  Which half of the squad an order is aimed at
 *  명령이 분대의 어느 절반을 향하는가
 */
UENUM(BlueprintType)
enum class ESquadElement : uint8
{
	Red		UMETA(DisplayName = "Red"),
	Blue	UMETA(DisplayName = "Blue"),
	All		UMETA(DisplayName = "Gold")
};

/**
 *  Combat role handed out by the SquadManager
 *  SquadManager가 배정하는 전투 역할
 */
UENUM(BlueprintType)
enum class ESquadRole : uint8
{
	None		UMETA(DisplayName = "None"),
	Suppressor	UMETA(DisplayName = "Suppressor"),
	FlankerLeft	UMETA(DisplayName = "Flanker Left"),
	FlankerRight UMETA(DisplayName = "Flanker Right")
};

/**
 *  Radio callouts broadcast to the squad
 *  분대 전체에 뿌려지는 무전 콜아웃
 */
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
	Holding			UMETA(DisplayName = "Holding"),
	Watching		UMETA(DisplayName = "Watching"),

	/** Shouted at a suspect / 용의자에게 외침 */
	Challenge		UMETA(DisplayName = "Challenge"),
	/** A suspect gave up / 용의자가 항복 */
	Surrendering	UMETA(DisplayName = "Surrendering"),
	/** A suspect refused / 용의자가 거부 */
	Defiant			UMETA(DisplayName = "Defiant")
};

/**
 *  Which side of a doorway a squad member stacks on
 *  분대원이 문의 어느 쪽에 붙는가
 */
UENUM(BlueprintType)
enum class EStackSide : uint8
{
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right")
};

/**
 *  Implemented by anything that can be shot at, so the weapon and the AI can tell friend
 *  from foe without knowing the concrete class.
 *
 *  총에 맞을 수 있는 모든 것이 구현합니다. 덕분에 무기와 AI가 구체 클래스를 몰라도
 *  아군과 적을 구분할 수 있습니다.
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

/**
 *  Static string and faction helpers. Kept in one place so debug draw and callouts stay in sync.
 *  문자열·진영 헬퍼 모음. 디버그 표시와 콜아웃이 서로 어긋나지 않도록 한곳에 모아둡니다.
 */
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
		case ECQBAIState::Watch:		return TEXT("Watch");
		case ECQBAIState::Surrender:	return TEXT("Surrender");
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

	/**
	 *  Spoken line for a callout, e.g. "Flanking left!"
	 *  콜아웃으로 실제 내뱉는 대사. 예: "Flanking left!"
	 */
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
		case ECalloutType::Watching:		return TEXT("Watching that");
		case ECalloutType::Challenge:		return TEXT("Drop the weapon!");
		case ECalloutType::Surrendering:	return TEXT("Hands up, I give up!");
		case ECalloutType::Defiant:			return TEXT("Not a chance!");
		}
		return TEXT("...");
	}

	static FString ElementToString(ESquadElement Element)
	{
		switch (Element)
		{
		case ESquadElement::Red:	return TEXT("RED");
		case ESquadElement::Blue:	return TEXT("BLUE");
		default:					return TEXT("GOLD");
		}
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

/**
 *  Faction lookups that work on any actor
 *  아무 액터에나 쓸 수 있는 진영 조회
 */
struct PROJECTTF_API FCQBFactions
{
	/**
	 *  Faction of an actor, or Neutral when it does not declare one
	 *  액터의 진영. 선언하지 않았으면 Neutral
	 */
	static ECQBFaction GetFaction(const AActor* Actor);

	/**
	 *  True when the two factions shoot at each other. Neutral fights nobody.
	 *  두 진영이 서로 쏘는 사이면 true. Neutral은 누구와도 싸우지 않습니다.
	 */
	static bool AreHostile(ECQBFaction A, ECQBFaction B);

	/**
	 *  Convenience: are these two actors on opposing sides
	 *  편의 함수: 이 두 액터가 서로 반대편인가
	 */
	static bool AreHostile(const AActor* A, const AActor* B);
};
