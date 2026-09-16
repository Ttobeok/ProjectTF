// CQB Sample - hitscan weapon component shared by player and enemies.
// CQB 샘플 - 플레이어와 적이 함께 쓰는 히트스캔 무기 컴포넌트.

#include "WeaponComponent.h"
#include "WeaponVisualComponent.h"
#include "CQBCharacter.h"
#include "WeaponData.h"
#include "HealthComponent.h"
#include "CQBTypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Perception/AISense_Hearing.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Debug message slots, offset by the owner id so two shooters never overwrite each other */
	constexpr int32 SlotFire = 0;
	constexpr int32 SlotHit = 1;
	constexpr int32 SlotReload = 2;
}

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// cache the camera of the owner. AI pawns have none and fall back to the pawn view point.
	// 소유자의 카메라를 캐시합니다. AI 폰은 카메라가 없어 폰 시점으로 대체합니다.
	if (AActor* Owner = GetOwner())
	{
		OwnerCamera = Owner->FindComponentByClass<UCameraComponent>();
	}

	CurrentAmmo = GetMagSize();
	OnAmmoChanged.Broadcast(CurrentAmmo, GetMagSize());

	// start from the hip FOV so ADS has something to blend from
	// 정조준이 보간을 시작할 기준이 있도록 허리 FOV에서 출발합니다
	if (OwnerCamera)
	{
		OwnerCamera->SetFieldOfView(GetWeaponData()->HipFov);
	}
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefireTimerHandle);
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

UWeaponData* UWeaponComponent::GetWeaponData() const
{
	if (WeaponData)
	{
		return WeaponData;
	}

	// no asset assigned: keep a transient instance around so the weapon works with zero setup
	// 에셋 미지정: 임시 인스턴스를 들고 있어서 아무 설정 없이도 무기가 동작합니다
	if (!RuntimeDefaultData)
	{
		RuntimeDefaultData = NewObject<UWeaponData>(const_cast<UWeaponComponent*>(this), UWeaponData::StaticClass(), TEXT("RuntimeDefaultWeaponData"));
	}

	return RuntimeDefaultData;
}

int32 UWeaponComponent::GetMagSize() const
{
	return GetWeaponData()->MagSize;
}

FVector UWeaponComponent::GetMuzzleLocation() const
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	GetViewPoint(Location, Rotation);
	return Location;
}

void UWeaponComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	AActor* Owner = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(Owner);

	// bullets leave from the camera when there is one
	// 카메라가 있으면 탄은 카메라에서 출발합니다
	if (OwnerCamera)
	{
		OutLocation = OwnerCamera->GetComponentLocation();
		OutRotation = OwnerCamera->GetComponentRotation();
	}
	else if (OwnerPawn)
	{
		OutLocation = OwnerPawn->GetPawnViewLocation();
		OutRotation = OwnerPawn->GetBaseAimRotation();
	}
	else if (Owner)
	{
		OutLocation = Owner->GetActorLocation();
		OutRotation = Owner->GetActorRotation();
	}

	// the controller rotation is a frame fresher than the camera component transform
	// 컨트롤러 회전이 카메라 컴포넌트 트랜스폼보다 한 프레임 더 최신입니다
	if (OwnerPawn && OwnerPawn->GetController())
	{
		OutRotation = OwnerPawn->GetBaseAimRotation();
	}
}

bool UWeaponComponent::CanFire() const
{
	if (bIsReloading || bRefireCooldown)
	{
		return false;
	}

	// a dead shooter keeps its finger off the trigger
	// 죽은 사수는 방아쇠에서 손을 뗍니다
	if (const UHealthComponent* Health = UHealthComponent::FindHealthComponent(GetOwner()))
	{
		if (Health->IsDead())
		{
			return false;
		}
	}

	return true;
}

void UWeaponComponent::StartFire()
{
	bTriggerHeld = true;
	Fire();
}

