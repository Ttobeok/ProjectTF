// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectTFCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "ProjectTF.h"
#include "CQB/HealthComponent.h"
#include "CQB/WeaponComponent.h"

AProjectTFCharacter::AProjectTFCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// CQB gameplay components, shared with the AI enemies
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health Component"));

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("Weapon Component"));
	WeaponComponent->AimSpreadHalfAngle = 0.25f;
	WeaponComponent->bApplyRecoilToController = true;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// make sure enemy bullets and AI sight traces can hit the player
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void AProjectTFCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &AProjectTFCharacter::OnHealthChanged);
		HealthComponent->OnDeath.AddDynamic(this, &AProjectTFCharacter::OnPlayerDeath);
	}
}

void AProjectTFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// blend the lean towards the requested side
	CurrentLeanRoll = FMath::FInterpTo(CurrentLeanRoll, LeanTarget * LeanRollAngle, DeltaSeconds, LeanInterpSpeed);
	CurrentLeanOffset = FMath::FInterpTo(CurrentLeanOffset, LeanTarget * LeanOffsetDistance, DeltaSeconds, LeanInterpSpeed);
}

void AProjectTFCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AProjectTFCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AProjectTFCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProjectTFCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProjectTFCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AProjectTFCharacter::LookInput);

		// CQB actions. These Input Actions are optional; see the direct key bindings below.
		if (CQBFireAction)
		{
			EnhancedInputComponent->BindAction(CQBFireAction, ETriggerEvent::Started, this, &AProjectTFCharacter::DoFireStart);
			EnhancedInputComponent->BindAction(CQBFireAction, ETriggerEvent::Completed, this, &AProjectTFCharacter::DoFireStop);
		}

		if (CQBADSAction)
		{
			EnhancedInputComponent->BindAction(CQBADSAction, ETriggerEvent::Started, this, &AProjectTFCharacter::DoADSStart);
			EnhancedInputComponent->BindAction(CQBADSAction, ETriggerEvent::Completed, this, &AProjectTFCharacter::DoADSStop);
		}

		if (CQBReloadAction)
		{
			EnhancedInputComponent->BindAction(CQBReloadAction, ETriggerEvent::Started, this, &AProjectTFCharacter::DoReload);
		}

		if (CQBLeanLeftAction)
		{
			EnhancedInputComponent->BindAction(CQBLeanLeftAction, ETriggerEvent::Started, this, &AProjectTFCharacter::LeanLeftStart);
			EnhancedInputComponent->BindAction(CQBLeanLeftAction, ETriggerEvent::Completed, this, &AProjectTFCharacter::LeanStop);
		}

		if (CQBLeanRightAction)
		{
			EnhancedInputComponent->BindAction(CQBLeanRightAction, ETriggerEvent::Started, this, &AProjectTFCharacter::LeanRightStart);
			EnhancedInputComponent->BindAction(CQBLeanRightAction, ETriggerEvent::Completed, this, &AProjectTFCharacter::LeanStop);
		}
	}
	else
	{
		UE_LOG(LogProjectTF, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	// Direct key bindings so the CQB controls work without authoring any Input Action assets.
	// Assign the matching Input Action above to take over a binding.
	if (PlayerInputComponent)
	{
		if (!CQBFireAction)
		{
			PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AProjectTFCharacter::DoFireStart);
			PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AProjectTFCharacter::DoFireStop);
		}

		if (!CQBADSAction)
		{
			PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AProjectTFCharacter::DoADSStart);
			PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AProjectTFCharacter::DoADSStop);
		}

		if (!CQBReloadAction)
		{
			PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AProjectTFCharacter::DoReload);
		}

		if (!CQBLeanLeftAction)
		{
			PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AProjectTFCharacter::LeanLeftStart);
			PlayerInputComponent->BindKey(EKeys::Q, IE_Released, this, &AProjectTFCharacter::LeanStop);
		}

		if (!CQBLeanRightAction)
		{
			PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AProjectTFCharacter::LeanRightStart);
			PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &AProjectTFCharacter::LeanStop);
		}
	}
}


void AProjectTFCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AProjectTFCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AProjectTFCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AProjectTFCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AProjectTFCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void AProjectTFCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void AProjectTFCharacter::DoFireStart()
{
	if (WeaponComponent)
	{
		WeaponComponent->StartFire();
	}
}

void AProjectTFCharacter::DoFireStop()
{
	if (WeaponComponent)
	{
		WeaponComponent->StopFire();
	}
}

void AProjectTFCharacter::DoADSStart()
{
	if (WeaponComponent)
	{
		WeaponComponent->StartADS();
	}
}

void AProjectTFCharacter::DoADSStop()
{
	if (WeaponComponent)
	{
		WeaponComponent->StopADS();
	}
}

void AProjectTFCharacter::DoReload()
{
	if (WeaponComponent)
	{
		WeaponComponent->Reload();
	}
}

void AProjectTFCharacter::DoLean(float Direction)
{
	LeanTarget = FMath::Clamp(Direction, -1.0f, 1.0f);
}

void AProjectTFCharacter::OnHealthChanged(UHealthComponent* HealthComp, float NewHealth, float Delta, AActor* Causer)
{
	if (Delta < 0.0f && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9001, 2.0f, FColor::Red,
			FString::Printf(TEXT("TAKING FIRE from %s   HP %.0f/%.0f"), *GetNameSafe(Causer), NewHealth, HealthComp->MaxHealth));
	}
}

void AProjectTFCharacter::OnPlayerDeath(AActor* DeadActor, AActor* Killer)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9002, 10.0f, FColor::Red, TEXT("YOU ARE DOWN"));
	}

	// stop shooting and hand back control
	if (WeaponComponent)
	{
		WeaponComponent->StopFire();
	}

	if (AController* MyController = GetController())
	{
		DisableInput(Cast<APlayerController>(MyController));
	}

	GetCharacterMovement()->DisableMovement();
}
