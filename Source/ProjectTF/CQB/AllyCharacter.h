// CQB Sample - a squad member the player commands.

#pragma once

#include "CoreMinimal.h"
#include "EnemyCharacter.h"
#include "AllyCharacter.generated.h"

/**
 *  Same pawn setup as the AI enemies: the health, weapon and visual components are identical,
 *  and so is the combat behaviour. Only the faction and the controller differ, which is what
 *  turns the shared state machine from a threat into a squad member.
 */
UCLASS()
class PROJECTTF_API AAllyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:

	AAllyCharacter();
};
