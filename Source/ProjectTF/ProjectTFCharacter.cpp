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
#include "CQB/CQBCharacter.h"
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
	// 충돌 캡슐 크기를 설정합니다
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// Create the first person mesh that will be viewed only by this character's owner
	// 이 캐릭터의 소유자에게만 보이는 1인칭 메시를 만듭니다
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component
	// 카메라 컴포넌트를 만듭니다
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// CQB gameplay components, shared with the AI enemies
	// CQB 게임플레이 컴포넌트. AI와 공유합니다
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health Component"));

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("Weapon Component"));
	WeaponComponent->AimSpreadHalfAngle = 0.25f;
	WeaponComponent->bApplyRecoilToController = true;

	// the visible weapon, riding the camera. It attaches itself on BeginPlay.
	// 카메라에 올라타는, 눈에 보이는 무기. BeginPlay에서 스스로 부착합니다.
	WeaponVisual = CreateDefaultSubobject<UWeaponVisualComponent>(TEXT("Weapon Visual"));
	WeaponVisual->SetupAttachment(FirstPersonCameraComponent);
	WeaponVisual->AttachMode = EWeaponAttachMode::Camera;

	// configure the character comps
	// 캐릭터 컴포넌트를 설정합니다
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// make sure enemy bullets and AI sight traces can hit the player
	// 적의 탄과 AI 시야 트레이스가 플레이어에게 닿도록 합니다
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Configure character movement
	// 캐릭터 무브먼트를 설정합니다
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

	// The arms are not animated to hold anything, so they would be empty handed next to a
	// weapon that rides the camera. Hiding them keeps the view honest.
	// 팔에는 무언가를 쥐는 애니메이션이 없어서, 카메라에 올라탄 무기 옆에 빈손으로 놓이게
	// 됩니다. 숨기는 편이 화면을 정직하게 유지합니다.
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(false, false);
	}
}

void AProjectTFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// blend the lean towards the requested side
	// 요청된 방향으로 린을 보간합니다
	CurrentLeanRoll = FMath::FInterpTo(CurrentLeanRoll, LeanTarget * LeanRollAngle, DeltaSeconds, LeanInterpSpeed);
	CurrentLeanOffset = FMath::FInterpTo(CurrentLeanOffset, LeanTarget * LeanOffsetDistance, DeltaSeconds, LeanInterpSpeed);

	UpdateAimedDoorway();
	UpdateChallengeTarget();



}

void AProjectTFCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	// 액션 바인딩을 설정합니다
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		// 점프
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AProjectTFCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AProjectTFCharacter::DoJumpEnd);

		// Moving
		// 이동
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProjectTFCharacter::MoveInput);

		// Looking/Aiming
		// 시점/조준
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProjectTFCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AProjectTFCharacter::LookInput);

		// CQB actions. These Input Actions are optional; see the direct key bindings below.
		// CQB 액션들. 이 Input Action들은 선택 사항입니다. 아래의 직접 키 바인딩을 보세요.
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
	// Input Action 에셋을 하나도 만들지 않아도 CQB 조작이 동작하도록 직접 키를 바인딩합니다.
	// 위에서 해당 Input Action을 지정하면 그쪽이 바인딩을 가져갑니다.
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
		// 분대 명령
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
	// Vector2D 이동 축을 가져옵니다
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	// 축 값을 이동 입력으로 넘깁니다
	DoMove(MovementVector.X, MovementVector.Y);

}

void AProjectTFCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	// Vector2D 시점 축을 가져옵니다
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	// 축 값을 조준 입력으로 넘깁니다
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AProjectTFCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		// 회전 입력을 넘깁니다
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AProjectTFCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		// 이동 입력을 넘깁니다
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AProjectTFCharacter::DoJumpStart()
{
	// pass Jump to the character
	// Jump를 캐릭터로 넘깁니다
	Jump();
}

void AProjectTFCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	// StopJumping을 캐릭터로 넘깁니다
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
		// the raw actor name in a packaged build is EnemyCharacter_2147482371; say Enemy_1 instead
		// 패키지 빌드에서 액터 원래 이름은 EnemyCharacter_2147482371입니다. 대신 Enemy_1로 부릅니다
		FString From = GetNameSafe(Causer);
		if (const APawn* CauserPawn = Cast<APawn>(Causer))
		{
			if (const AEnemyAIController* Brain = Cast<AEnemyAIController>(CauserPawn->GetController()))
			{
				From = Brain->GetDisplayName();
			}
		}

		GEngine->AddOnScreenDebugMessage(9001, 2.0f, FColor::Red,
			FString::Printf(TEXT("TAKING FIRE from %s   HP %.0f/%.0f"), *From, NewHealth, HealthComp->MaxHealth));
	}
}

