// CQB Sample - AI enemy pawn.
// CQB 샘플 - AI가 조종하는 폰.

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
 *
 *  양쪽 AI가 공통으로 올라서는 폰입니다.
 *
 *  플레이어와 같은 체력·무기·비주얼 컴포넌트를 들고, 죽으면 래그돌이 되며, 설득해서
 *  항복시킬 수 있습니다. 자기 편은 따로 없습니다 — AEnemyCharacter와 AAllyCharacter가
 *  진영과 컨트롤러만 지정하는 얇은 하위 클래스이고, 위협과 분대원을 가르는 건 그것뿐입니다.
 */
UCLASS()
class PROJECTTF_API ACQBCharacter : public ACharacter, public ICQBFactionAgent, public IAISightTargetInterface
{
	GENERATED_BODY()

	/**
	 *  Health pool shared with the player character
	 *  플레이어 캐릭터와 공유하는 체력
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UHealthComponent* HealthComponent;

	/**
	 *  Hitscan weapon shared with the player character
	 *  플레이어 캐릭터와 공유하는 히트스캔 무기
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWeaponComponent* WeaponComponent;

	/**
	 *  The weapon you can see, on the hand socket
	 *  손 소켓에 붙어 눈에 보이는 무기
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWeaponVisualComponent* WeaponVisual;

	/**
	 *  Keeps navmesh tiles generated around this character at runtime
	 *  런타임에 이 캐릭터 주변 navmesh 타일이 유지되도록 합니다
	 */
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

	/**
	 *  Gave up: weapon down, no longer a threat. Anyone can ask, the controller sets it.
	 *  항복 여부. 무기를 내렸고 더 이상 위협이 아닙니다. 조회는 누구나, 설정은 컨트롤러가 합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "CQB")
	bool IsSurrendered() const { return bSurrendered; }

	/**
	 *  Puts the weapon away and drops into a kneel. Called by the AI controller.
	 *  무기를 치우고 무릎을 꿇립니다. AI 컨트롤러가 호출합니다.
	 */
	void SetSurrendered(bool bInSurrendered);

	/**
	 *  Tints the whole body. The squad uses it to wear its element colour, so a glance at a
	 *  figure in a doorway says whose it is before the name over its head is readable.
	 *
	 *  몸 전체에 색을 입힙니다. 분대가 이걸로 element 색 옷을 입으므로, 문간에 선 실루엣이
	 *  누구 편인지 머리 위 이름을 읽기 전에 알 수 있습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "CQB")
	void SetBodyTint(FLinearColor Tint);

	/**
	 *  Vector parameter on the body material that SetBodyTint writes. M_Mannequin calls it this.
	 *  SetBodyTint가 쓰는 몸 머티리얼의 벡터 파라미터 이름. M_Mannequin에서는 이 이름입니다.
	 */
	UPROPERTY(EditAnywhere, Category = "CQB")
	FName BodyTintParameter = FName("Paint Tint");

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

	/**
	 *  Ragdolls or destroys the pawn and lets the AI controller and squad know
	 *  폰을 래그돌로 만들거나 파괴하고, AI 컨트롤러와 분대에 알립니다
	 */
	UFUNCTION()
	void OnCharacterDeath(AActor* DeadActor, AActor* Killer);

	/**
	 *  Removes the corpse
	 *  시체를 치웁니다
	 */
	void DeferredDestroy();

	/**
	 *  Side this character fights for. Subclasses set it.
	 *  이 캐릭터가 싸우는 편. 하위 클래스가 지정합니다.
	 */
	UPROPERTY(EditAnywhere, Category = "CQB")
	ECQBFaction Faction = ECQBFaction::Neutral;

	/**
	 *  Ragdoll on death when true, plain Destroy otherwise
	 *  true면 사망 시 래그돌, 아니면 그냥 Destroy
	 */
	UPROPERTY(EditAnywhere, Category = "CQB")
	bool bRagdollOnDeath = true;

	/**
	 *  Seconds a body lies there before it is removed. Zero or less leaves it for good, which is
	 *  the default: a room you have cleared should look like it, and the bodies are how the
	 *  player reads what happened while they were somewhere else.
	 *
	 *  시체가 치워지기까지의 시간(초). 0 이하면 계속 남으며, 그게 기본값입니다.
	 *  정리한 방은 정리한 것처럼 보여야 하고, 자리를 비운 사이 무슨 일이 있었는지
	 *  플레이어가 읽는 수단이 시체이기 때문입니다.
	 */
	UPROPERTY(EditAnywhere, Category = "CQB")
	float DeferredDestructionTime = 0.0f;

	/**
	 *  Starting health for enemies
	 *  적의 시작 체력
	 */
	UPROPERTY(EditAnywhere, Category = "CQB")
	float MaxHealth = 60.0f;

	/**
	 *  Damage per enemy bullet
	 *  적 탄 한 발당 피해량
	 */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	float WeaponDamage = 7.0f;

	/**
	 *  Enemy shots per second
	 *  적의 초당 발사 수
	 */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	float WeaponFireRate = 2.5f;

	/**
	 *  Rifle holding pose for the body. Soft on purpose: it lives in the shooter variant, and a
	 *  missing asset should leave the enemy in its default pose rather than break the build.
	 *
	 *  몸의 라이플 파지 포즈. 일부러 소프트 참조입니다 — 슈터 variant에 있는 에셋이라,
	 *  없더라도 빌드가 깨지는 대신 기본 포즈로 서 있게 하려는 것입니다.
	 */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	TSoftClassPtr<UAnimInstance> BodyAnimAsset = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C")));

	/**
	 *  Half angle of the enemy aim cone, in degrees. Keeps them from being laser accurate.
	 *  적 탄 퍼짐 원뿔의 반각(도). 레이저처럼 정확해지지 않게 막아줍니다.
	 */
	UPROPERTY(EditAnywhere, Category = "CQB|Weapon")
	float AimSpreadHalfAngle = 4.0f;

	bool bIsDead = false;

	bool bSurrendered = false;

	FTimerHandle DestroyTimerHandle;
};
