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
};
