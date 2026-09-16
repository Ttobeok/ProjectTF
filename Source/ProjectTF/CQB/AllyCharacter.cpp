// CQB Sample - a squad member the player commands.
// CQB 샘플 - 플레이어가 지휘하는 분대원.

#include "AllyCharacter.h"
#include "AllyAIController.h"

AAllyCharacter::AAllyCharacter()
{
	Faction = ECQBFaction::Ally;

	AIControllerClass = AAllyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
