// CQB Sample - the weapon you can see. Gameplay lives in UWeaponComponent.
// CQB 샘플 - 눈에 보이는 무기. 게임플레이는 UWeaponComponent에 있습니다.

#include "WeaponVisualComponent.h"
#include "WeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "ProjectTF.h"

UWeaponVisualComponent::UWeaponVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetCollisionProfileName(FName("NoCollision"));
	SetGenerateOverlapEvents(false);

	// the rifle that ships with the template
	// 템플릿에 딸려오는 라이플
	WeaponMeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle")));
}

void UWeaponVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	// Loaded here rather than from a constructor. Pulling content in while the class default
	// object is still being built can deadlock the async loader.
	// 생성자가 아니라 여기서 로드합니다. 클래스 기본 오브젝트가 만들어지는 중에 콘텐츠를
	// 끌어오면 async 로더가 데드락에 빠질 수 있습니다.
	if (USkeletalMesh* Mesh = WeaponMeshAsset.LoadSynchronous())
	{
		SetSkeletalMesh(Mesh);
	}
	else
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB: weapon mesh %s could not be loaded"), *WeaponMeshAsset.ToString());
	}

	AttachToOwner();

	// kick the weapon on every shot
	// 발사할 때마다 무기를 튕겨줍니다
	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>())
		{
			Weapon->OnAmmoChanged.AddUniqueDynamic(this, &UWeaponVisualComponent::OnAmmoChanged);
		}
	}
}

FVector UWeaponVisualComponent::GetMuzzleLocation(const FVector& AimDirection) const
{
	if (DoesSocketExist(MuzzleSocket))
	{
		return GetSocketLocation(MuzzleSocket);
	}

	return GetComponentLocation() + AimDirection.GetSafeNormal() * MuzzleForwardOffset;
}

void UWeaponVisualComponent::AttachToOwner()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, false);

	if (AttachMode == EWeaponAttachMode::Camera)
	{
		UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
		if (!Camera)
		{
			UE_LOG(LogProjectTF, Warning, TEXT("CQB: %s has no camera, the weapon cannot be a view model"), *Owner->GetName());
			return;
		}

		AttachToComponent(Camera, Rules);
		SetRelativeLocationAndRotation(ViewOffset, ViewRotation);

		// first person only: the world sees the weapon on the third person body instead
		// 1인칭 전용입니다. 월드에서는 3인칭 몸에 붙은 무기가 보입니다
		SetOnlyOwnerSee(true);
		FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		SetCastShadow(false);
		return;
	}

	// hand socket: the weapon belongs to the body everyone else sees
	// 손 소켓: 이 무기는 남들이 보는 몸에 속합니다
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(Owner))
	{
		AttachToComponent(OwnerCharacter->GetMesh(), Rules, HandSocket);
	}
}

void UWeaponVisualComponent::OnAmmoChanged(int32 CurrentAmmo, int32 MagSize)
{
	// a shot lowers the count; a finished reload fills it, and should not kick
	// 발사는 탄약을 줄이고 재장전 완료는 채웁니다. 후자는 반동이 없어야 합니다
	if (CurrentAmmo < MagSize)
	{
		AddFireKick();
	}
}

void UWeaponVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// only the view model animates its offset; a weapon on a hand socket follows the animation
	// 오프셋을 애니메이션하는 건 뷰모델뿐입니다. 손 소켓의 무기는 애님을 따라갑니다
	if (AttachMode != EWeaponAttachMode::Camera || KickAlpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	KickAlpha = FMath::FInterpTo(KickAlpha, 0.0f, DeltaTime, KickRecoverySpeed);

	const FVector KickedLocation = ViewOffset - FVector(KickDistance * KickAlpha, 0.0f, 0.0f);
	const FRotator KickedRotation = ViewRotation + FRotator(KickPitch * KickAlpha, 0.0f, 0.0f);

	SetRelativeLocationAndRotation(KickedLocation, KickedRotation);
}
