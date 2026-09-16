// CQB Sample - how a character is seen by the AI sight sense.
// CQB 샘플 - AI 시야 감각이 캐릭터를 어떻게 보는가.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AISightTargetInterface.h"

/**
 *  Shared implementation of IAISightTargetInterface::CanBeSeenFrom for characters.
 *
 *  Left to itself the sight sense traces to the actor origin, which on a character sits at the
 *  waist. Cover in this level is 110 to 120 cm tall, so a standing figure whose head and chest
 *  are in plain view reads as completely hidden, and the AI walks past people it is looking at.
 *
 *  Testing eyes first, then chest, then waist matches what the geometry says: visible if any
 *  part of the body is exposed, and the part that is seen is reported back so the AI aims there.
 *
 *  캐릭터용 IAISightTargetInterface::CanBeSeenFrom 공통 구현입니다.
 *
 *  그대로 두면 시야 감각은 액터 원점으로 트레이스하는데, 캐릭터에서 그 지점은 허리입니다.
 *  이 레벨의 엄폐물은 110~120cm라, 머리와 가슴이 훤히 보이는 사람이 완전히 숨은 것으로
 *  판정되고 AI는 자기가 보고 있는 사람을 지나쳐 갑니다.
 *
 *  눈 → 가슴 → 허리 순으로 검사하면 지오메트리가 말하는 것과 일치합니다. 몸의 어느 한 곳이라도
 *  노출되면 보이는 것이고, 보인 지점을 되돌려주어 AI가 그곳을 조준하게 합니다.
 */
namespace CQBSightTarget
{
	/**
	 *  Heights above the actor origin to test, nearest the head first
	 *  액터 원점에서 검사할 높이들. 머리에 가까운 순서
	 */
	inline void GetSightPoints(const AActor& Target, TArray<FVector, TInlineAllocator<3>>& OutPoints)
	{
		const FVector Origin = Target.GetActorLocation();

		OutPoints.Add(Origin + FVector(0.0f, 0.0f, 60.0f));		// eyes / 눈
		OutPoints.Add(Origin + FVector(0.0f, 0.0f, 25.0f));		// chest / 가슴
		OutPoints.Add(Origin);									// waist / 허리
	}

	/**
	 *  Traces from the observer to each sight point and reports the first one that is exposed.
	 *  Returns true when any part of the target can be seen.
	 *
	 *  관찰자에서 각 시야 지점으로 트레이스를 쏘고, 처음으로 노출된 지점을 보고합니다.
	 *  대상의 어느 부분이라도 보이면 true를 돌려줍니다.
	 */
	bool PROJECTTF_API CanBeSeenFrom(const AActor& Target, const FVector& ObserverLocation,
		const AActor* IgnoreActor, FVector& OutSeenLocation, int32& OutChecksPerformed, float& OutSightStrength);
}
