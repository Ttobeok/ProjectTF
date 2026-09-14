// CQB Sample - shared health component for player and enemies.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

/** Fired whenever health changes. Delta is negative for damage, positive for healing. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHealthChangedSignature, UHealthComponent*, HealthComp, float, NewHealth, float, Delta, AActor*, Causer);

/** Fired once when health reaches zero. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeathSignature, AActor*, DeadActor, AActor*, Killer);

/**
 *  Minimal health container shared by the player character and the AI enemies.
 *  Also listens to the engine damage pipeline so ApplyDamage() based sources work too.
 */
UCLASS(ClassGroup = (CQB), meta = (BlueprintSpawnableComponent))
class PROJECTTF_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHealthComponent();

	/** Maximum health granted on BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.0f;

	/** Current health. Replicated-free, this is a single player sample. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.0f;

	/** Broadcast on every health change */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChangedSignature OnHealthChanged;

	/** Broadcast once when this actor dies */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathSignature OnDeath;

	/** Applies damage. Returns the amount actually applied. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float TakeDamage(float DamageAmount, AActor* DamageCauser = nullptr, AController* EventInstigator = nullptr);

	/** Restores health up to MaxHealth. Ignored once dead. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float HealAmount);

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	/** Convenience: returns the health component of an actor, or null */
	static UHealthComponent* FindHealthComponent(AActor* Actor);

protected:

	virtual void BeginPlay() override;

	/** Bridges AActor::TakeDamage / UGameplayStatics::ApplyDamage into this component */
	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** True once health has hit zero, so death only fires once */
	bool bIsDead = false;
};
