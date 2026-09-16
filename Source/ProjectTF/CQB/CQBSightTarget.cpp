// CQB Sample - how a character is seen by the AI sight sense.
// CQB 샘플 - AI 시야 감각이 캐릭터를 어떻게 보는가.

#include "CQBSightTarget.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace CQBSightTarget
{
	bool CanBeSeenFrom(const AActor& Target, const FVector& ObserverLocation,
		const AActor* IgnoreActor, FVector& OutSeenLocation, int32& OutChecksPerformed, float& OutSightStrength)
	{
		OutSightStrength = 0.0f;

		const UWorld* World = Target.GetWorld();
		if (!World)
		{
			return false;
		}

		TArray<FVector, TInlineAllocator<3>> Points;
		GetSightPoints(Target, Points);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBSightTest), true, IgnoreActor);
		Params.AddIgnoredActor(&Target);

		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			++OutChecksPerformed;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, ObserverLocation, Points[Index], ECC_Visibility, Params);

			if (!bBlocked)
			{
				OutSeenLocation = Points[Index];

				// Points run head first, so the index says how much of the body is exposed: a
				// target showing only the top of its head is not as visible as one standing in
				// the open, and reporting 1.0 for both throws away what the probes just measured.
				//
				// 지점들은 머리부터 순서대로이므로 인덱스가 곧 노출 정도입니다. 머리만 내민
				// 대상과 훤히 선 대상이 같을 리 없는데, 둘 다 1.0으로 보고하면 방금 잰 것을
				// 버리는 셈입니다.
				OutSightStrength = 1.0f - (static_cast<float>(Index) / static_cast<float>(Points.Num()));
				return true;
			}
		}

		OutSightStrength = 0.0f;
		return false;
	}
}