void UWeaponComponent::StopFire()
{
	bTriggerHeld = false;
}

void UWeaponComponent::Fire()
{
	if (!CanFire())
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	UWeaponData* Data = GetWeaponData();

	// dry fire: kick off a reload instead
	// 빈 격발: 대신 재장전을 겁니다
	if (CurrentAmmo <= 0)
	{
		DebugMessage(SlotFire, FColor::Orange, FString::Printf(TEXT("[%s] click - out of ammo"), *Owner->GetName()));
		Reload();
		return;
	}

	--CurrentAmmo;
	OnAmmoChanged.Broadcast(CurrentAmmo, GetMagSize());

	LastFireTime = World->GetTimeSeconds();

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	// apply the aim cone
	// 탄 퍼짐 원뿔을 적용합니다
	FVector ShotDirection = ViewRotation.Vector();
	if (AimSpreadHalfAngle > 0.0f)
	{
		ShotDirection = FMath::VRandCone(ShotDirection, FMath::DegreesToRadians(AimSpreadHalfAngle));
	}

	const FVector TraceEnd = ViewLocation + ShotDirection * Data->Range;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CQBWeaponTrace), false, Owner);
	QueryParams.AddIgnoredActor(Owner);

	FHitResult Hit;
	const bool bHitSomething = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);
	const FVector ImpactPoint = bHitSomething ? Hit.ImpactPoint : TraceEnd;

	DebugMessage(SlotFire, FColor::Yellow, FString::Printf(TEXT("[%s] FIRE  %d/%d"), *Owner->GetName(), CurrentAmmo, GetMagSize()));

	if (bDrawDebugTrace)
	{
		// from the muzzle, not the eye: a line from the camera to what the camera is looking at
		// is seen end on, and a thick one fills the middle of the screen with a coloured slab
		// 눈이 아니라 총구에서: 카메라에서 카메라가 보는 지점까지 그은 선은 정면으로 보이므로,
		// 두꺼우면 화면 한가운데가 색 덩어리로 찹니다
		FVector TracerStart = ViewLocation;
		if (const UWeaponVisualComponent* Visual = Owner->FindComponentByClass<UWeaponVisualComponent>())
		{
			TracerStart = Visual->GetMuzzleLocation(ShotDirection);
		}

		DrawDebugLine(World, TracerStart, ImpactPoint, FColor::Yellow, false, 0.6f, 0, 0.6f);
	}

	// damage anything carrying a health component
	// 체력 컴포넌트를 가진 대상에게 피해를 줍니다
	if (bHitSomething)
	{
		AActor* HitActor = Hit.GetActor();

		// never shoot your own side. The AI aims with a cone, so a squad member crossing the
		// line of fire is a matter of time rather than an accident.
		// 같은 편은 절대 쏘지 않습니다. AI는 원뿔로 조준하므로, 분대원이 사선을 가로지르는 건
		// 사고가 아니라 시간 문제입니다.
		// Someone who has put their hands up is out of the fight for everyone. The AI already
		// stops aiming at them, but a 4 degree cone at room distance is wide enough to catch a
		// kneeling suspect standing next to a live one, so the shot has to be refused here too.
		//
		// 손을 든 사람은 모두에게 전투 밖입니다. AI는 이미 조준을 멈추지만, 실내 거리에서
		// 4도 원뿔은 살아 있는 적 옆에 무릎 꿇은 용의자를 맞힐 만큼 넓습니다. 그래서 여기서도
		// 사격을 거부해야 합니다.
		const ACQBCharacter* HitCharacter = Cast<ACQBCharacter>(HitActor);
		const bool bHasSurrendered = HitCharacter && HitCharacter->IsSurrendered();

		const bool bFriendly = bHasSurrendered
			|| (!FCQBFactions::AreHostile(Owner, HitActor)
				&& FCQBFactions::GetFaction(HitActor) != ECQBFaction::Neutral);

		if (bFriendly)
		{
			DebugMessage(SlotHit, FColor::Silver, FString::Printf(TEXT("[%s] holding fire, %s is friendly"),
				*Owner->GetName(), *HitActor->GetName()));
		}
		else if (UHealthComponent* TargetHealth = UHealthComponent::FindHealthComponent(HitActor))
		{
			AController* InstigatorController = nullptr;
			if (APawn* OwnerPawn = Cast<APawn>(Owner))
			{
				InstigatorController = OwnerPawn->GetController();
			}

			// go through the engine damage pipeline rather than poking the health component
			// directly, so god mode, damage types and immunity all keep working.
			// UHealthComponent listens to OnTakeAnyDamage and applies the result.
			// 체력 컴포넌트를 직접 건드리지 않고 엔진 데미지 파이프라인을 거칩니다. 그래야
			// God 치트·데미지 타입·면역이 모두 살아 있습니다.
			// UHealthComponent가 OnTakeAnyDamage를 구독해 결과를 반영합니다.
			const float DamageDealt = UGameplayStatics::ApplyPointDamage(
				HitActor, Data->Damage, ShotDirection, Hit, InstigatorController, Owner, nullptr);

			DebugMessage(SlotHit, FColor::Red, FString::Printf(TEXT("[%s] HIT %s  -%.0f  (%.0f HP left)"),
				*Owner->GetName(), *HitActor->GetName(), DamageDealt, TargetHealth->CurrentHealth));

			OnWeaponHit.Broadcast(HitActor, DamageDealt);

			if (bDrawDebugTrace)
			{
				DrawDebugSphere(World, ImpactPoint, 12.0f, 8, FColor::Red, false, 1.0f);
			}
		}
		else if (bDrawDebugTrace)
		{
			DrawDebugPoint(World, ImpactPoint, 8.0f, FColor::White, false, 1.0f);
		}
	}

	ApplyRecoil();

	// let the AI hearing sense know a shot went off
	// 총이 나갔다는 걸 AI 청각에 알립니다
	UAISense_Hearing::ReportNoiseEvent(World, ViewLocation, Data->NoiseLoudness, Owner, Data->NoiseRange, TEXT("Gunshot"));

	// start the refire cooldown
	// 재발사 쿨다운을 시작합니다
	bRefireCooldown = true;
	World->GetTimerManager().SetTimer(RefireTimerHandle, this, &UWeaponComponent::OnRefireReady, Data->GetShotInterval(), false);
}

