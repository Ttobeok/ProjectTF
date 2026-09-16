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
		const UWorld* World = Target.GetWorld();
		if (!World)
		{
			return false;
		}

		TArray<FVector, TInlineAllocator<3>> Points;
		GetSightPoints(Target, Points);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBSightTest), true, IgnoreActor);
		Params.AddIgnoredActor(&Target);

		for (const FVector& Point : Points)
		{
			++OutChecksPerformed;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, ObserverLocation, Point, ECC_Visibility, Params);

			if (!bBlocked)
			{
				OutSeenLocation = Point;
				OutSightStrength = 1.0f;
				return true;
			}
		}

		OutSightStrength = 0.0f;
		return false;
	}
}
