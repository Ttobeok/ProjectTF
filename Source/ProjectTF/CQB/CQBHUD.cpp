// CQB Sample - crosshair and ammo readout, drawn from C++.

#include "CQBHUD.h"
#include "WeaponComponent.h"
#include "HealthComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EnemyCharacter.h"
#include "DoorwayMarker.h"
#include "ProjectTFCharacter.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "ProjectTF.h"

ACQBHUD::ACQBHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACQBHUD::BindToPlayerWeapon()
{
	APawn* MyPawn = GetOwningPawn();
	UWeaponComponent* Weapon = MyPawn ? MyPawn->FindComponentByClass<UWeaponComponent>() : nullptr;

	if (!Weapon || BoundWeapon.Get() == Weapon)
	{
		return;
	}

	BoundWeapon = Weapon;
	Weapon->OnWeaponHit.AddUniqueDynamic(this, &ACQBHUD::OnWeaponHit);
	Weapon->OnAmmoChanged.AddUniqueDynamic(this, &ACQBHUD::OnAmmoChanged);
}

void ACQBHUD::OnWeaponHit(AActor* HitActor, float DamageDealt)
{
	HitMarkerTime = 0.25f;
}

void ACQBHUD::OnAmmoChanged(int32 CurrentAmmo, int32 MagSize)
{
	// only a shot reduces the count; a reload fills it back up
	FireKick = FireKickGap;
}

void ACQBHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	BindToPlayerWeapon();

	const APawn* MyPawn = GetOwningPawn();
	const UWeaponComponent* Weapon = MyPawn ? MyPawn->FindComponentByClass<UWeaponComponent>() : nullptr;
	const UHealthComponent* Health = MyPawn ? MyPawn->FindComponentByClass<UHealthComponent>() : nullptr;

	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

	// settle the recoil kick and the hit marker
	FireKick = FMath::FInterpTo(FireKick, 0.0f, DeltaTime, GapInterpSpeed);
	HitMarkerTime = FMath::Max(0.0f, HitMarkerTime - DeltaTime);

	DrawCrosshair(Weapon);
	DrawReadout(Weapon, Health);
	DrawSquadBar();
}

void ACQBHUD::DrawCrosshair(const UWeaponComponent* Weapon)
{
	const float CentreX = Canvas->SizeX * 0.5f;
	const float CentreY = Canvas->SizeY * 0.5f;

	const bool bIsADS = Weapon && Weapon->IsADS();
	const float TargetGap = (bIsADS ? ADSGap : HipGap) + FireKick;

	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	CurrentGap = FMath::FInterpTo(CurrentGap, TargetGap, DeltaTime, GapInterpSpeed);

	const FLinearColor Color = HitMarkerTime > 0.0f ? HitMarkerColor : CrosshairColor;

	// four ticks: left, right, top, bottom
	DrawLine(CentreX - CurrentGap - TickLength, CentreY, CentreX - CurrentGap, CentreY, Color, TickThickness);
	DrawLine(CentreX + CurrentGap, CentreY, CentreX + CurrentGap + TickLength, CentreY, Color, TickThickness);
	DrawLine(CentreX, CentreY - CurrentGap - TickLength, CentreX, CentreY - CurrentGap, Color, TickThickness);
	DrawLine(CentreX, CentreY + CurrentGap, CentreX, CentreY + CurrentGap + TickLength, Color, TickThickness);

	// centre dot
	DrawRect(Color, CentreX - 1.0f, CentreY - 1.0f, 2.0f, 2.0f);

	// a hit draws an X over the centre so it reads at a glance
	if (HitMarkerTime > 0.0f)
	{
		const float Size = 7.0f;
		DrawLine(CentreX - Size, CentreY - Size, CentreX + Size, CentreY + Size, HitMarkerColor, TickThickness);
		DrawLine(CentreX - Size, CentreY + Size, CentreX + Size, CentreY - Size, HitMarkerColor, TickThickness);
	}
}

