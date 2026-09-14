// CQB Sample - a squad member the player commands.

#include "AllyCharacter.h"
#include "AllyAIController.h"

AAllyCharacter::AAllyCharacter()
{
	Faction = ECQBFaction::Ally;

	AIControllerClass = AAllyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
