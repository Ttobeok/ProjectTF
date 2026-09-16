// Copyright Epic Games, Inc. All Rights Reserved.


#include "ProjectTFCameraManager.h"
#include "ProjectTFCharacter.h"

AProjectTFCameraManager::AProjectTFCameraManager()
{
	// set the min/max pitch
	// 최소/최대 피치를 설정합니다
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;

	// Radius of the sphere swept when leaning, in cm. Small enough to reach into a doorway,
	// large enough that the eye cannot end up inside a wall.
	// 린할 때 스윕하는 구의 반경(cm). 문간으로 들어갈 만큼 작고, 눈이 벽 속에 들어가지
	// 않을 만큼은 큽니다.
	LeanProbeRadius = 12.0f;
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

	// Slide the camera sideways using the yaw only, so looking up or down keeps the lean level.
	// yaw만 써서 카메라를 옆으로 밀어냅니다. 위아래를 봐도 린이 수평을 유지하도록
	const FRotator YawOnly(0.0f, OutVT.POV.Rotation.Yaw, 0.0f);
	const FVector Desired = OutVT.POV.Location + YawOnly.RotateVector(FVector::RightVector) * LeanOffset;

	// Sweep to it. The capsule is 34 cm in radius and the lean reaches 45, so standing against a
	// wall and leaning into it put the eye 11 cm past the surface - and straight through any
	// partition thinner than that. Beside a doorway that is a free look into the room with no
	// exposure at all, which is the opposite of what leaning is supposed to cost you.
	//
	// 스윕해서 이동합니다. 캡슐 반경은 34cm인데 린은 45cm까지 나가므로, 벽에 붙어 그쪽으로
	// 기울이면 눈이 표면 밖 11cm에 놓입니다. 그보다 얇은 칸막이는 그냥 통과합니다. 문 옆에서
	// 이러면 노출 없이 방을 들여다보는 셈이라, 린이 치러야 할 대가와 정반대가 됩니다.
	const UWorld* World = Character->GetWorld();
	if (!World)
	{
		OutVT.POV.Location = Desired;
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBLeanSweep), false, Character);

	FHitResult Hit;
	const bool bBlocked = World->SweepSingleByChannel(Hit, OutVT.POV.Location, Desired,
		FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(LeanProbeRadius), Params);

	OutVT.POV.Location = bBlocked ? Hit.Location : Desired;
}
