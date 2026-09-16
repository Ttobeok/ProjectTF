// CQB Sample - shared health component for player and enemies.
// CQB 샘플 - 플레이어와 적이 함께 쓰는 체력 컴포넌트.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

/**
 *  Fired whenever health changes. Delta is negative for damage, positive for healing.
 *  체력이 바뀔 때마다 발생합니다. Delta는 피해면 음수, 회복이면 양수입니다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHealthChangedSignature, UHealthComponent*, HealthComp, float, NewHealth, float, Delta, AActor*, Causer);

/**
 *  Fired once when health reaches zero.
 *  체력이 0이 되는 순간 한 번만 발생합니다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeathSignature, AActor*, DeadActor, AActor*, Killer);

/**
 *  Minimal health container shared by the player character and the AI enemies.
 *  Also listens to the engine damage pipeline so ApplyDamage() based sources work too.
 *
 *  플레이어 캐릭터와 AI가 공유하는 최소한의 체력 보관소입니다.
 *  엔진 데미지 파이프라인도 함께 구독하므로 ApplyDamage() 계열도 그대로 들어옵니다.
 */
UCLASS(ClassGroup = (CQB), meta = (BlueprintSpawnableComponent))
class PROJECTTF_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHealthComponent();

	/**
	 *  Maximum health granted on BeginPlay
	 *  BeginPlay에서 채워지는 최대 체력
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.0f;

	/**
	 *  Current health. Replicated-free, this is a single player sample.
	 *  현재 체력. 싱글 플레이 샘플이라 리플리케이션은 없습니다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.0f;

	/**
	 *  Broadcast on every health change
	 *  체력이 바뀔 때마다 브로드캐스트
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChangedSignature OnHealthChanged;

	/**
	 *  Broadcast once when this actor dies
	 *  이 액터가 죽을 때 한 번 브로드캐스트
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathSignature OnDeath;

	/**
	 *  Applies damage. Returns the amount actually applied.
	 *  피해를 적용하고, 실제로 들어간 양을 돌려줍니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float TakeDamage(float DamageAmount, AActor* DamageCauser = nullptr, AController* EventInstigator = nullptr);

	/**
	 *  Restores health up to MaxHealth. Ignored once dead.
	 *  MaxHealth까지 회복합니다. 이미 죽었으면 무시됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float HealAmount);

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	/**
	 *  Convenience: returns the health component of an actor, or null
	 *  편의 함수: 액터의 체력 컴포넌트를 돌려주고, 없으면 null
	 */
	static UHealthComponent* FindHealthComponent(AActor* Actor);

protected:

	virtual void BeginPlay() override;

	/**
	 *  Bridges AActor::TakeDamage / UGameplayStatics::ApplyDamage into this component
	 *  AActor::TakeDamage / UGameplayStatics::ApplyDamage를 이 컴포넌트로 연결합니다
	 */
	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/**
	 *  True once health has hit zero, so death only fires once
	 *  체력이 0이 된 뒤 true. 사망이 한 번만 발생하도록 막아줍니다
	 */
	bool bIsDead = false;
};
