// CQB Sample - data asset describing a hitscan weapon.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"

/**
 *  All tunable stats for a hitscan weapon.
 *  Create one from the Content Browser: Miscellaneous > Data Asset > WeaponData.
 *  A WeaponComponent with no asset assigned falls back to a transient instance of
 *  this class, so the defaults below are what you get out of the box.
 */
UCLASS(BlueprintType)
class PROJECTTF_API UWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Display name used by the debug HUD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName WeaponName = FName("Rifle");

	/** Damage dealt per bullet that connects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 20.0f;

	/** Shots per second. The refire timer uses 1 / FireRate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.01"))
	float FireRate = 8.0f;

	/** If true, holding the fire input keeps firing at FireRate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bAutomatic = true;

	/** Maximum hitscan trace distance in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "100.0"))
	float Range = 10000.0f;

	/** Rounds held by one magazine */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "1"))
	int32 MagSize = 30;

	/** Seconds a reload takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0.0"))
	float ReloadTime = 1.8f;

	/** Minimum upward kick per shot, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilPitchMin = 0.25f;

	/** Maximum upward kick per shot, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilPitchMax = 0.7f;

	/** Minimum horizontal kick per shot, in degrees. Negative values kick left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilYawMin = -0.25f;

	/** Maximum horizontal kick per shot, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilYawMax = 0.25f;

	/** Degrees per second the accumulated recoil is pulled back down once firing stops */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoverySpeed = 6.0f;

	/** Grace period after the last shot before recovery kicks in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoveryDelay = 0.15f;

	/** Camera FOV while aiming down sights */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float ADSFov = 55.0f;

	/** Camera FOV while hip firing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float HipFov = 90.0f;

	/** How fast the camera blends between hip and ADS FOV */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0.1"))
	float ADSInterpSpeed = 12.0f;

	/** Loudness reported to the AI hearing sense on every shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0"))
	float NoiseLoudness = 2.0f;

	/** Max distance the gunshot noise event carries, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0"))
	float NoiseRange = 4000.0f;

	/** Seconds between shots, derived from FireRate */
	float GetShotInterval() const { return 1.0f / FMath::Max(0.01f, FireRate); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("WeaponData"), GetFName());
	}
};