void ACQBHUD::DrawReadout(const UWeaponComponent* Weapon, const UHealthComponent* Health)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const float Margin = 40.0f;
	const float BottomY = Canvas->SizeY - 70.0f;

	if (Health)
	{
		const float Percent = Health->GetHealthPercent();
		const FLinearColor HealthColor = Percent > 0.5f ? FLinearColor::White
			: (Percent > 0.25f ? FLinearColor(1.0f, 0.75f, 0.1f) : FLinearColor(1.0f, 0.2f, 0.15f));

		DrawText(FString::Printf(TEXT("HP  %.0f"), Health->CurrentHealth), HealthColor, Margin, BottomY, Font, 1.2f);
	}

	if (Weapon)
	{
		const FString AmmoText = Weapon->IsReloading()
			? FString(TEXT("RELOADING"))
			: FString::Printf(TEXT("%d / %d"), Weapon->GetCurrentAmmo(), Weapon->GetMagSize());

		const FLinearColor AmmoColor = Weapon->GetCurrentAmmo() > 0 ? FLinearColor::White : FLinearColor(1.0f, 0.35f, 0.2f);

		// rough right align, the large font is about 14 px per character at this scale
		const float TextWidth = AmmoText.Len() * 16.0f;
		DrawText(AmmoText, AmmoColor, Canvas->SizeX - Margin - TextWidth, BottomY, Font, 1.2f);
	}
}


void ACQBHUD::DrawSquadBar()
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	const AProjectTFCharacter* Player = Cast<AProjectTFCharacter>(GetOwningPawn());
	if (!Font || !Player)
	{
		return;
	}

	const float CentreX = Canvas->SizeX * 0.5f;

	// what the squad is doing right now
	const FString Orders = Player->GetSquadOrderSummary();
	if (!Orders.IsEmpty())
	{
		const float Width = Orders.Len() * 8.0f;
		DrawText(Orders, FLinearColor(0.7f, 0.85f, 1.0f), CentreX - Width * 0.5f, Canvas->SizeY - 110.0f, Font, 1.0f);
	}

	// aiming at a doorway turns 1 and 2 into orders about it
	if (const ADoorwayMarker* Doorway = Player->GetAimedDoorway())
	{
		const FString Hint = FString::Printf(TEXT("%s    [1] Stack    [2] Clear"), *Doorway->GetDisplayName());
		const float Width = Hint.Len() * 11.0f;

		DrawText(Hint, FLinearColor(1.0f, 0.9f, 0.4f), CentreX - Width * 0.5f, Canvas->SizeY - 145.0f, Font, 1.15f);
	}
	else
	{
		const FString Hint(TEXT("[Z] Follow    [H] Hold    aim a doorway for Stack / Clear"));
		const float Width = Hint.Len() * 7.0f;

		DrawText(Hint, FLinearColor(0.55f, 0.55f, 0.6f), CentreX - Width * 0.5f, Canvas->SizeY - 145.0f, Font, 0.85f);
	}
}

//~ Debug console commands ----------------------------------------------------

void ACQBHUD::CQBKillEnemy(float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (DelaySeconds > 0.0f)
	{
		World->GetTimerManager().SetTimer(KillTimerHandle, this, &ACQBHUD::KillOneEnemy, DelaySeconds, false);
		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: killing an enemy in %.1fs"), DelaySeconds);
		return;
	}

	KillOneEnemy();
}

void ACQBHUD::KillOneEnemy()
{
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}

		UHealthComponent* Health = Enemy->GetHealthComponent();
		if (!Health)
		{
			continue;
		}

		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: killing %s"), *Enemy->GetName());
		Health->TakeDamage(Health->MaxHealth * 10.0f, GetOwningPawn(), GetOwningPlayerController());
		return;
	}

	UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: no living enemy to kill"));
}

void ACQBHUD::CQBMovePlayer(float X, float Y, float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PendingTeleport = FVector(X, Y, 150.0f);

	if (DelaySeconds > 0.0f)
	{
		World->GetTimerManager().SetTimer(TeleportTimerHandle, this, &ACQBHUD::TeleportPlayer, DelaySeconds, false);
		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: teleporting the player in %.1fs"), DelaySeconds);
		return;
	}

	TeleportPlayer();
}

void ACQBHUD::TeleportPlayer()
{
	if (APawn* MyPawn = GetOwningPawn())
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: teleporting the player to %s"), *PendingTeleport.ToCompactString());
		MyPawn->TeleportTo(PendingTeleport, MyPawn->GetActorRotation());
	}
}
