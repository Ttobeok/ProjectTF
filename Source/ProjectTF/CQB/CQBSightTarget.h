// CQB Sample - how a character is seen by the AI sight sense.

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
 */
namespace CQBSightTarget
{
	/** Heights above the actor origin to test, nearest the head first */
	inline void GetSightPoints(const AActor& Target, TArray<FVector, TInlineAllocator<3>>& OutPoints)
	{
		const FVector Origin = Target.GetActorLocation();

		OutPoints.Add(Origin + FVector(0.0f, 0.0f, 60.0f));		// eyes
		OutPoints.Add(Origin + FVector(0.0f, 0.0f, 25.0f));		// chest
		OutPoints.Add(Origin);									// waist
	}

	/**
	 *  Traces from the observer to each sight point and reports the first one that is exposed.
	 *  Returns true when any part of the target can be seen.
	 */
	bool PROJECTTF_API CanBeSeenFrom(const AActor& Target, const FVector& ObserverLocation,
		const AActor* IgnoreActor, FVector& OutSeenLocation, int32& OutChecksPerformed, float& OutSightStrength);
}
