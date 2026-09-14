// Copyright Epic Games, Inc. All Rights Reserved.


#include "ProjectTFCameraManager.h"
#include "ProjectTFCharacter.h"

AProjectTFCameraManager::AProjectTFCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void AProjectTFCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	// the camera component owns the view rotation, so the lean is applied here on the final POV
	const AProjectTFCharacter* Character = Cast<AProjectTFCharacter>(OutVT.Target);
	if (!Character)
	{
		return;
	}

	const float LeanRoll = Character->GetLeanRoll();
	const float LeanOffset = Character->GetLeanOffset();

	if (FMath::IsNearlyZero(LeanRoll) && FMath::IsNearlyZero(LeanOffset))
	{
		return;
	}

	OutVT.POV.Rotation.Roll += LeanRoll;

	// slide the camera sideways using the yaw only, so looking up or down keeps the lean level
	const FRotator YawOnly(0.0f, OutVT.POV.Rotation.Yaw, 0.0f);
	OutVT.POV.Location += YawOnly.RotateVector(FVector::RightVector) * LeanOffset;
}
