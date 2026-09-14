// CQB Sample - hitscan weapon component shared by player and enemies.

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
 */
UCLASS(ClassGroup = (CQB), meta = (BlueprintSpawnableComponent))
class PROJECTTF_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWeaponComponent();

	/** Stats for this weapon. When left empty a default UWeaponData is created at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TObjectPtr<UWeaponData> WeaponData;

	/** Half angle of the random aim cone, in degrees. Players want ~0, AI wants a few degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float AimSpreadHalfAngle = 0.35f;

	/** Recoil is pushed into the owning controller's rotation. Disabled for AI shooters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	bool bApplyRecoilToController = true;

	/** Print fire / hit / reload messages in the top left corner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowDebugMessages = true;

	/** Draw the bullet trace and impact point in the world */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebugTrace = true;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnAmmoChangedSignature OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponHitSignature OnWeaponHit;

	/** Fires one bullet if the weapon is ready. Also drives the automatic refire timer. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

	/** Begins holding the trigger. Automatic weapons keep firing until StopFire. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartFire();

	/** Releases the trigger */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

	/** Starts the reload timer. No-op when already full or reloading. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	/** Blends the camera towards the ADS FOV */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartADS();

	/** Blends the camera back to the hip FOV */
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

	/** Returns the assigned data asset, creating the runtime default the first time if needed. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponData* GetWeaponData() const;

	/** World space point bullets originate from (camera, or pawn eyes for AI) */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Resolves the trace origin and direction for this shot */
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Random pitch/yaw kick pushed into the controller rotation */
	void ApplyRecoil();

	/** Pulls accumulated recoil back down once the trigger has been idle */
	void RecoverRecoil(float DeltaTime);

	/** Blends the camera FOV between hip and ADS */
	void UpdateADS(float DeltaTime);

	/** Refire timer callback */
	void OnRefireReady();

	/** Reload timer callback */
	void FinishReload();

	/** True when the owner is alive and the weapon is not busy */
	bool CanFire() const;

	/** Screen message helper. Uses a stable key per owner so lines replace instead of stacking. */
	void DebugMessage(int32 Slot, const FColor& Color, const FString& Message) const;

	/** Camera used as the trace origin. Null on AI pawns. */
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> OwnerCamera;

	/** Instance created when no WeaponData asset is assigned */
	UPROPERTY(Transient)
	mutable TObjectPtr<UWeaponData> RuntimeDefaultData;

	UPROPERTY(Transient)
	int32 CurrentAmmo = 0;

	bool bTriggerHeld = false;
	bool bRefireCooldown = false;
	bool bIsReloading = false;
	bool bIsADS = false;

	/** Recoil pushed into the controller so far and not yet recovered, in degrees */
	FVector2D AccumulatedRecoil = FVector2D::ZeroVector;

	float LastFireTime = -100.0f;

	FTimerHandle RefireTimerHandle;
	FTimerHandle ReloadTimerHandle;
};
