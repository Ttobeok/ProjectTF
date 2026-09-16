// CQB Sample - crosshair and ammo readout, drawn from C++.
// CQB 샘플 - C++로 그리는 크로스헤어와 탄약 표시.

#include "CQBHUD.h"
#include "WeaponComponent.h"
#include "HealthComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "CQBCharacter.h"
#include "DoorwayMarker.h"
#include "AllyAIController.h"
#include "ProjectTFCharacter.h"
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
	// 탄약을 줄이는 것은 발사뿐입니다. 재장전은 도로 채웁니다
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
	// 반동 튐과 히트 마커를 가라앉힙니다
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
	// 네 개의 눈금: 좌, 우, 상, 하
	DrawLine(CentreX - CurrentGap - TickLength, CentreY, CentreX - CurrentGap, CentreY, Color, TickThickness);
	DrawLine(CentreX + CurrentGap, CentreY, CentreX + CurrentGap + TickLength, CentreY, Color, TickThickness);
	DrawLine(CentreX, CentreY - CurrentGap - TickLength, CentreX, CentreY - CurrentGap, Color, TickThickness);
	DrawLine(CentreX, CentreY + CurrentGap, CentreX, CentreY + CurrentGap + TickLength, Color, TickThickness);

	// centre dot
	// 가운데 점
	DrawRect(Color, CentreX - 1.0f, CentreY - 1.0f, 2.0f, 2.0f);

	// a hit draws an X over the centre so it reads at a glance
	// 명중하면 가운데에 X를 그려 한눈에 읽히게 합니다
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
		// 대략적인 우측 정렬. 이 스케일에서 큰 폰트는 글자당 약 14px입니다
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

	// squad roster down the right hand side, the way a team readout usually reads
	// 분대 명부를 세로로 나열합니다. 팀 표시가 보통 읽히는 방식대로
	const TArray<AAllyAIController*> Squad = Player->GetSquad();
	const ESquadElement Selected = Player->GetSelectedElement();

	// Left hand side: the weapon view model owns the bottom right corner of the screen.
	// 왼쪽입니다. 화면 우하단은 무기 뷰모델이 차지하고 있기 때문입니다.
	const float RosterX = 40.0f;
	float RowY = Canvas->SizeY - 250.0f;

	for (const AAllyAIController* Member : Squad)
	{
		const APawn* MemberPawn = Member->GetPawn();
		const UHealthComponent* Health = MemberPawn ? MemberPawn->FindComponentByClass<UHealthComponent>() : nullptr;

		FString Status(TEXT("--"));
		FLinearColor StatusColour = FLinearColor::Gray;

		if (Health)
		{
			const float Percent = Health->GetHealthPercent();

			if (Health->IsDead())
			{
				Status = TEXT("DOWN");
				StatusColour = FLinearColor(1.0f, 0.2f, 0.15f);
			}
			else if (Percent < 0.5f)
			{
				Status = TEXT("HURT");
				StatusColour = FLinearColor(1.0f, 0.75f, 0.1f);
			}
			else
			{
				Status = TEXT("OK");
				StatusColour = FLinearColor(0.6f, 0.9f, 0.6f);
			}
		}

		// dim the members this order would not reach
		// 이 명령이 닿지 않는 인원은 흐리게 표시합니다
		const bool bInSelection = (Selected == ESquadElement::All || Member->GetElement() == Selected);
		const float Dim = bInSelection ? 1.0f : 0.4f;

		const FLinearColor ElementColour = (Member->GetElement() == ESquadElement::Red)
			? FLinearColor(1.0f, 0.45f, 0.45f) : FLinearColor(0.45f, 0.65f, 1.0f);

		// three columns, drawn separately so the condition can carry its own colour
		// 세 개의 열로 따로 그립니다. 그래야 상태가 자기 색을 가질 수 있습니다
		DrawText(Member->GetDisplayName(), ElementColour * Dim, RosterX, RowY, Font, 0.95f);
		DrawText(Status, StatusColour * Dim, RosterX + 80.0f, RowY, Font, 0.95f);
		DrawText(Member->GetOrderName(), FLinearColor(0.8f, 0.8f, 0.85f) * Dim, RosterX + 145.0f, RowY, Font, 0.95f);

		RowY += 22.0f;
	}

	// which element the next order goes to
	// 다음 명령이 어느 element로 갈지
	const FString ElementLine = FString::Printf(TEXT("COMMANDING: %s"), *FCQBNames::ElementToString(Selected));
	const FLinearColor ElementLineColour = (Selected == ESquadElement::Red) ? FLinearColor(1.0f, 0.45f, 0.45f)
		: (Selected == ESquadElement::Blue ? FLinearColor(0.45f, 0.65f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.4f));

	DrawText(ElementLine, ElementLineColour, RosterX, RowY + 6.0f, Font, 1.0f);

	// context hint under the crosshair
	// 크로스헤어 아래의 상황별 힌트
	FString Hint;
	FLinearColor HintColour(0.55f, 0.55f, 0.6f);

	if (Player->GetChallengeTarget())
	{
		Hint = TEXT("[F] Shout: drop the weapon");
		HintColour = FLinearColor(1.0f, 0.9f, 0.4f);
	}
	else if (const ADoorwayMarker* Doorway = Player->GetAimedDoorway())
	{
		Hint = FString::Printf(TEXT("%s    [1] Stack    [2] Clear"), *Doorway->GetDisplayName());
		HintColour = FLinearColor(1.0f, 0.9f, 0.4f);
	}
	else
	{
		Hint = TEXT("[Z] Follow   [H] Hold   [3] Watch   wheel: pick element");
	}

	const float Width = Hint.Len() * 8.5f;
	DrawText(Hint, HintColour, CentreX - Width * 0.5f, Canvas->SizeY - 145.0f, Font, 1.0f);
}

//~ Debug console commands ----------------------------------------------------
//~ Debug console commands / 디버그 콘솔 명령 ------------------------------------
