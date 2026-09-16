// CQB Sample - shared health component for player and enemies.
// CQB 샘플 - 플레이어와 적이 함께 쓰는 체력 컴포넌트.

#include "HealthComponent.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDead = false;

	// route engine damage (explosions, kill volumes, ApplyDamage) through this component too
	// 엔진 쪽 피해(폭발, 킬 볼륨, ApplyDamage)도 이 컴포넌트를 거치도록 연결합니다
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleAnyDamage);
	}
}

void UHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	TakeDamage(Damage, DamageCauser, InstigatedBy);
}

float UHealthComponent::TakeDamage(float DamageAmount, AActor* DamageCauser, AController* EventInstigator)
{
	if (bIsDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	const float Applied = OldHealth - CurrentHealth;
	OnHealthChanged.Broadcast(this, CurrentHealth, -Applied, DamageCauser);

	if (CurrentHealth <= 0.0f)
	{
		bIsDead = true;
		OnDeath.Broadcast(GetOwner(), DamageCauser);
	}

	return Applied;
}

float UHealthComponent::Heal(float HealAmount)
{
	if (bIsDead || HealAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + HealAmount);

	const float Applied = CurrentHealth - OldHealth;
	if (Applied > 0.0f)
	{
		OnHealthChanged.Broadcast(this, CurrentHealth, Applied, nullptr);
	}

	return Applied;
}

UHealthComponent* UHealthComponent::FindHealthComponent(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
}
