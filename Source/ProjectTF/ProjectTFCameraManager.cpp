// Copyright Epic Games, Inc. All Rights Reserved.


#include "ProjectTFCameraManager.h"
#include "ProjectTFCharacter.h"

AProjectTFCameraManager::AProjectTFCameraManager()
{
	// set the min/max pitch
	// 최소/최대 피치를 설정합니다
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void AProjectTFCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	// the camera component owns the view rotation, so the lean is applied here on the final POV
	// 시점 회전은 카메라 컴포넌트가 쥐고 있으므로, 린은 최종 POV가 정해진 여기서 적용합니다
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
	// yaw만 써서 카메라를 옆으로 밀어냅니다. 위아래를 봐도 린이 수평을 유지하도록
	const FRotator YawOnly(0.0f, OutVT.POV.Rotation.Yaw, 0.0f);
	OutVT.POV.Location += YawOnly.RotateVector(FVector::RightVector) * LeanOffset;
}
