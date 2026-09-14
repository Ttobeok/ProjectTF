// CQB Sample - AI enemy pawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Variant_Shooter/Weapons/ShooterWeaponHolder.h"
#include "EnemyCharacter.generated.h"

class UHealthComponent;
class UWeaponComponent;
class AShooterWeapon;
class UAnimMontage;
class UNavigationInvokerComponent;

/**
 *  AI controlled enemy.
 *  Reuses the same UHealthComponent and UWeaponComponent as the player character.
 *  Ragdolls on death and is destroyed a few seconds later.
 */
UCLASS()
class PROJECTTF_API AEnemyCharacter : public ACharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()

	/** Health pool shared with the player character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UHealthComponent* HealthComponent;

	/** Hitscan weapon shared with the player character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWeaponComponent* WeaponComponent;

	/** Weapon actor used for the meshes and the rifle poses */
	UPROPERTY(Transient)
	TObjectPtr<AShooterWeapon> WeaponVisual;

	/** Keeps navmesh tiles generated around this character at runtime */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UNavigationInvokerComponent* NavigationInvoker;

public:

	AEnemyCharacter();

	UFUNCTION(BlueprintPure, Category = "Components")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Components")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return bIsDead; }

	//~Begin IShooterWeaponHolder interface. Visuals only; the firing lives in UWeaponComponent.
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;
	virtual void AddWeaponRecoil(float Recoil) override;
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;
	virtual FVector GetWeaponTargetLocation() override;
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;
	virtual void OnSemiWeaponRefire() override;
	//~End IShooterWeaponHolder interface

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Ragdolls or destroys the pawn and lets the AI controller and squad know */
	UFUNCTION()
	void OnEnemyDeath(AActor* DeadActor, AActor* Killer);

	/** Removes the corpse */
	void DeferredDestroy();

	/** Ragdoll on death when true, plain Destroy otherwise */
	UPROPERTY(EditAnywhere, Category = "Enemy")
	bool bRagdollOnDeath = true;

	/** Seconds the corpse stays around before being destroyed */
	UPROPERTY(EditAnywhere, Category = "Enemy")
	float DeferredDestructionTime = 6.0f;

	/** Starting health for enemies */
	UPROPERTY(EditAnywhere, Category = "Enemy")
	float EnemyMaxHealth = 60.0f;

	/** Damage per enemy bullet */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	float EnemyWeaponDamage = 7.0f;

	/** Enemy shots per second */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	float EnemyWeaponFireRate = 2.5f;

	/** Weapon actor spawned for the visuals */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	TSubclassOf<AShooterWeapon> WeaponVisualClass;

	/** Loaded on BeginPlay when no class is set. Never load this from the constructor. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	TSoftClassPtr<AShooterWeapon> WeaponVisualAsset = TSoftClassPtr<AShooterWeapon>(FSoftObjectPath(TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Rifle.BP_ShooterWeapon_Rifle_C")));

	/** Socket the weapon mesh attaches to */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	FName WeaponSocket = FName("HandGrip_R");

	/** Half angle of the enemy aim cone, in degrees. Keeps them from being laser accurate. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	float EnemyAimSpreadHalfAngle = 4.0f;

	bool bIsDead = false;

	FTimerHandle DestroyTimerHandle;
};