void UWeaponComponent::OnRefireReady()
{
	bRefireCooldown = false;

	// automatic weapons keep going while the trigger is held
	// 자동 무기는 방아쇠를 누르고 있는 동안 계속 나갑니다
	if (bTriggerHeld && GetWeaponData()->bAutomatic)
	{
		Fire();
	}
}

void UWeaponComponent::Reload()
{
	UWorld* World = GetWorld();
	if (!World || bIsReloading || CurrentAmmo >= GetMagSize())
	{
		return;
	}

	if (const UHealthComponent* Health = UHealthComponent::FindHealthComponent(GetOwner()))
	{
		if (Health->IsDead())
		{
			return;
		}
	}

	bIsReloading = true;

	DebugMessage(SlotReload, FColor::Cyan, FString::Printf(TEXT("[%s] RELOADING..."), *GetNameSafe(GetOwner())));

	World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UWeaponComponent::FinishReload, GetWeaponData()->ReloadTime, false);
}

void UWeaponComponent::FinishReload()
{
	bIsReloading = false;
	CurrentAmmo = GetMagSize();
	OnAmmoChanged.Broadcast(CurrentAmmo, GetMagSize());

	DebugMessage(SlotReload, FColor::Green, FString::Printf(TEXT("[%s] RELOADED  %d/%d"), *GetNameSafe(GetOwner()), CurrentAmmo, GetMagSize()));

	// keep firing if the trigger is still down
	// 방아쇠가 아직 눌려 있으면 계속 쏩니다
	if (bTriggerHeld && GetWeaponData()->bAutomatic)
	{
		Fire();
	}
}

