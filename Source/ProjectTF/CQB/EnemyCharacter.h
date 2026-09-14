// CQB Sample - AI enemy pawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CQBTypes.h"
#include "Perception/AISightTargetInterface.h"
#include "EnemyCharacter.generated.h"

class UHealthComponent;
class UWeaponComponent;
class UWeaponVisualComponent;
class UAnimInstance;
class UNavigationInvokerComponent;

/**
 *  AI controlled enemy.
 *  Reuses the same UHealthComponent and UWeaponComponent as the player character.
 *  Ragdolls on death and is destroyed a few seconds later.
 */
UCLASS()
class PROJECTTF_API AEnemyCharacter : public ACharacter, public ICQBFactionAgent, public IAISightTargetInterface
{
	GENERATED_BODY()

	/** Health pool shared with the player character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UHealthComponent* HealthComponent;

	/** Hitscan weapon shared with the player character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWeaponComponent* WeaponComponent;

	/** The weapon you can see, on the hand socket */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWeaponVisualComponent* WeaponVisual;

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

	//~Begin ICQBFactionAgent
	virtual ECQBFaction GetFaction() const override { return Faction; }
	//~End ICQBFactionAgent

	//~Begin IAISightTargetInterface
	virtual UAISense_Sight::EVisibilityResult CanBeSeenFrom(const FCanBeSeenFromContext& Context,
		FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested,
		float& OutSightStrength, int32* UserData = nullptr,
		const FOnPendingVisibilityQueryProcessedDelegate* Delegate = nullptr) override;
	//~End IAISightTargetInterface

	UFUNCTION(BlueprintPure, Category = "Components")
	UWeaponVisualComponent* GetWeaponVisual() const { return WeaponVisual; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Ragdolls or destroys the pawn and lets the AI controller and squad know */
	UFUNCTION()
	void OnEnemyDeath(AActor* DeadActor, AActor* Killer);

	/** Removes the corpse */
	void DeferredDestroy();

	/** Side this character fights for. Subclasses change it to switch sides. */
	UPROPERTY(EditAnywhere, Category = "Enemy")
	ECQBFaction Faction = ECQBFaction::Enemy;

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

	/**
	 *  Rifle holding pose for the body. Soft on purpose: it lives in the shooter variant, and a
	 *  missing asset should leave the enemy in its default pose rather than break the build.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	TSoftClassPtr<UAnimInstance> BodyAnimAsset = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C")));

	/** Half angle of the enemy aim cone, in degrees. Keeps them from being laser accurate. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Weapon")
	float EnemyAimSpreadHalfAngle = 4.0f;

	bool bIsDead = false;

	FTimerHandle DestroyTimerHandle;
};
