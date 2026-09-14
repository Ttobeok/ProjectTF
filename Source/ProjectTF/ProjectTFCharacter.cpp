// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectTFCharacter.h"
#include "CQB/CQBSightTarget.h"
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
#include "CQB/WeaponVisualComponent.h"
#include "CQB/DoorwayMarker.h"
#include "CQB/AllyAIController.h"
#include "CQB/EnemyAIController.h"
#include "CQB/EnemyCharacter.h"
#include "Perception/AISense_Hearing.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationInvokerComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"

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

	// the visible weapon, riding the camera. It attaches itself on BeginPlay.
	WeaponVisual = CreateDefaultSubobject<UWeaponVisualComponent>(TEXT("Weapon Visual"));
	WeaponVisual->SetupAttachment(FirstPersonCameraComponent);
	WeaponVisual->AttachMode = EWeaponAttachMode::Camera;

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

	// debug hook: drive the squad from the command line for headless checks
	float OrderAfter = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CQBOrderAfter="), OrderAfter) && OrderAfter > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ScriptedOrderTimer, this, &AProjectTFCharacter::RunScriptedOrder, OrderAfter, false);
	}

	// The arms are not animated to hold anything, so they would be empty handed next to a
	// weapon that rides the camera. Hiding them keeps the view honest.
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(false, false);
	}
}

void AProjectTFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// blend the lean towards the requested side
	CurrentLeanRoll = FMath::FInterpTo(CurrentLeanRoll, LeanTarget * LeanRollAngle, DeltaSeconds, LeanInterpSpeed);
	CurrentLeanOffset = FMath::FInterpTo(CurrentLeanOffset, LeanTarget * LeanOffsetDistance, DeltaSeconds, LeanInterpSpeed);

	UpdateAimedDoorway();
	UpdateChallengeTarget();



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

		// squad commands
		PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AProjectTFCharacter::CommandFollow);
		PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AProjectTFCharacter::CommandHold);
		PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AProjectTFCharacter::CommandStackOrOne);
		PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AProjectTFCharacter::CommandClearOrTwo);
		PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AProjectTFCharacter::CommandWatch);
		PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AProjectTFCharacter::CommandChallenge);
		PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AProjectTFCharacter::CycleElementUp);
		PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AProjectTFCharacter::CycleElementDown);
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


//~ Squad commands --------------------------------------------------------------

void AProjectTFCharacter::UpdateAimedDoorway()
{
	AimedDoorway = nullptr;

	const UWorld* World = GetWorld();
	if (!World || !FirstPersonCameraComponent)
	{
		return;
	}

	// Picked geometrically rather than by a collision trace. Giving the doorway a box to trace
	// against means giving it a collision channel, and the obvious one - visibility - is the same
	// channel the AI sight sense uses, so the marker ends up blocking line of sight through the
	// very doorway it describes.
	const FVector ViewLocation = FirstPersonCameraComponent->GetComponentLocation();
	const FVector ViewDirection = GetBaseAimRotation().Vector();

	float BestDot = FMath::Cos(FMath::DegreesToRadians(DoorwayAimAngle));

	for (TActorIterator<ADoorwayMarker> It(World); It; ++It)
	{
		ADoorwayMarker* Doorway = *It;

		const FVector ToDoorway = Doorway->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f) - ViewLocation;
		const float Distance = ToDoorway.Size();

		if (Distance > DoorwayAimRange || Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(ToDoorway / Distance, ViewDirection);
		if (Dot < BestDot)
		{
			continue;
		}

		// no ordering a doorway through a wall
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBDoorwayAim), false, this);
		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, ViewLocation, Doorway->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), ECC_Visibility, Params))
		{
			continue;
		}

		BestDot = Dot;
		AimedDoorway = Doorway;
	}
}

TArray<AAllyAIController*> AProjectTFCharacter::GetSquad() const
{
	TArray<AAllyAIController*> Squad;

	for (TActorIterator<AAllyAIController> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->GetPawn())
		{
			Squad.Add(*It);
		}
	}

	// stable order, so Red always lists before Blue
	Squad.Sort([](const AAllyAIController& A, const AAllyAIController& B)
	{
		return A.GetDisplayName() < B.GetDisplayName();
	});

	return Squad;
}

TArray<AAllyAIController*> AProjectTFCharacter::GetSelectedSquad() const
{
	TArray<AAllyAIController*> Selected;

	for (AAllyAIController* Member : GetSquad())
	{
		if (SelectedElement == ESquadElement::All || Member->GetElement() == SelectedElement)
		{
			Selected.Add(Member);
		}
	}

	return Selected;
}

void AProjectTFCharacter::CycleElementUp()
{
	SelectedElement = (SelectedElement == ESquadElement::All) ? ESquadElement::Red
		: (SelectedElement == ESquadElement::Red ? ESquadElement::Blue : ESquadElement::All);
}

void AProjectTFCharacter::CycleElementDown()
{
	SelectedElement = (SelectedElement == ESquadElement::All) ? ESquadElement::Blue
		: (SelectedElement == ESquadElement::Blue ? ESquadElement::Red : ESquadElement::All);
}

bool AProjectTFCharacter::GetAimedPoint(FVector& OutPoint) const
{
	const UWorld* World = GetWorld();
	if (!World || !FirstPersonCameraComponent)
	{
		return false;
	}

	const FVector Start = FirstPersonCameraComponent->GetComponentLocation();
	const FVector End = Start + GetBaseAimRotation().Vector() * 6000.0f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBOrderPoint), false, this);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}

	return false;
}

