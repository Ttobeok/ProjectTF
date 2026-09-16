// CQB Sample - AI enemy pawn.

#include "CQBCharacter.h"
#include "CQBSightTarget.h"
#include "HealthComponent.h"
#include "WeaponComponent.h"
#include "WeaponData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WeaponVisualComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NavigationInvokerComponent.h"
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
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// grab the template mannequin so the class is usable without making a Blueprint first
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
	}

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));

	// the visible rifle, on the hand the template animations grip with
	WeaponVisual = CreateDefaultSubobject<UWeaponVisualComponent>(TEXT("Weapon Visual"));
	WeaponVisual->SetupAttachment(GetMesh(), FName("HandGrip_R"));
	WeaponVisual->AttachMode = EWeaponAttachMode::HandSocket;

	// gameplay components, the same ones the player uses
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health Component"));
	HealthComponent->MaxHealth = MaxHealth;

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("Weapon Component"));
	// the AI controller owns the aim, so recoil must not fight it, and only the player HUD gets messages
	WeaponComponent->bApplyRecoilToController = false;
	WeaponComponent->bShowDebugMessages = false;
	WeaponComponent->AimSpreadHalfAngle = AimSpreadHalfAngle;

	// face the direction the AI controller is aiming at
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;

	// 540 deg/s spins a pawn to face a new heading almost instantly, which reads as a turret
	// rather than a person. Slower, so a change of direction costs something to watch.
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 300.0f, 0.0f);

	// 2.3 m/s: a purposeful walk. Room to room in a building nobody runs, and at the 3.8 m/s
	// this used to be, a suspect crossed an 8 m room in two seconds and the fight was over
	// before the player had read it.
	GetCharacterMovement()->MaxWalkSpeed = 230.0f;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ACQBCharacter::BeginPlay()
{
	// Build the enemy weapon profile before Super::BeginPlay, because the weapon component
	// reads its magazine size during its own BeginPlay, which Super dispatches.
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
	if (UClass* BodyAnim = BodyAnimAsset.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(BodyAnim);
	}
	else
	{
		// a packaged build reaches here when the asset was not cooked - see the
		// DirectoriesToAlwaysCook entries in DefaultGame.ini
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
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (!bRagdollOnDeath)
	{
		// nothing worth looking at, so do not leave an empty capsule standing about
		DeferredDestroy();
		return;
	}

	// the Ragdoll profile ignores the visibility channel, so a body neither blocks the AI's
	// line of sight nor stops a shot: it lies there and is walked around
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
	if (WeaponVisual)
	{
		WeaponVisual->SetVisibility(!bSurrendered, true);
	}

	// crouch reads as hands up well enough without an animation for it
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();

		if (bSurrendered)
		{
			Movement->DisableMovement();
			GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -150.0f));
		}
		else
		{
			Movement->SetMovementMode(MOVE_Walking);
			GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
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