void AProjectTFCharacter::OnPlayerDeath(AActor* DeadActor, AActor* Killer)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9002, 10.0f, FColor::Red, TEXT("YOU ARE DOWN"));
	}

	// stop shooting and hand back control
	// 사격을 멈추고 조작을 돌려줍니다
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
//~ Squad commands / 분대 명령 ----------------------------------------------------

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
	// 충돌 트레이스가 아니라 기하학적으로 고릅니다. 문에 트레이스용 박스를 달면 충돌 채널을
	// 줘야 하는데, 가장 자연스러운 선택인 visibility는 AI 시야 감각이 쓰는 바로 그 채널입니다.
	// 그러면 마커가 자기가 가리키는 그 문의 시야를 막아버립니다.
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
		// 벽 너머의 문에는 명령할 수 없습니다
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBDoorwayAim), false, this);
		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, ViewLocation, Doorway->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), ECC_Visibility, Params))
		{
			continue;
		}

		BestDot = Dot;
		AimedDoorway = Doorway;
	}

	// tell every doorway where it stands, so the highlight follows the crosshair
	// 모든 문에 자기 상태를 알려줍니다. 그래야 표시가 크로스헤어를 따라갑니다
	for (TActorIterator<ADoorwayMarker> It(World); It; ++It)
	{
		It->SetAimedAt(*It == AimedDoorway);
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
	// 일정한 순서. Red가 항상 Blue보다 먼저 나열되게 합니다
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

	ACQBCharacter* Suspect = Cast<ACQBCharacter>(Hit.GetActor());
	if (Suspect && !Suspect->IsDead() && !Suspect->IsSurrendered()
		&& FCQBFactions::AreHostile(this, Suspect))
	{
		ChallengeTarget = Suspect;
	}
}

void AProjectTFCharacter::CommandChallenge()
{
	IssueChallenge(ChallengeTarget);
}

void AProjectTFCharacter::IssueChallenge(AActor* Suspect)
{
	APawn* SuspectPawn = Cast<APawn>(Suspect);
	if (!SuspectPawn)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(32000, 2.5f, FColor::Yellow,
			FString::Printf(TEXT("[You] %s"), *FCQBNames::CalloutToString(ECalloutType::Challenge)));
	}

	// a shout is a noise like any other; anyone nearby hears it
	// 외침도 다른 소리와 마찬가지입니다. 근처에 있으면 누구든 듣습니다
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.5f, this, ChallengeRange, TEXT("Shout"));

	if (AEnemyAIController* Brain = Cast<AEnemyAIController>(SuspectPawn->GetController()))
	{
		// aiming straight at them is what makes a demand credible
		// 정면으로 겨누고 있다는 사실이 그 요구에 무게를 실어줍니다
		Brain->ReceiveChallenge(this, 0.35f);
	}
}

void AProjectTFCharacter::IssueSquadOrder(const FString& OrderName, ADoorwayMarker* Doorway)
{
	TArray<AAllyAIController*> Selected = GetSelectedSquad();

	if (OrderName == TEXT("hold"))
	{
		for (AAllyAIController* Member : Selected)
		{
			Member->OrderHold();
		}
	}
	else if (OrderName == TEXT("watch"))
	{
		const FVector Point = Doorway ? Doorway->GetClearPoint() : GetActorLocation();
		for (AAllyAIController* Member : Selected)
		{
			Member->OrderWatch(Point);
		}
	}
	else if (OrderName == TEXT("stack") && Doorway)
	{
		for (int32 Index = 0; Index < Selected.Num(); ++Index)
		{
			Selected[Index]->OrderStack(Doorway, (Index % 2 == 0) ? EStackSide::Left : EStackSide::Right);
		}
	}
	else if (OrderName == TEXT("clear") && Doorway)
	{
		for (AAllyAIController* Member : Selected)
		{
			Member->OrderClear(Doorway);
		}
	}
	else
	{
		for (AAllyAIController* Member : Selected)
		{
			Member->OrderFollow();
		}
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
	IssueSquadOrder(TEXT("follow"), nullptr);
}

void AProjectTFCharacter::CommandHold()
{
	IssueSquadOrder(TEXT("hold"), nullptr);
}

void AProjectTFCharacter::CommandStackOrOne()
{
	IssueSquadOrder(TEXT("stack"), AimedDoorway);
}

void AProjectTFCharacter::CommandClearOrTwo()
{
	IssueSquadOrder(TEXT("clear"), AimedDoorway);
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
