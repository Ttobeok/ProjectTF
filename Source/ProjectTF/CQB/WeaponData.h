// CQB Sample - data asset describing a hitscan weapon.
// CQB 샘플 - 히트스캔 무기의 수치를 담는 데이터 에셋.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"

/**
 *  All tunable stats for a hitscan weapon.
 *  Create one from the Content Browser: Miscellaneous > Data Asset > WeaponData.
 *  A WeaponComponent with no asset assigned falls back to a transient instance of
 *  this class, so the defaults below are what you get out of the box.
 *
 *  히트스캔 무기의 조정 가능한 수치 전부입니다.
 *  콘텐츠 브라우저에서 Miscellaneous > Data Asset > WeaponData로 만듭니다.
 *  에셋을 지정하지 않은 WeaponComponent는 이 클래스의 임시 인스턴스를 만들어 쓰므로,
 *  아래 기본값이 아무것도 설정하지 않았을 때의 값입니다.
 */
UCLASS(BlueprintType)
class PROJECTTF_API UWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/**
	 *  Display name used by the debug HUD
	 *  디버그 HUD에 표시되는 이름
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName WeaponName = FName("Rifle");

	/**
	 *  Damage dealt per bullet that connects
	 *  명중한 탄 한 발당 피해량
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 20.0f;

	/**
	 *  Shots per second. The refire timer uses 1 / FireRate.
	 *  초당 발사 수. 재발사 타이머가 1 / FireRate를 씁니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.01"))
	float FireRate = 8.0f;

	/**
	 *  If true, holding the fire input keeps firing at FireRate
	 *  true면 발사 입력을 누르고 있는 동안 FireRate로 계속 나갑니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bAutomatic = true;

	/**
	 *  Maximum hitscan trace distance in cm
	 *  히트스캔 트레이스 최대 거리(cm)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "100.0"))
	float Range = 10000.0f;

	/**
	 *  Rounds held by one magazine
	 *  탄창 하나에 들어가는 탄 수
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "1"))
	int32 MagSize = 30;

	/**
	 *  Seconds a reload takes
	 *  재장전에 걸리는 시간(초)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0.0"))
	float ReloadTime = 1.8f;

	/**
	 *  Minimum upward kick per shot, in degrees
	 *  한 발당 최소 상승 반동(도)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilPitchMin = 0.25f;

	/**
	 *  Maximum upward kick per shot, in degrees
	 *  한 발당 최대 상승 반동(도)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilPitchMax = 0.7f;

	/**
	 *  Minimum horizontal kick per shot, in degrees. Negative values kick left.
	 *  한 발당 최소 좌우 반동(도). 음수면 왼쪽으로 틉니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilYawMin = -0.25f;

	/**
	 *  Maximum horizontal kick per shot, in degrees
	 *  한 발당 최대 좌우 반동(도)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilYawMax = 0.25f;

	/**
	 *  Degrees per second the accumulated recoil is pulled back down once firing stops
	 *  사격을 멈춘 뒤 누적 반동을 되돌리는 속도(도/초)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoverySpeed = 6.0f;

	/**
	 *  Grace period after the last shot before recovery kicks in
	 *  마지막 발사 후 반동 회복이 시작되기까지의 유예
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoveryDelay = 0.15f;

	/**
	 *  Camera FOV while aiming down sights
	 *  정조준 중 카메라 FOV
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float ADSFov = 55.0f;

	/**
	 *  Camera FOV while hip firing
	 *  허리 사격 중 카메라 FOV
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float HipFov = 90.0f;

	/**
	 *  How fast the camera blends between hip and ADS FOV
	 *  허리/정조준 FOV 사이를 오가는 보간 속도
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0.1"))
	float ADSInterpSpeed = 12.0f;

	/**
	 *  Loudness reported to the AI hearing sense on every shot
	 *  발사할 때마다 AI 청각에 전달되는 소리 크기
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0"))
	float NoiseLoudness = 2.0f;

	/**
	 *  Max distance the gunshot noise event carries, in cm
	 *  총성 노이즈 이벤트가 닿는 최대 거리(cm)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0"))
	float NoiseRange = 4000.0f;

	/**
	 *  Seconds between shots, derived from FireRate
	 *  FireRate에서 유도되는 발사 간격(초)
	 */
	float GetShotInterval() const { return 1.0f / FMath::Max(0.01f, FireRate); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("WeaponData"), GetFName());
	}
};
