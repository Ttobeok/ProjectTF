// CQB Sample - the weapon you can see. Gameplay lives in UWeaponComponent.

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
	WeaponMeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle")));
}

void UWeaponVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	// Loaded here rather than from a constructor. Pulling content in while the class default
	// object is still being built can deadlock the async loader.
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
	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* Weapon = Owner->FindComponentByClass<UWeaponComponent>())
		{
			Weapon->OnAmmoChanged.AddUniqueDynamic(this, &UWeaponVisualComponent::OnAmmoChanged);
		}
	}
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
		SetOnlyOwnerSee(true);
		FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		SetCastShadow(false);
		return;
	}

	// hand socket: the weapon belongs to the body everyone else sees
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(Owner))
	{
		AttachToComponent(OwnerCharacter->GetMesh(), Rules, HandSocket);
	}
}

void UWeaponVisualComponent::OnAmmoChanged(int32 CurrentAmmo, int32 MagSize)
{
	// a shot lowers the count; a finished reload fills it, and should not kick
	if (CurrentAmmo < MagSize)
	{
		AddFireKick();
	}
}

void UWeaponVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// only the view model animates its offset; a weapon on a hand socket follows the animation
	if (AttachMode != EWeaponAttachMode::Camera || KickAlpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	KickAlpha = FMath::FInterpTo(KickAlpha, 0.0f, DeltaTime, KickRecoverySpeed);

	const FVector KickedLocation = ViewOffset - FVector(KickDistance * KickAlpha, 0.0f, 0.0f);
	const FRotator KickedRotation = ViewRotation + FRotator(KickPitch * KickAlpha, 0.0f, 0.0f);

	SetRelativeLocationAndRotation(KickedLocation, KickedRotation);
}
