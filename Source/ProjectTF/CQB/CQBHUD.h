// CQB Sample - crosshair and ammo readout, drawn from C++.
// CQB 샘플 - C++로 그리는 크로스헤어와 탄약 표시.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CQBHUD.generated.h"

class UWeaponComponent;
class UHealthComponent;

/**
 *  Draws the crosshair and a small ammo and health readout.
 *  Everything is drawn with the canvas, so the HUD needs no assets or widgets.
 *
 *  크로스헤어와 간단한 탄약·체력 표시를 그립니다.
 *  전부 캔버스로 그리므로 이 HUD는 에셋도 위젯도 필요하지 않습니다.
 */
UCLASS()
class PROJECTTF_API ACQBHUD : public AHUD
{
	GENERATED_BODY()

public:

	ACQBHUD();

	virtual void DrawHUD() override;

protected:

	/**
	 *  Four ticks around a centre dot. The gap grows with spread.
	 *  가운데 점을 둘러싼 네 개의 눈금. 탄 퍼짐이 커지면 간격도 벌어집니다.
	 */
	void DrawCrosshair(const UWeaponComponent* Weapon);

	/**
	 *  Ammo counter bottom right, health bottom left
	 *  탄약은 우하단, 체력은 좌하단
	 */
	void DrawReadout(const UWeaponComponent* Weapon, const UHealthComponent* Health);

	/**
	 *  Squad orders along the bottom, and the doorway hint when one is under the crosshair
	 *  하단의 분대 명령, 그리고 크로스헤어에 문이 잡히면 나오는 힌트
	 */
	void DrawSquadBar();

	/**
	 *  Length of each crosshair tick, in pixels
	 *  크로스헤어 눈금 하나의 길이(픽셀)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float TickLength = 10.0f;

	/**
	 *  Thickness of each crosshair tick, in pixels
	 *  크로스헤어 눈금 하나의 두께(픽셀)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float TickThickness = 2.0f;

	/**
	 *  Gap from the centre while hip firing, in pixels
	 *  허리 사격 중 중심에서의 간격(픽셀)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float HipGap = 14.0f;

	/**
	 *  Gap from the centre while aiming, in pixels
	 *  정조준 중 중심에서의 간격(픽셀)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float ADSGap = 5.0f;

	/**
	 *  Extra gap added briefly after each shot, in pixels
	 *  발사 직후 잠깐 더해지는 간격(픽셀)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float FireKickGap = 12.0f;

	/**
	 *  How fast the gap settles back down
	 *  간격이 원래대로 돌아오는 속도
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float GapInterpSpeed = 9.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	/**
	 *  Turns red for a moment when a shot connects
	 *  명중하면 잠시 빨갛게 바뀝니다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor HitMarkerColor = FLinearColor(1.0f, 0.25f, 0.2f, 1.0f);

	/**
	 *  Called when the player weapon hits something, to flash the hit marker
	 *  플레이어 무기가 뭔가 맞혔을 때 히트 마커를 번쩍이기 위해 불립니다
	 */
	UFUNCTION()
	void OnWeaponHit(AActor* HitActor, float DamageDealt);

	/**
	 *  Called when the player weapon ammo changes, to kick the crosshair open
	 *  플레이어 무기 탄약이 바뀌었을 때 크로스헤어를 벌리기 위해 불립니다
	 */
	UFUNCTION()
	void OnAmmoChanged(int32 CurrentAmmo, int32 MagSize);

	/**
	 *  Hooks the player weapon delegates once the pawn exists
	 *  폰이 생긴 뒤 플레이어 무기 델리게이트에 연결합니다
	 */
	void BindToPlayerWeapon();

	/**
	 *  Current interpolated crosshair gap
	 *  현재 보간된 크로스헤어 간격
	 */
	float CurrentGap = 14.0f;

	/**
	 *  Extra gap left over from the last shot
	 *  직전 발사에서 남은 추가 간격
	 */
	float FireKick = 0.0f;

	/**
	 *  Seconds of hit marker left
	 *  히트 마커가 남은 시간(초)
	 */
	float HitMarkerTime = 0.0f;

	/**
	 *  Weapon the delegates are bound to
	 *  델리게이트가 연결된 무기
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWeaponComponent> BoundWeapon;
};
