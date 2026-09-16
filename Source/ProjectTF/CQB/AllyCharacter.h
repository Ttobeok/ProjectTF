// CQB Sample - a squad member the player commands.
// CQB 샘플 - 플레이어가 지휘하는 분대원.

#pragma once

#include "CoreMinimal.h"
#include "CQBCharacter.h"
#include "AllyCharacter.generated.h"

/**
 *  A squad member. Same pawn as the suspects, down to the components and the combat behaviour;
 *  only the faction and the controller differ, which is what turns the shared state machine
 *  from a threat into someone who takes orders.
 *
 *  분대원. 컴포넌트와 전투 행동까지 용의자와 같은 폰입니다. 다른 것은 진영과 컨트롤러뿐이고,
 *  공유하는 상태머신을 위협에서 명령을 받는 쪽으로 바꾸는 것이 바로 그 둘입니다.
 */
UCLASS()
class PROJECTTF_API AAllyCharacter : public ACQBCharacter
{
	GENERATED_BODY()

public:

	AAllyCharacter();
};
