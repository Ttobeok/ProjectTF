// CQB Sample - EQS context that resolves to the player.
// CQB 샘플 - 플레이어로 해석되는 EQS 컨텍스트.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_CQBPlayer.generated.h"

/**
 *  Returns the player pawn as the EQS context.
 *  Use it as the Context of a cover query so tests such as Trace can be run against the player.
 *
 *  플레이어 폰을 EQS 컨텍스트로 돌려줍니다.
 *  엄폐 쿼리의 Context로 지정하면 Trace 같은 테스트를 플레이어 기준으로 돌릴 수 있습니다.
 */
UCLASS()
class PROJECTTF_API UEnvQueryContext_CQBPlayer : public UEnvQueryContext
{
	GENERATED_BODY()

public:

	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