void UWeaponComponent::StartADS()
{
	bIsADS = true;
}

void UWeaponComponent::StopADS()
{
	bIsADS = false;
}

void UWeaponComponent::ApplyRecoil()
{
	if (!bApplyRecoilToController)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	if (!OwnerController)
	{
		return;
	}

	const UWeaponData* Data = GetWeaponData();

	const float PitchKick = FMath::FRandRange(Data->RecoilPitchMin, Data->RecoilPitchMax);
	const float YawKick = FMath::FRandRange(Data->RecoilYawMin, Data->RecoilYawMax);

	FRotator ControlRotation = OwnerController->GetControlRotation();
	ControlRotation.Pitch += PitchKick;
	ControlRotation.Yaw += YawKick;
	OwnerController->SetControlRotation(ControlRotation);

	// remember how much we pushed so it can be pulled back down
	// 나중에 되돌릴 수 있도록 얼마나 밀었는지 기억해 둡니다
	AccumulatedRecoil.X += PitchKick;
	AccumulatedRecoil.Y += YawKick;
}

void UWeaponComponent::RecoverRecoil(float DeltaTime)
{
	if (!bApplyRecoilToController || AccumulatedRecoil.IsNearlyZero())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UWeaponData* Data = GetWeaponData();
	if (!World || World->GetTimeSeconds() - LastFireTime < Data->RecoilRecoveryDelay)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	if (!OwnerController)
	{
		AccumulatedRecoil = FVector2D::ZeroVector;
		return;
	}

	// walk each axis back towards zero and subtract the same amount from the view
	// 각 축을 0쪽으로 되돌리면서 같은 양만큼 시점에서 뺍니다
	const FVector2D Recovered(
		FMath::FInterpConstantTo(AccumulatedRecoil.X, 0.0f, DeltaTime, Data->RecoilRecoverySpeed),
		FMath::FInterpConstantTo(AccumulatedRecoil.Y, 0.0f, DeltaTime, Data->RecoilRecoverySpeed));

	const FVector2D Delta = Recovered - AccumulatedRecoil;

	FRotator ControlRotation = OwnerController->GetControlRotation();
	ControlRotation.Pitch += Delta.X;
	ControlRotation.Yaw += Delta.Y;
	OwnerController->SetControlRotation(ControlRotation);

	AccumulatedRecoil = Recovered;
}

void UWeaponComponent::UpdateADS(float DeltaTime)
{
	if (!OwnerCamera)
	{
		return;
	}

	const UWeaponData* Data = GetWeaponData();
	const float TargetFov = bIsADS ? Data->ADSFov : Data->HipFov;

	// already settled, so leave the camera alone
	// 이미 안정됐으므로 카메라를 건드리지 않습니다
	if (FMath::IsNearlyEqual(OwnerCamera->FieldOfView, TargetFov, 0.01f))
	{
		return;
	}

	const float NewFov = FMath::FInterpTo(OwnerCamera->FieldOfView, TargetFov, DeltaTime, Data->ADSInterpSpeed);
	OwnerCamera->SetFieldOfView(NewFov);
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateADS(DeltaTime);
	RecoverRecoil(DeltaTime);
}

void UWeaponComponent::DebugMessage(int32 Slot, const FColor& Color, const FString& Message) const
{
	if (!bShowDebugMessages || !GEngine)
	{
		return;
	}

	// stable key per owner and slot so repeated lines replace instead of scrolling away
	// 소유자·슬롯마다 고정 키를 써서 같은 줄이 흘러가지 않고 교체되게 합니다
	const int32 Key = (GetOwner() ? static_cast<int32>(GetOwner()->GetUniqueID() % 1000) * 10 : 0) + Slot;
	GEngine->AddOnScreenDebugMessage(Key, 2.0f, Color, Message);
}
