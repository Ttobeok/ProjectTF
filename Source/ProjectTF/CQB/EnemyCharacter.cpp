// CQB Sample - AI enemy pawn.

#include "EnemyCharacter.h"
#include "CQBSightTarget.h"
#include "HealthComponent.h"
#include "WeaponComponent.h"
#include "WeaponData.h"
#include "EnemyAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WeaponVisualComponent.h"
#include "NavigationInvokerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

AEnemyCharacter::AEnemyCharacter()
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
	HealthComponent->MaxHealth = EnemyMaxHealth;

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("Weapon Component"));
	// the AI controller owns the aim, so recoil must not fight it, and only the player HUD gets messages
	WeaponComponent->bApplyRecoilToController = false;
	WeaponComponent->bShowDebugMessages = false;
	WeaponComponent->AimSpreadHalfAngle = EnemyAimSpreadHalfAngle;

	// face the direction the AI controller is aiming at
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 380.0f;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AEnemyCharacter::BeginPlay()
{
	// Build the enemy weapon profile before Super::BeginPlay, because the weapon component
	// reads its magazine size during its own BeginPlay, which Super dispatches.
	if (WeaponComponent && !WeaponComponent->WeaponData)
	{
		UWeaponData* EnemyWeapon = NewObject<UWeaponData>(this, UWeaponData::StaticClass(), TEXT("EnemyWeaponData"));
		EnemyWeapon->WeaponName = FName("EnemyRifle");
		EnemyWeapon->Damage = EnemyWeaponDamage;
		EnemyWeapon->FireRate = EnemyWeaponFireRate;
		EnemyWeapon->MagSize = 30;
		EnemyWeapon->ReloadTime = 2.5f;
		EnemyWeapon->bAutomatic = true;

		WeaponComponent->WeaponData = EnemyWeapon;
		WeaponComponent->AimSpreadHalfAngle = EnemyAimSpreadHalfAngle;
	}

	if (HealthComponent)
	{
		HealthComponent->MaxHealth = EnemyMaxHealth;
		HealthComponent->OnDeath.AddDynamic(this, &AEnemyCharacter::OnEnemyDeath);
	}

	Super::BeginPlay();

	// Loaded here, never from the constructor: pulling an animation blueprint in during class
	// default object construction can deadlock the async loader.
	if (UClass* BodyAnim = BodyAnimAsset.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(BodyAnim);
	}
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DestroyTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemyCharacter::OnEnemyDeath(AActor* DeadActor, AActor* Killer)
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

	if (bRagdollOnDeath)
	{
		GetMesh()->SetCollisionProfileName(FName("Ragdoll"));
		GetMesh()->SetAllBodiesSimulatePhysics(true);
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->WakeAllRigidBodies();
	}
	else
	{
		DeferredDestructionTime = 0.0f;
	}

	if (UWorld* World = GetWorld())
	{
		if (DeferredDestructionTime > 0.0f)
		{
			World->GetTimerManager().SetTimer(DestroyTimerHandle, this, &AEnemyCharacter::DeferredDestroy, DeferredDestructionTime, false);
		}
		else
		{
			DeferredDestroy();
		}
	}
}

void AEnemyCharacter::DeferredDestroy()
{
	Destroy();
}

UAISense_Sight::EVisibilityResult AEnemyCharacter::CanBeSeenFrom(const FCanBeSeenFromContext& Context,
	FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested,
	float& OutSightStrength, int32* UserData, const FOnPendingVisibilityQueryProcessedDelegate* Delegate)
{
	OutNumberOfLoSChecksPerformed = 0;
	OutNumberOfAsyncLosCheckRequested = 0;

	const bool bSeen = CQBSightTarget::CanBeSeenFrom(*this, Context.ObserverLocation, Context.IgnoreActor,
		OutSeenLocation, OutNumberOfLoSChecksPerformed, OutSightStrength);

	return bSeen ? UAISense_Sight::EVisibilityResult::Visible : UAISense_Sight::EVisibilityResult::NotVisible;
}
