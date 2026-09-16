// CQB Sample - AI enemy pawn.
// CQB 샘플 - AI가 조종하는 폰.

#include "CQBCharacter.h"
#include "CQBSightTarget.h"
#include "HealthComponent.h"
#include "WeaponComponent.h"
#include "WeaponData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WeaponVisualComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProjectTF.h"
#include "Engine/World.h"
#include "TimerManager.h"

ACQBCharacter::ACQBCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

	// bullets and AI sight traces must be able to hit enemies
	// 탄과 AI 시야 트레이스가 적에게 닿아야 합니다
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// grab the template mannequin so the class is usable without making a Blueprint first
	// 블루프린트를 먼저 만들지 않아도 쓸 수 있도록 템플릿 마네킹을 가져옵니다
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
	}

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));

	// the visible rifle, on the hand the template animations grip with
	// 템플릿 애니메이션이 쥐는 손에 붙는, 눈에 보이는 라이플
	WeaponVisual = CreateDefaultSubobject<UWeaponVisualComponent>(TEXT("Weapon Visual"));
	WeaponVisual->SetupAttachment(GetMesh(), FName("HandGrip_R"));
	WeaponVisual->AttachMode = EWeaponAttachMode::HandSocket;

	// gameplay components, the same ones the player uses
	// 게임플레이 컴포넌트. 플레이어가 쓰는 것과 같습니다
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health Component"));
	HealthComponent->MaxHealth = MaxHealth;

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("Weapon Component"));
	// the AI controller owns the aim, so recoil must not fight it, and only the player HUD gets messages
	// 조준은 AI 컨트롤러가 쥐고 있으므로 반동이 이를 방해하면 안 되고, 메시지는 플레이어 HUD에만 갑니다
	WeaponComponent->bApplyRecoilToController = false;
	WeaponComponent->bShowDebugMessages = false;
	WeaponComponent->bDrawDebugTrace = false;
	WeaponComponent->AimSpreadHalfAngle = AimSpreadHalfAngle;

	// face the direction the AI controller is aiming at
	// AI 컨트롤러가 조준하는 방향을 바라보게 합니다
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;

	// 540 deg/s spins a pawn to face a new heading almost instantly, which reads as a turret
	// rather than a person. Slower, so a change of direction costs something to watch.
	// 540도/초는 새 방향으로 거의 즉시 홱 돌아서, 사람이 아니라 포탑처럼 보입니다.
	// 방향 전환에 시간이 들도록 늦춥니다.
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 300.0f, 0.0f);

	// 2.3 m/s: a purposeful walk. Room to room in a building nobody runs, and at the 3.8 m/s
	// this used to be, a suspect crossed an 8 m room in two seconds and the fight was over
	// before the player had read it.
	// 2.3m/s: 목적 있는 걸음. 건물 안에서 방을 옮길 때 뛰는 사람은 없고, 이전 값인 3.8m/s에서는
	// 용의자가 8m 방을 2초에 가로질러 플레이어가 상황을 읽기도 전에 교전이 끝났습니다.
	GetCharacterMovement()->MaxWalkSpeed = 230.0f;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ACQBCharacter::BeginPlay()
{
	// Build the enemy weapon profile before Super::BeginPlay, because the weapon component
	// reads its magazine size during its own BeginPlay, which Super dispatches.
	// Super::BeginPlay보다 먼저 적 무기 프로파일을 만듭니다. 무기 컴포넌트가 자기 BeginPlay에서
	// 탄창 크기를 읽는데, 그 BeginPlay를 Super가 호출하기 때문입니다.
	if (WeaponComponent && !WeaponComponent->WeaponData)
	{
		UWeaponData* EnemyWeapon = NewObject<UWeaponData>(this, UWeaponData::StaticClass(), TEXT("EnemyWeaponData"));
		EnemyWeapon->WeaponName = FName("AIRifle");
		EnemyWeapon->Damage = WeaponDamage;
		EnemyWeapon->FireRate = WeaponFireRate;
		EnemyWeapon->MagSize = 30;
		EnemyWeapon->ReloadTime = 2.5f;
		EnemyWeapon->bAutomatic = true;

		WeaponComponent->WeaponData = EnemyWeapon;
	}

	// Spread belongs to the shooter, not to the weapon profile, so it is applied whether or not
	// a WeaponData asset was assigned. Inside the block above it was a dead knob for any pawn
	// that had one.
	//
	// 탄 퍼짐은 무기 프로파일이 아니라 사수의 것이므로, WeaponData 에셋 유무와 무관하게
	// 적용합니다. 위 블록 안에 있을 때는 에셋을 지정한 폰에서 동작하지 않는 값이었습니다.
	if (WeaponComponent)
	{
		WeaponComponent->AimSpreadHalfAngle = AimSpreadHalfAngle;
	}

	if (HealthComponent)
	{
		HealthComponent->MaxHealth = MaxHealth;
		HealthComponent->OnDeath.AddDynamic(this, &ACQBCharacter::OnCharacterDeath);
	}

	Super::BeginPlay();

	// Loaded here, never from the constructor: pulling an animation blueprint in during class
	// default object construction can deadlock the async loader.
	// 생성자가 아니라 여기서 로드합니다. 클래스 기본 오브젝트를 만드는 중에 애님 블루프린트를
	// 끌어오면 async 로더가 데드락에 빠질 수 있습니다.
	if (UClass* BodyAnim = BodyAnimAsset.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(BodyAnim);
	}
	else
	{
		// a packaged build reaches here when the asset was not cooked - see the
		// DirectoriesToAlwaysCook entries in DefaultGame.ini
		// 에셋이 쿡되지 않은 패키지 빌드가 여기로 옵니다 — DefaultGame.ini의
		// DirectoriesToAlwaysCook 항목을 보세요
		UE_LOG(LogProjectTF, Warning, TEXT("CQB: body animation %s could not be loaded, the pawn will not animate"),
			*BodyAnimAsset.ToString());
	}
}

void ACQBCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DestroyTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ACQBCharacter::SetBodyTint(FLinearColor Tint)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return;
	}

	const int32 SlotCount = MeshComponent->GetNumMaterials();
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		UMaterialInstanceDynamic* Dynamic = MeshComponent->CreateAndSetMaterialInstanceDynamic(Slot);
		if (!Dynamic)
		{
			continue;
		}

		FLinearColor Existing;
		if (!Dynamic->GetVectorParameterValue(FMaterialParameterInfo(BodyTintParameter), Existing))
		{
			// setting a parameter the material has never heard of does nothing and says nothing,
			// so say it here rather than wonder later why everyone is still grey
			// 머티리얼이 모르는 파라미터를 설정하면 아무 일도 안 일어나고 아무 말도 없습니다.
			// 나중에 왜 다들 회색인지 고민하지 않도록 여기서 말해 둡니다
			UE_LOG(LogProjectTF, Warning, TEXT("CQB: %s has no '%s' parameter on material %d, tint ignored"),
				*GetName(), *BodyTintParameter.ToString(), Slot);
			continue;
		}

		Dynamic->SetVectorParameterValue(BodyTintParameter, Tint);
	}
}

void ACQBCharacter::OnCharacterDeath(AActor* DeadActor, AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	if (WeaponComponent)
	{
		WeaponComponent->StopFire();
	}

	// stop moving and let the corpse be shot through
	// 움직임을 멈추고, 시체는 관통되도록 둡니다
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (!bRagdollOnDeath)
	{
		// Nothing worth looking at, so do not leave an empty capsule standing about - but go
		// away next tick rather than now. This runs inside OnDeath, which runs inside
		// ApplyPointDamage, which runs inside UWeaponComponent::Fire; destroying here hands
		// Fire a pending-kill actor to finish reading.
		//
		// 볼 것도 없으니 빈 캡슐을 세워두지 않습니다. 다만 지금이 아니라 다음 틱에 사라집니다.
		// 이 코드는 OnDeath 안이고, OnDeath는 ApplyPointDamage 안이며, 그것은
		// UWeaponComponent::Fire 안입니다. 여기서 파괴하면 Fire가 이미 죽은 액터를 계속
		// 읽게 됩니다.
		SetLifeSpan(0.01f);
		return;
	}

	// the Ragdoll profile ignores the visibility channel, so a body neither blocks the AI's
	// line of sight nor stops a shot: it lies there and is walked around
	// Ragdoll 프로파일은 visibility 채널을 무시하므로, 시체는 AI 시야도 탄도 막지 않습니다.
	// 그냥 누워 있고, 사람들은 돌아서 지나갑니다
	GetMesh()->SetCollisionProfileName(FName("Ragdoll"));
	GetMesh()->SetAllBodiesSimulatePhysics(true);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->WakeAllRigidBodies();

	if (DeferredDestructionTime > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(DestroyTimerHandle, this, &ACQBCharacter::DeferredDestroy, DeferredDestructionTime, false);
		}
	}
}

void ACQBCharacter::SetSurrendered(bool bInSurrendered)
{
	if (bSurrendered == bInSurrendered || bIsDead)
	{
		return;
	}

	bSurrendered = bInSurrendered;

	if (WeaponComponent)
	{
		WeaponComponent->StopFire();
	}

	// the weapon goes away, which is the whole point of a surrender
	// 무기를 치웁니다. 항복의 핵심이 이것입니다
	if (WeaponVisual)
	{
		WeaponVisual->SetVisibility(!bSurrendered, true);
	}

	// Crouch reads as hands up well enough without an animation for it. The capsule has to come
	// down with the mesh: dropping the mesh alone put the feet through the floor and left the
	// hitbox standing at full height, so a surrendered suspect could be killed by shooting the
	// empty air half a metre above his head.
	//
	// 전용 애니메이션 없이도 앉은 자세면 손 든 것으로 충분히 읽힙니다. 다만 캡슐도 같이
	// 내려와야 합니다. 메시만 내렸더니 발이 바닥을 뚫고, 히트박스는 선 키 그대로 남아서,
	// 항복한 용의자를 머리 위 50cm 빈 공중을 쏴서 죽일 수 있었습니다.
	const float StandingHalfHeight = GetClass()->GetDefaultObject<ACQBCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float TargetHalfHeight = bSurrendered ? SurrenderedCapsuleHalfHeight : StandingHalfHeight;

	GetCapsuleComponent()->SetCapsuleHalfHeight(TargetHalfHeight, true);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -TargetHalfHeight));

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();

		if (bSurrendered)
		{
			Movement->DisableMovement();
		}
		else
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

void ACQBCharacter::DeferredDestroy()
{
	Destroy();
}

UAISense_Sight::EVisibilityResult ACQBCharacter::CanBeSeenFrom(const FCanBeSeenFromContext& Context,
	FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested,
	float& OutSightStrength, int32* UserData, const FOnPendingVisibilityQueryProcessedDelegate* Delegate)
{
	OutNumberOfLoSChecksPerformed = 0;
	OutNumberOfAsyncLosCheckRequested = 0;

	const bool bSeen = CQBSightTarget::CanBeSeenFrom(*this, Context.ObserverLocation, Context.IgnoreActor,
		OutSeenLocation, OutNumberOfLoSChecksPerformed, OutSightStrength);

	return bSeen ? UAISense_Sight::EVisibilityResult::Visible : UAISense_Sight::EVisibilityResult::NotVisible;
}
