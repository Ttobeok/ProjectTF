// CQB Sample - shared enums and helpers.

#pragma once

#include "CoreMinimal.h"
#include "CQBTypes.generated.h"

/** States handled by the enemy AI state machine */
UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Investigate	UMETA(DisplayName = "Investigate"),
	Engage		UMETA(DisplayName = "Engage"),
	Cover		UMETA(DisplayName = "Cover"),
	Flank		UMETA(DisplayName = "Flank"),
	Suppress	UMETA(DisplayName = "Suppress")
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
	ManDown			UMETA(DisplayName = "Man Down")
};

/** Static string helpers. Kept in one place so debug draw and callouts stay in sync. */
struct FCQBNames
{
	static FString StateToString(EEnemyState State)
	{
		switch (State)
		{
		case EEnemyState::Idle:			return TEXT("Idle");
		case EEnemyState::Investigate:	return TEXT("Investigate");
		case EEnemyState::Engage:		return TEXT("Engage");
		case EEnemyState::Cover:		return TEXT("Cover");
		case EEnemyState::Flank:		return TEXT("Flank");
		case EEnemyState::Suppress:		return TEXT("Suppress");
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
		}
		return TEXT("...");
	}
};
