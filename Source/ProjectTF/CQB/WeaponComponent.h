// CQB Sample - hitscan weapon component shared by player and enemies.
// CQB 샘플 - 플레이어와 적이 함께 쓰는 히트스캔 무기 컴포넌트.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class UCameraComponent;
class UWeaponData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChangedSignature, int32, CurrentAmmo, int32, MagSize);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponHitSignature, AActor*, HitActor, float, DamageDealt);

/**
 *  Hitscan weapon driven entirely from C++.
 *  Traces from the owner's camera component (or the pawn view point when there is no camera),
 *  applies damage to any UHealthComponent it hits, kicks the controller rotation for recoil and
 *  reports a hearing stimulus so nearby AI can react to gunfire.
 *
 *  전부 C++로 돌아가는 히트스캔 무기입니다.
 *  소유자의 카메라(카메라가 없으면 폰 시점)에서 트레이스를 쏘고, 맞은 UHealthComponent에
 *  피해를 주며, 반동으로 컨트롤러 회전을 밀고, 주변 AI가 총성에 반응하도록 청각 자극을
 *  보고합니다.
 */
UCLASS(ClassGroup = (CQB), meta = (BlueprintSpawnableComponent))
class PROJECTTF_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWeaponComponent();

	/**
	 *  Stats for this weapon. When left empty a default UWeaponData is created at runtime.
	 *  이 무기의 수치. 비워두면 런타임에 기본 UWeaponData를 만들어 씁니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TObjectPtr<UWeaponData> WeaponData;

	/**
	 *  Half angle of the random aim cone, in degrees. Players want ~0, AI wants a few degrees.
	 *  탄 퍼짐 원뿔의 반각(도). 플레이어는 0에 가깝게, AI는 몇 도쯤 줍니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float AimSpreadHalfAngle = 0.35f;

	/**
	 *  Recoil is pushed into the owning controller's rotation. Disabled for AI shooters.
	 *  반동을 소유 컨트롤러의 회전에 더합니다. AI 사수는 끕니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	bool bApplyRecoilToController = true;

	/**
	 *  Print fire / hit / reload messages in the top left corner.
	 *  Off by default: the sample ships as a Development build so the AI state text survives, so
	 *  anything left on here is printed over the real HUD eight times a second.
	 *
	 *  발사·명중·재장전 메시지를 화면 좌상단에 표시합니다.
	 *  기본은 꺼짐입니다. AI 상태 표시를 살리려고 Development로 납품하므로, 켜두면 초당
	 *  여덟 번씩 실제 HUD 위에 찍힙니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowDebugMessages = false;

	/**
	 *  Draw the bullet trace and impact point in the world. Off by default, for the same reason.
	 *  탄도와 명중 지점을 월드에 그립니다. 같은 이유로 기본은 꺼짐입니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebugTrace = false;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnAmmoChangedSignature OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponHitSignature OnWeaponHit;

	/**
	 *  Fires one bullet if the weapon is ready. Also drives the automatic refire timer.
	 *  준비됐으면 한 발 쏩니다. 자동 사격의 재발사 타이머도 여기서 굴립니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

	/**
	 *  Begins holding the trigger. Automatic weapons keep firing until StopFire.
	 *  방아쇠를 당기기 시작합니다. 자동 무기는 StopFire까지 계속 나갑니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartFire();

	/**
	 *  Releases the trigger
	 *  방아쇠를 놓습니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

	/**
	 *  Starts the reload timer. No-op when already full or reloading.
	 *  재장전 타이머를 겁니다. 이미 가득 찼거나 재장전 중이면 아무것도 하지 않습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	/**
	 *  Blends the camera towards the ADS FOV
	 *  카메라를 정조준 FOV 쪽으로 보간
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartADS();

	/**
	 *  Blends the camera back to the hip FOV
	 *  카메라를 허리 사격 FOV로 되돌림
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopADS();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsADS() const { return bIsADS; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsFiring() const { return bTriggerHeld; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetMagSize() const;

	/**
	 *  Returns the assigned data asset, creating the runtime default the first time if needed.
	 *  지정된 데이터 에셋을 돌려줍니다. 없으면 처음 호출 때 런타임 기본값을 만듭니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponData* GetWeaponData() const;

	/**
	 *  World space point bullets originate from (camera, or pawn eyes for AI)
	 *  탄이 출발하는 월드 좌표(카메라, AI는 폰 눈높이)
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 *  Resolves the trace origin and direction for this shot
	 *  이번 사격의 트레이스 시작점과 방향을 결정
	 */
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/**
	 *  Random pitch/yaw kick pushed into the controller rotation
	 *  컨트롤러 회전에 더해지는 무작위 pitch/yaw 반동
	 */
	void ApplyRecoil();

	/**
	 *  Pulls accumulated recoil back down once the trigger has been idle
	 *  방아쇠를 놓고 있으면 누적된 반동을 도로 내립니다
	 */
	void RecoverRecoil(float DeltaTime);

	/**
	 *  Blends the camera FOV between hip and ADS
	 *  카메라 FOV를 허리/정조준 사이로 보간
	 */
	void UpdateADS(float DeltaTime);

	/**
	 *  Refire timer callback
	 *  재발사 타이머 콜백
	 */
	void OnRefireReady();

	/**
	 *  Reload timer callback
	 *  재장전 타이머 콜백
	 */
	void FinishReload();

	/**
	 *  True when the owner is alive and the weapon is not busy
	 *  소유자가 살아 있고 무기가 바쁘지 않으면 true
	 */
	bool CanFire() const;

	/**
	 *  Screen message helper. Uses a stable key per owner so lines replace instead of stacking.
	 *  화면 메시지 헬퍼. 소유자마다 고정 키를 써서 줄이 쌓이지 않고 교체되게 합니다.
	 */
	void DebugMessage(int32 Slot, const FColor& Color, const FString& Message) const;

	/**
	 *  Camera used as the trace origin. Null on AI pawns.
	 *  트레이스 시작점으로 쓰는 카메라. AI 폰에서는 null입니다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> OwnerCamera;

	/**
	 *  Instance created when no WeaponData asset is assigned
	 *  WeaponData 에셋이 없을 때 만들어지는 인스턴스
	 */
	UPROPERTY(Transient)
	mutable TObjectPtr<UWeaponData> RuntimeDefaultData;

	UPROPERTY(Transient)
	int32 CurrentAmmo = 0;

	bool bTriggerHeld = false;
	bool bRefireCooldown = false;
	bool bIsReloading = false;
	bool bIsADS = false;

	/**
	 *  Recoil pushed into the controller so far and not yet recovered, in degrees
	 *  컨트롤러에 더해졌고 아직 회복되지 않은 반동 누적치(도)
	 */
	FVector2D AccumulatedRecoil = FVector2D::ZeroVector;

	float LastFireTime = -100.0f;

	FTimerHandle RefireTimerHandle;
	FTimerHandle ReloadTimerHandle;
};
