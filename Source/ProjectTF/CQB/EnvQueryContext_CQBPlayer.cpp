// CQB Sample - EQS context that resolves to the player.

#include "EnvQueryContext_CQBPlayer.h"
#include "EnemyAIController.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "Kismet/GameplayStatics.h"

void UEnvQueryContext_CQBPlayer::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner)
	{
		return;
	}

	AActor* Target = nullptr;

	// prefer the player this enemy is actually tracking
	if (const AEnemyAIController* Controller = Cast<AEnemyAIController>(QueryOwner))
	{
		Target = Controller->GetPlayerTarget();
	}

	if (!Target)
	{
		Target = UGameplayStatics::GetPlayerPawn(QueryOwner, 0);
	}

	if (Target)
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Target);
	}
}
