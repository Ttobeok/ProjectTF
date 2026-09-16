// CQB Sample - AI enemy pawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CQBTypes.h"
#include "Perception/AISightTargetInterface.h"
#include "CQBCharacter.generated.h"

class UHealthComponent;
class UWeaponComponent;
class UWeaponVisualComponent;
class UAnimInstance;
class UNavigationInvokerComponent;

/**
 *  The pawn both AI sides are built on.
 *
 *  Carries the same health, weapon and visual components the player does, ragdolls on death and
 *  can be talked into surrendering. It takes no side of its own: AEnemyCharacter and
 *  AAllyCharacter are thin subclasses that set a faction and a controller, which is the only
 *  difference between a threat and a squad member.
 */
UCLASS()
class PROJECTTF_API ACQBCharacter : public ACharacter, public ICQBFactionAgent, public IAISightTargetInterface
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

	ACQBCharacter();

	UFUNCTION(BlueprintPure, Category = "Components")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Components")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "CQB")
	bool IsDead() const { return bIsDead; }

	/** Gave up: weapon down, no longer a threat. Anyone can ask, the controller sets it. */
	UFUNCTION(BlueprintPure, Category = "CQB")
	bool IsSurrendered() const { return bSurrendered; }

	/** Puts the weapon away and drops into a kneel. Called by the AI controller. */
	void SetSurrendered(bool bInSurrendered);

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
	void OnCharacterDeath(AActor* DeadActor, AActor* Killer);

	/** Removes the corpse */
	void DeferredDestroy();

	/** Side this character fights for. Subclasses set it. */
	UPROPERTY(EditAnywhere, Category = "CQB")
	ECQBFaction Faction = ECQBFaction::Neutral;

	/** Ragdoll on death when true, plain Destroy otherwise */
	UPROPERTY(EditAnywhere, Category = "CQB")
	bool bRagdollOnDeath = true;

	/**
	 *  Seconds a body lies there before it is removed. Zero or less leaves it for good, which is
	 *  the default: a room you have cleared should look like it, and the bodies are how the
	 *  player reads what happened while they were somewhere else.
	 */
	UPROPERTY(EditAnywhere, Category = "CQB")
	float DeferredDestructionTime = 0.0f;

	/** Starting health for enemies */
	UPROPERTY(EditAnywhere, Category = "CQB")
	float MaxHealth = 60.0f;

	/** Damage per enemy bullet */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	float WeaponDamage = 7.0f;

	/** Enemy shots per second */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	float WeaponFireRate = 2.5f;

	/**
	 *  Rifle holding pose for the body. Soft on purpose: it lives in the shooter variant, and a
	 *  missing asset should leave the enemy in its default pose rather than break the build.
	 */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	TSoftClassPtr<UAnimInstance> BodyAnimAsset = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C")));

	/** Half angle of the enemy aim cone, in degrees. Keeps them from being laser accurate. */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	float AimSpreadHalfAngle = 4.0f;

	bool bIsDead = false;

	bool bSurrendered = false;

	FTimerHandle DestroyTimerHandle;
};
