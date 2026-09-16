// CQB Sample - faction lookups.
// CQB 샘플 - 진영 판별.

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

	AActor* Mutable = const_cast<AActor*>(Actor);

	if (const ICQBFactionAgent* Agent = Cast<ICQBFactionAgent>(Mutable))
	{
		return Agent->GetFaction();
	}

	// a controller can answer for its pawn and the other way round
	// 컨트롤러가 자기 폰을 대신해 답할 수 있고, 그 반대도 마찬가지입니다
	if (APawn* AsPawn = Cast<APawn>(Mutable))
	{
		if (const ICQBFactionAgent* ControllerAgent = Cast<ICQBFactionAgent>(AsPawn->GetController()))
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
	// 플레이어와 그 분대가 한 편, 나머지가 반대편입니다
	const bool bAFriendly = (A == ECQBFaction::Player || A == ECQBFaction::Ally);
	const bool bBFriendly = (B == ECQBFaction::Player || B == ECQBFaction::Ally);

	return bAFriendly != bBFriendly;
}

bool FCQBFactions::AreHostile(const AActor* A, const AActor* B)
{
	return AreHostile(GetFaction(A), GetFaction(B));
}
