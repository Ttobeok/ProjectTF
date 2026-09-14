// CQB Sample - faction lookups.

#include "CQBTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

ECQBFaction FCQBFactions::GetFaction(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return ECQBFaction::Neutral;
	}

	if (const ICQBFactionAgent* Agent = Cast<const ICQBFactionAgent>(Actor))
	{
		return Agent->GetFaction();
	}

	// a controller can answer for its pawn and the other way round
	if (const APawn* AsPawn = Cast<const APawn>(Actor))
	{
		if (const ICQBFactionAgent* ControllerAgent = Cast<const ICQBFactionAgent>(AsPawn->GetController()))
		{
			return ControllerAgent->GetFaction();
		}
	}

	return ECQBFaction::Neutral;
}

bool FCQBFactions::AreHostile(ECQBFaction A, ECQBFaction B)
{
	if (A == ECQBFaction::Neutral || B == ECQBFaction::Neutral)
	{
		return false;
	}

	// the player and their squad are one side, everyone else is the other
	const bool bAFriendly = (A == ECQBFaction::Player || A == ECQBFaction::Ally);
	const bool bBFriendly = (B == ECQBFaction::Player || B == ECQBFaction::Ally);

	return bAFriendly != bBFriendly;
}

bool FCQBFactions::AreHostile(const AActor* A, const AActor* B)
{
	return AreHostile(GetFaction(A), GetFaction(B));
}
