// CQB Sample - AI enemy pawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacter.generated.h"

class UHealthComponent;
class UWeaponComponent;

/**
 *  AI controlled enemy.
 *  Reuses the same UHealthComponent and UWeaponComponent as the player character.
 *  Ragdolls on death and is destroyed a few seconds later.
 */
UCLASS()
class PROJECTTF_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Health pool shared with the player character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UHealthComponent* HealthComponent;

	/** Hitscan weapon shared with the player character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWeaponComponent* WeaponComponent;

public:

	AEnemyCharacter();

	UFUNCTION(BlueprintPure, Category = "Components")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Components")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return bIsDead; }

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

	/** Half angle of the enemy aim cone, in degrees. Keeps them from being laser accurate. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	float EnemyAimSpreadHalfAngle = 4.0f;

	bool bIsDead = false;

	FTimerHandle DestroyTimerHandle;
};
