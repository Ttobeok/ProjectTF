// CQB Sample - a hostile.
// CQB 샘플 - 적대 세력.

#pragma once

#include "CoreMinimal.h"
#include "CQBCharacter.h"
#include "EnemyCharacter.generated.h"

/**
 *  A suspect. Everything it can do lives in ACQBCharacter; this only picks a side and the brain
 *  that drives it.
 *
 *  용의자. 할 수 있는 일은 전부 ACQBCharacter에 있고, 여기서는 어느 편인지와 어느 뇌가
 *  조종할지만 정합니다.
 */
UCLASS()
class PROJECTTF_API AEnemyCharacter : public ACQBCharacter
{
	GENERATED_BODY()

public:

	AEnemyCharacter();
};
