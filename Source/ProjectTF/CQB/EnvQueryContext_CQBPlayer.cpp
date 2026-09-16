// CQB Sample - EQS context that resolves to the player.
// CQB 샘플 - 플레이어로 해석되는 EQS 컨텍스트.

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
	// 이 적이 실제로 추적 중인 플레이어를 우선합니다
	if (const AEnemyAIController* Controller = Cast<AEnemyAIController>(QueryOwner))
	{
		Target = Controller->GetCurrentTarget();
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
