// CQB Sample - the weapon you can see. Gameplay lives in UWeaponComponent.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "WeaponVisualComponent.generated.h"

class UWeaponComponent;

/** Where the visible weapon hangs */
UENUM(BlueprintType)
enum class EWeaponAttachMode : uint8
{
	/** On the owner's camera, as a first person view model */
	Camera		UMETA(DisplayName = "Camera"),

	/** On a socket of the owner's mesh, for characters seen from the outside */
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
 */
UCLASS(ClassGroup = (CQB), meta = (BlueprintSpawnableComponent))
class PROJECTTF_API UWeaponVisualComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:

	UWeaponVisualComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Weapon mesh. Loaded on BeginPlay, never from a constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	TSoftObjectPtr<USkeletalMesh> WeaponMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	EWeaponAttachMode AttachMode = EWeaponAttachMode::Camera;

	/** Socket used in HandSocket mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FName HandSocket = FName("HandGrip_R");

	/** Offset from the camera in Camera mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FVector ViewOffset = FVector(42.0f, 13.0f, -16.0f);

	/**
	 *  The mesh is authored for a hand socket, so it has to be turned to lie along the view.
	 *  -90 puts the muzzle downrange; +90 is the same axis the other way round, which points the
	 *  stock at whatever you are aiming at.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FRotator ViewRotation = FRotator(0.0f, -90.0f, 0.0f);

	/** How far the weapon is shoved back on each shot, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual|Kick")
	float KickDistance = 5.0f;

	/** How far the muzzle rises on each shot, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual|Kick")
	float KickPitch = 6.0f;

	/** How fast the weapon settles back after a shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual|Kick")
	float KickRecoverySpeed = 9.0f;

	/** Shoves the weapon back. Called automatically on every shot. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Visual")
	void AddFireKick() { KickAlpha = 1.0f; }

protected:

	/** Attaches to the camera or to the hand socket, depending on the mode */
	void AttachToOwner();

	/** Shot hook: the ammo count drops on a shot and fills on a reload */
	UFUNCTION()
	void OnAmmoChanged(int32 CurrentAmmo, int32 MagSize);

	/** How much of the fire kick is left, 0 to 1 */
	float KickAlpha = 0.0f;
};
