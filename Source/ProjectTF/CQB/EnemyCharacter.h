// CQB Sample - a hostile.

#pragma once

#include "CoreMinimal.h"
#include "CQBCharacter.h"
#include "EnemyCharacter.generated.h"

/**
 *  A suspect. Everything it can do lives in ACQBCharacter; this only picks a side and the brain
 *  that drives it.
 */
UCLASS()
class PROJECTTF_API AEnemyCharacter : public ACQBCharacter
{
	GENERATED_BODY()

public:

	AEnemyCharacter();
};
