// CQB Sample - hitscan weapon component shared by player and enemies.

#include "WeaponComponent.h"
#include "WeaponVisualComponent.h"
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
	if (AActor* Owner = GetOwner())
	{
		OwnerCamera = Owner->FindComponentByClass<UCameraComponent>();
	}

	CurrentAmmo = GetMagSize();
	OnAmmoChanged.Broadcast(CurrentAmmo, GetMagSize());

	// start from the hip FOV so ADS has something to blend from
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
		FVector TracerStart = ViewLocation;
		if (const UWeaponVisualComponent* Visual = Owner->FindComponentByClass<UWeaponVisualComponent>())
		{
			TracerStart = Visual->GetMuzzleLocation(ShotDirection);
		}

		DrawDebugLine(World, TracerStart, ImpactPoint, FColor::Yellow, false, 0.6f, 0, 0.6f);
	}

	// damage anything carrying a health component
	if (bHitSomething)
	{
		AActor* HitActor = Hit.GetActor();

		// never shoot your own side. The AI aims with a cone, so a squad member crossing the
		// line of fire is a matter of time rather than an accident.
		const bool bFriendly = !FCQBFactions::AreHostile(Owner, HitActor)
			&& FCQBFactions::GetFaction(HitActor) != ECQBFaction::Neutral;

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
	UAISense_Hearing::ReportNoiseEvent(World, ViewLocation, Data->NoiseLoudness, Owner, Data->NoiseRange, TEXT("Gunshot"));

	// start the refire cooldown
	bRefireCooldown = true;
	World->GetTimerManager().SetTimer(RefireTimerHandle, this, &UWeaponComponent::OnRefireReady, Data->GetShotInterval(), false);
}

void UWeaponComponent::OnRefireReady()
{
	bRefireCooldown = false;

	// automatic weapons keep going while the trigger is held
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
	const int32 Key = (GetOwner() ? static_cast<int32>(GetOwner()->GetUniqueID() % 1000) * 10 : 0) + Slot;
	GEngine->AddOnScreenDebugMessage(Key, 2.0f, Color, Message);
}
