// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ProjectTFCameraManager.generated.h"

/**
 *  Basic First Person camera manager.
 *  Limits min/max look pitch and applies the character's lean (camera roll plus a sideways offset).
 */
UCLASS()
class AProjectTFCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:

	/** Constructor */
	AProjectTFCameraManager();

	/** Applies the lean offset on top of the regular first person view */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

	/**
	 *  Radius of the sphere swept when leaning, in cm. Keeps the eye out of walls.
	 *  린할 때 스윕하는 구의 반경(cm). 눈이 벽에 들어가지 않게 합니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "CQB|Lean")
	float LeanProbeRadius = 12.0f;
};