void AProjectTFCharacter::UpdateChallengeTarget()
{
	ChallengeTarget = nullptr;

	const UWorld* World = GetWorld();
	if (!World || !FirstPersonCameraComponent)
	{
		return;
	}

	const FVector Start = FirstPersonCameraComponent->GetComponentLocation();
	const FVector End = Start + GetBaseAimRotation().Vector() * ChallengeRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBChallenge), false, this);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return;
	}

	AEnemyCharacter* Suspect = Cast<AEnemyCharacter>(Hit.GetActor());
	if (Suspect && !Suspect->IsDead() && !Suspect->IsSurrendered()
		&& FCQBFactions::AreHostile(this, Suspect))
	{
		ChallengeTarget = Suspect;
	}
}

void AProjectTFCharacter::CommandChallenge()
{
	if (!ChallengeTarget)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(32000, 2.5f, FColor::Yellow,
			FString::Printf(TEXT("[You] %s"), *FCQBNames::CalloutToString(ECalloutType::Challenge)));
	}

	// a shout is a noise like any other; anyone nearby hears it
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.5f, this, ChallengeRange, TEXT("Shout"));

	if (AEnemyAIController* Brain = Cast<AEnemyAIController>(Cast<APawn>(ChallengeTarget)->GetController()))
	{
		// aiming straight at them is what makes a demand credible
		Brain->ReceiveChallenge(this, 0.35f);
	}
}

void AProjectTFCharacter::CommandWatch()
{
	FVector Point;
	if (!GetAimedPoint(Point))
	{
		return;
	}

	for (AAllyAIController* Member : GetSelectedSquad())
	{
		Member->OrderWatch(Point);
	}
}

void AProjectTFCharacter::CommandFollow()
{
	for (AAllyAIController* Member : GetSelectedSquad())
	{
		Member->OrderFollow();
	}
}

void AProjectTFCharacter::CommandHold()
{
	for (AAllyAIController* Member : GetSelectedSquad())
	{
		Member->OrderHold();
	}
}

void AProjectTFCharacter::CommandStackOrOne()
{
	if (!AimedDoorway)
	{
		return;
	}

	// the squad splits across the doorway, one side each
	TArray<AAllyAIController*> Squad = GetSelectedSquad();

	for (int32 Index = 0; Index < Squad.Num(); ++Index)
	{
		const EStackSide Side = (Index % 2 == 0) ? EStackSide::Left : EStackSide::Right;
		Squad[Index]->OrderStack(AimedDoorway, Side);
	}
}

void AProjectTFCharacter::CommandClearOrTwo()
{
	if (!AimedDoorway)
	{
		return;
	}

	for (AAllyAIController* Member : GetSelectedSquad())
	{
		Member->OrderClear(AimedDoorway);
	}
}

void AProjectTFCharacter::RunScriptedOrder()
{
	FString Order;
	FParse::Value(FCommandLine::Get(), TEXT("CQBOrder="), Order);

	int32 DoorIndex = 0;
	FParse::Value(FCommandLine::Get(), TEXT("CQBOrderDoor="), DoorIndex);

	// pick the doorway by index, in the order the level lists them
	TArray<ADoorwayMarker*> Doorways;
	for (TActorIterator<ADoorwayMarker> It(GetWorld()); It; ++It)
	{
		Doorways.Add(*It);
	}
	Doorways.Sort([](const ADoorwayMarker& A, const ADoorwayMarker& B)
	{
		return A.GetActorLocation().X < B.GetActorLocation().X;
	});

	AimedDoorway = Doorways.IsValidIndex(DoorIndex) ? Doorways[DoorIndex] : nullptr;

	UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: scripted order '%s' on %s"),
		*Order, AimedDoorway ? *AimedDoorway->GetDisplayName() : TEXT("no doorway"));

	if (Order == TEXT("hold"))
	{
		CommandHold();
	}
	else if (Order == TEXT("watch"))
	{
		// aim at the far wall of the room beyond and have the squad keep eyes on it
		for (AAllyAIController* Member : GetSelectedSquad())
		{
			Member->OrderWatch(AimedDoorway ? AimedDoorway->GetClearPoint() : GetActorLocation());
		}
	}
	else if (Order == TEXT("challenge"))
	{
		// shout at the nearest standing suspect, wherever the crosshair happens to be
		AEnemyCharacter* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();

		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			AEnemyCharacter* Suspect = *It;
			if (!Suspect || Suspect->IsDead() || Suspect->IsSurrendered()
				|| !FCQBFactions::AreHostile(this, Suspect))
			{
				continue;
			}

			const float Distance = FVector::Dist(GetActorLocation(), Suspect->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = Suspect;
			}
		}

		if (Nearest)
		{
			UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: challenging %s at %.0f"), *Nearest->GetName(), NearestDistance);
			ChallengeTarget = Nearest;
			CommandChallenge();
		}
	}
	else if (Order == TEXT("stack"))
	{
		CommandStackOrOne();
	}
	else if (Order == TEXT("clear"))
	{
		CommandClearOrTwo();
	}
	else
	{
		CommandFollow();
	}
}

UAISense_Sight::EVisibilityResult AProjectTFCharacter::CanBeSeenFrom(const FCanBeSeenFromContext& Context,
	FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested,
	float& OutSightStrength, int32* UserData, const FOnPendingVisibilityQueryProcessedDelegate* Delegate)
{
	OutNumberOfLoSChecksPerformed = 0;
	OutNumberOfAsyncLosCheckRequested = 0;

	const bool bSeen = CQBSightTarget::CanBeSeenFrom(*this, Context.ObserverLocation, Context.IgnoreActor,
		OutSeenLocation, OutNumberOfLoSChecksPerformed, OutSightStrength);

	return bSeen ? UAISense_Sight::EVisibilityResult::Visible : UAISense_Sight::EVisibilityResult::NotVisible;
}
