// CQB Sample - a squad member the player commands.

#pragma once

#include "CoreMinimal.h"
#include "CQBCharacter.h"
#include "AllyCharacter.generated.h"

/**
 *  A squad member. Same pawn as the suspects, down to the components and the combat behaviour;
 *  only the faction and the controller differ, which is what turns the shared state machine
 *  from a threat into someone who takes orders.
 */
UCLASS()
class PROJECTTF_API AAllyCharacter : public ACQBCharacter
{
	GENERATED_BODY()

public:

	AAllyCharacter();
};
