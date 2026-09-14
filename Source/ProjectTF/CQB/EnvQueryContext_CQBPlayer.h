// CQB Sample - EQS context that resolves to the player.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_CQBPlayer.generated.h"

/**
 *  Returns the player pawn as the EQS context.
 *  Use it as the Context of a cover query so tests such as Trace can be run against the player.
 */
UCLASS()
class PROJECTTF_API UEnvQueryContext_CQBPlayer : public UEnvQueryContext
{
	GENERATED_BODY()

public:

	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
