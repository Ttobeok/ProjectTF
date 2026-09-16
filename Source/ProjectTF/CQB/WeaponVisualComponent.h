// CQB Sample - the weapon you can see. Gameplay lives in UWeaponComponent.
// CQB 샘플 - 눈에 보이는 무기. 게임플레이는 UWeaponComponent에 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "WeaponVisualComponent.generated.h"

class UWeaponComponent;

/**
 *  Where the visible weapon hangs
 *  보이는 무기가 어디에 매달리는가
 */
UENUM(BlueprintType)
enum class EWeaponAttachMode : uint8
{
	/**
	 *  On the owner's camera, as a first person view model
	 *  소유자의 카메라에. 1인칭 뷰모델 방식
	 */
	Camera		UMETA(DisplayName = "Camera"),

	/**
	 *  On a socket of the owner's mesh, for characters seen from the outside
	 *  소유자 메시의 소켓에. 밖에서 보이는 캐릭터용
	 */
	HandSocket	UMETA(DisplayName = "Hand Socket")
};

/**
 *  The weapon mesh, and nothing else.
 *
 *  Camera mode is a view model: the weapon sits at a fixed offset from the camera, so it tracks
 *  the view exactly. Parenting it to animated hands instead needs an arm animation that tracks
 *  the camera, and without one the weapon swings the opposite way on every turn.
 *
 *  Firing shoves the weapon back and tips the muzzle up, then it settles. The camera recoil is
 *  separate and lives in UWeaponComponent.
 *
 *  무기 메시, 그것뿐입니다.
 *
 *  Camera 모드는 뷰모델입니다. 무기가 카메라에서 고정 오프셋에 놓이므로 시점을 정확히
 *  따라갑니다. 애니메이션되는 손에 붙이려면 카메라를 따라가는 팔 애님이 필요하고,
 *  그게 없으면 고개를 돌릴 때마다 무기가 반대로 휩니다.
 *
 *  사격하면 무기가 뒤로 밀리고 총구가 들렸다가 가라앉습니다. 카메라 반동은 별개이며
 *  UWeaponComponent에 있습니다.
 */
UCLASS(ClassGroup = (CQB), meta = (BlueprintSpawnableComponent))
class PROJECTTF_API UWeaponVisualComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:

	UWeaponVisualComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 *  Where a shot should appear to come from, given the direction it was fired.
	 *
	 *  Uses the mesh's muzzle socket when it has one. Otherwise it runs along the aim from the
	 *  weapon's own position, which is all a tracer needs: the point is that it starts somewhere
	 *  other than the eye.
	 *
	 *  발사 방향을 받아, 사격이 어디서 나온 것처럼 보여야 하는지 알려줍니다.
	 *
	 *  메시에 총구 소켓이 있으면 그걸 씁니다. 없으면 무기 자신의 위치에서 조준 방향으로
	 *  나아간 지점을 씁니다. 예광선에는 그 정도면 충분합니다 — 핵심은 눈이 아닌 다른
	 *  곳에서 시작한다는 것입니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon Visual")
	FVector GetMuzzleLocation(const FVector& AimDirection) const;

	/**
	 *  Socket on the weapon mesh to fire from, when it has one
	 *  무기 메시에 총구 소켓이 있을 때 쓸 소켓 이름
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FName MuzzleSocket = FName("Muzzle");

	/**
	 *  Distance along the aim used when the mesh has no muzzle socket, in cm
	 *  총구 소켓이 없을 때 조준 방향으로 나아가는 거리(cm)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	float MuzzleForwardOffset = 55.0f;

	/**
	 *  Weapon mesh. Loaded on BeginPlay, never from a constructor.
	 *  무기 메시. BeginPlay에서 로드하며 생성자에서는 절대 하지 않습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	TSoftObjectPtr<USkeletalMesh> WeaponMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	EWeaponAttachMode AttachMode = EWeaponAttachMode::Camera;

	/**
	 *  Socket used in HandSocket mode
	 *  HandSocket 모드에서 쓰는 소켓
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FName HandSocket = FName("HandGrip_R");

	/**
	 *  Offset from the camera in Camera mode
	 *  Camera 모드에서 카메라 기준 오프셋
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FVector ViewOffset = FVector(42.0f, 13.0f, -16.0f);

	/**
	 *  The mesh is authored for a hand socket, so it has to be turned to lie along the view.
	 *  -90 puts the muzzle downrange; +90 is the same axis the other way round, which points the
	 *  stock at whatever you are aiming at.
	 *
	 *  메시가 손 소켓 기준으로 만들어져 있어서, 시선 방향에 맞게 돌려야 합니다.
	 *  -90이 총구를 앞으로 보냅니다. +90은 같은 축의 반대 방향이라 개머리판이 조준 대상을
	 *  향하게 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FRotator ViewRotation = FRotator(0.0f, -90.0f, 0.0f);

	/**
	 *  How far the weapon is shoved back on each shot, in cm
	 *  한 발당 무기가 뒤로 밀리는 거리(cm)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual|Kick")
	float KickDistance = 5.0f;

	/**
	 *  How far the muzzle rises on each shot, in degrees
	 *  한 발당 총구가 들리는 각도(도)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual|Kick")
	float KickPitch = 6.0f;

	/**
	 *  How fast the weapon settles back after a shot
	 *  사격 후 무기가 제자리로 돌아오는 속도
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual|Kick")
	float KickRecoverySpeed = 9.0f;

	/**
	 *  Shoves the weapon back. Called automatically on every shot.
	 *  무기를 뒤로 밉니다. 발사할 때마다 자동으로 불립니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Visual")
	void AddFireKick() { KickAlpha = 1.0f; }

protected:

	/**
	 *  Attaches to the camera or to the hand socket, depending on the mode
	 *  모드에 따라 카메라 또는 손 소켓에 부착
	 */
	void AttachToOwner();

	/**
	 *  Shot hook: the ammo count drops on a shot and fills on a reload
	 *  사격 훅: 탄약은 발사 시 줄고 재장전 시 채워집니다
	 */
	UFUNCTION()
	void OnAmmoChanged(int32 CurrentAmmo, int32 MagSize);

	/**
	 *  How much of the fire kick is left, 0 to 1
	 *  발사 반동이 얼마나 남았는지, 0~1
	 */
	float KickAlpha = 0.0f;
};
