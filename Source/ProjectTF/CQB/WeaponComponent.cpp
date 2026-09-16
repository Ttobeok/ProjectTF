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

	// Nothing in TickComponent applies without a camera: UpdateADS needs one and RecoverRecoil
	// only runs when the recoil goes to a controller, which it does not for AI. An AI weapon
	// paid a tick registration and dispatch per frame for two immediate returns.
	//
	// 카메라가 없으면 TickComponent에서 할 일이 없습니다. UpdateADS는 카메라가 필요하고,
	// RecoverRecoil은 반동이 컨트롤러로 가는 경우에만 도는데 AI는 아닙니다. AI 무기는 즉시
	// 반환되는 두 함수를 위해 매 프레임 틱 등록과 디스패치 비용을 내고 있었습니다.
	SetComponentTickEnabled(OwnerCamera != nullptr);

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

	// Claim the cooldown now: the shot is committed, and everything after this can re-enter
	// Fire(). The damage lands, something dies, the squad reshuffles roles and an AI is told to
	// start firing - all inside this call. Claiming it at the end of the function let that
	// second shot through, for double ammo and double damage on one trigger pull.
	//
	// It goes here rather than at the top of the function because the dry-fire path above
	// returns without arming the refire timer, and nothing else clears this flag: claiming it
	// any earlier means the weapon never fires again after its first empty trigger pull.
	//
	// 여기서 쿨다운을 잡습니다. 사격은 이미 확정됐고, 이 아래의 모든 것이 Fire()로 다시
	// 들어올 수 있습니다. 피해가 들어가고, 누가 죽고, 분대가 역할을 재편성하고, 어떤 AI에게
	// 사격 시작이 지시되는 일이 전부 이 호출 안에서 벌어집니다. 함수 끝에서 잡으면 그 두 번째
	// 발사가 통과해 방아쇠 한 번에 탄약과 피해가 두 배로 들어갔습니다.
	//
	// 함수 맨 위가 아니라 여기인 이유는, 위쪽 빈 격발 경로가 재발사 타이머를 걸지 않고
	// 반환하는데 이 플래그를 푸는 곳이 그 타이머뿐이기 때문입니다. 더 일찍 잡으면 첫 빈
	// 격발 이후로 무기가 영영 발사되지 않습니다.
	bRefireCooldown = true;
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

		// The Neutral clause has to look at the shooter as well. Testing only the target inverted
		// the rule for a Neutral shooter: it could not hurt anyone hostile, and the one thing it
		// could hurt was another Neutral.
		//
		// Neutral 조건은 사수도 함께 봐야 합니다. 대상만 검사하면 Neutral 사수에서 규칙이
		// 뒤집혀서, 적대 대상에게는 피해를 못 주고 유일하게 때릴 수 있는 것이 다른
		// Neutral이 됩니다.
		const bool bEitherNeutral = FCQBFactions::GetFaction(Owner) == ECQBFaction::Neutral
			|| FCQBFactions::GetFaction(HitActor) == ECQBFaction::Neutral;

		const bool bFriendly = bHasSurrendered
			|| (!FCQBFactions::AreHostile(Owner, HitActor) && !bEitherNeutral);

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
			// ApplyPointDamage returns what was requested, not what landed - the health component
			// clamps at zero. Measure the difference so a hit on a suspect with 5 HP left reports
			// 5 and not 20.
			//
			// ApplyPointDamage는 요청한 값을 돌려주지 실제로 들어간 값을 주지 않습니다. 체력
			// 컴포넌트가 0에서 자릅니다. 차이를 재서, 5 남은 용의자를 맞혔을 때 20이 아니라
			// 5로 보고되게 합니다.
			const float HealthBefore = TargetHealth->CurrentHealth;

			UGameplayStatics::ApplyPointDamage(
				HitActor, Data->Damage, ShotDirection, Hit, InstigatorController, Owner, nullptr);

			const float DamageDealt = HealthBefore - TargetHealth->CurrentHealth;

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

	const FRotator Before = OwnerController->GetControlRotation();

	FRotator ControlRotation = Before;
	ControlRotation.Pitch += PitchKick;
	ControlRotation.Yaw += YawKick;
	OwnerController->SetControlRotation(ControlRotation);

	// Book what survived, not what was asked for. The controller clamps pitch to its view
	// limits, so firing while already aimed near vertical banks recoil that never reached the
	// view - and recovery then pulls the crosshair down below where the player left it.
	//
	// 요청한 값이 아니라 실제로 남은 값을 기록합니다. 컨트롤러가 피치를 시야 한계로 자르므로,
	// 거의 수직으로 조준한 채 쏘면 화면에 반영되지 않은 반동이 장부에 쌓이고, 회복이 그만큼
	// 조준점을 플레이어가 두고 간 곳보다 아래로 끌어내립니다.
	const FRotator Applied = OwnerController->GetControlRotation();

	AccumulatedRecoil.X += FRotator::NormalizeAxis(Applied.Pitch - Before.Pitch);
	AccumulatedRecoil.Y += FRotator::NormalizeAxis(Applied.Yaw - Before.Yaw);
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
