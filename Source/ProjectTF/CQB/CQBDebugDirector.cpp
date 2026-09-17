// CQB Sample - every debug hook, in one place and out of the gameplay classes.
// CQB 샘플 - 모든 디버그 훅을 한곳에, 게임플레이 클래스 바깥에.

#include "CQBDebugDirector.h"
#include "CQBCharacter.h"
#include "AllyAIController.h"
#include "EnemyAIController.h"
#include "DoorwayMarker.h"
#include "HealthComponent.h"
#include "ProjectTFCharacter.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "ProjectTF.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"

ACQBDebugDirector::ACQBDebugDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

void ACQBDebugDirector::EnsureExists(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return;
	}

	for (TActorIterator<ACQBDebugDirector> It(World); It; ++It)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	World->SpawnActor<ACQBDebugDirector>(ACQBDebugDirector::StaticClass(), FTransform::Identity, SpawnParams);
}

void ACQBDebugDirector::BeginPlay()
{
	Super::BeginPlay();

	ArmFromCommandLine();
}

void ACQBDebugDirector::ArmFromCommandLine()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FTimerManager& Timers = GetWorld()->GetTimerManager();

	// the player pawn is not possessed yet at BeginPlay, so give it a moment
	// BeginPlay 시점에는 플레이어 폰이 아직 빙의되지 않았으므로 잠깐 여유를 줍니다
	Timers.SetTimer(PlaceTimer, this, &ACQBDebugDirector::PlacePlayer, 0.5f, false);

	float DamageAfter = 0.0f;
	if (FParse::Value(CommandLine, TEXT("CQBKillEnemyAfter="), DamageAfter) && DamageAfter > 0.0f)
	{
		FParse::Value(CommandLine, TEXT("CQBKillCount="), DamageCount);
		FParse::Value(CommandLine, TEXT("CQBKillDamage="), DamageFraction);

		FString Side;
		if (FParse::Value(CommandLine, TEXT("CQBKillSide="), Side) && Side.Equals(TEXT("ally"), ESearchCase::IgnoreCase))
		{
			DamageSide = ECQBFaction::Ally;
		}

		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: hitting %d suspect(s) for %.0f%% in %.1fs"),
			DamageCount, DamageFraction * 100.0f, DamageAfter);

		Timers.SetTimer(DamageTimer, this, &ACQBDebugDirector::RunDamage, DamageAfter, false);
	}

	float OrderAfter = 0.0f;
	if (FParse::Value(CommandLine, TEXT("CQBOrderAfter="), OrderAfter) && OrderAfter > 0.0f)
	{
		FParse::Value(CommandLine, TEXT("CQBOrder="), OrderName);
		FParse::Value(CommandLine, TEXT("CQBOrderDoor="), OrderDoorIndex);

		Timers.SetTimer(OrderTimer, this, &ACQBDebugDirector::RunOrder, OrderAfter, false);
	}

	float TraceInterval = 0.0f;
	if (FParse::Value(CommandLine, TEXT("CQBTraceSquad="), TraceInterval) && TraceInterval > 0.0f)
	{
		Timers.SetTimer(TraceSquadTimer, this, &ACQBDebugDirector::RunTraceSquad, TraceInterval, true);
	}

	float ScreenshotAfter = 0.0f;
	if (FParse::Value(CommandLine, TEXT("CQBScreenshotAfter="), ScreenshotAfter) && ScreenshotAfter > 0.0f)
	{
		Timers.SetTimer(ScreenshotTimer, this, &ACQBDebugDirector::RunScreenshot, ScreenshotAfter, false);
	}
}

TArray<ACQBCharacter*> ACQBDebugDirector::GatherSuspects() const
{
	const AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	TArray<ACQBCharacter*> Suspects;
	for (TActorIterator<ACQBCharacter> It(GetWorld()); It; ++It)
	{
		ACQBCharacter* Character = *It;
		if (Character && !Character->IsDead() && !Character->IsSurrendered()
			&& Character->GetFaction() == DamageSide)
		{
			Suspects.Add(Character);
		}
	}

	if (Player)
	{
		const FVector From = Player->GetActorLocation();
		Suspects.Sort([From](const ACQBCharacter& A, const ACQBCharacter& B)
		{
			return FVector::DistSquared(A.GetActorLocation(), From) < FVector::DistSquared(B.GetActorLocation(), From);
		});
	}

	return Suspects;
}

TArray<ADoorwayMarker*> ACQBDebugDirector::GatherDoorways() const
{
	TArray<ADoorwayMarker*> Doorways;
	for (TActorIterator<ADoorwayMarker> It(GetWorld()); It; ++It)
	{
		Doorways.Add(*It);
	}

	Doorways.Sort([](const ADoorwayMarker& A, const ADoorwayMarker& B)
	{
		return A.GetActorLocation().X < B.GetActorLocation().X;
	});

	return Doorways;
}

void ACQBDebugDirector::PlacePlayer()
{
	FString Spot;
	// the last argument keeps FParse from stopping at the commas
	// 마지막 인자가 FParse가 쉼표에서 멈추지 않게 해줍니다
	if (!FParse::Value(FCommandLine::Get(), TEXT("CQBPlayerAt="), Spot, false))
	{
		return;
	}

	TArray<FString> Parts;
	Spot.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() != 3)
	{
		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: -CQBPlayerAt wants X,Y,Z but got '%s'"), *Spot);
		return;
	}

	const FVector Location(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]));

	if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Player->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: player placed at %s"), *Location.ToCompactString());
	}
}

void ACQBDebugDirector::RunDamage()
{
	int32 Remaining = DamageCount;

	for (ACQBCharacter* Suspect : GatherSuspects())
	{
		if (Remaining <= 0)
		{
			break;
		}

		if (UHealthComponent* Health = Suspect->GetHealthComponent())
		{
			UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: hitting %s"), *Suspect->GetName());
			Health->TakeDamage(Health->MaxHealth * DamageFraction, this, nullptr);
			--Remaining;
		}
	}
}

void ACQBDebugDirector::RunOrder()
{
	AProjectTFCharacter* Player = Cast<AProjectTFCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player)
	{
		return;
	}

	const TArray<ADoorwayMarker*> Doorways = GatherDoorways();
	ADoorwayMarker* Doorway = Doorways.IsValidIndex(OrderDoorIndex) ? Doorways[OrderDoorIndex] : nullptr;

	UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: scripted order '%s' on %s"),
		*OrderName, Doorway ? *Doorway->GetDisplayName() : TEXT("no doorway"));

	// a shout needs a suspect rather than a doorway
	// 외침은 문이 아니라 용의자를 대상으로 합니다
	if (OrderName == TEXT("challenge"))
	{
		const TArray<ACQBCharacter*> Suspects = GatherSuspects();
		if (Suspects.Num() > 0)
		{
			Player->IssueChallenge(Suspects[0]);
		}
		return;
	}

	Player->IssueSquadOrder(OrderName, Doorway);
}

void ACQBDebugDirector::RunTraceSquad()
{

	for (TActorIterator<AAllyAIController> It(GetWorld()); It; ++It)
	{
		const AAllyAIController* Member = *It;
		const APawn* Body = Member ? Member->GetPawn() : nullptr;
		if (!Body)
		{
			continue;
		}

		FString PathText = TEXT("no path");
		if (const UPathFollowingComponent* Follow = Member->GetPathFollowingComponent())
		{
			if (const FNavPathSharedPtr Path = Follow->GetPath(); Path.IsValid() && Path->GetPathPoints().Num() > 0)
			{
				const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
				PathText = FString::Printf(TEXT("%d pts, len %.0f, next %s, goal %s"),
					Points.Num(), Path->GetLength(),
					*Points[FMath::Min(1, Points.Num() - 1)].Location.ToCompactString(),
					*Points.Last().Location.ToCompactString());
			}
		}

		UE_LOG(LogProjectTF, Warning, TEXT("CQB trace: %s  %-8s at %s  status %d  %s"),
			*Member->GetDisplayName(), *FCQBNames::StateToString(Member->GetState()),
			*Body->GetActorLocation().ToCompactString(),
			static_cast<int32>(Member->GetMoveStatus()), *PathText);
	}
}

void ACQBDebugDirector::RunScreenshot()
{
	UE_LOG(LogProjectTF, Warning, TEXT("CQB debug: taking a screenshot"));

	// with UI, or the shot is useless: the HUD draws to Canvas but DrawDebugString goes to the
	// separate DebugCanvas, and a no-UI capture silently drops the state text over each pawn
	// UI를 포함해야 합니다. 안 그러면 쓸모없는 스크린샷이 됩니다 — HUD는 Canvas에 그리지만
	// DrawDebugString은 별개인 DebugCanvas에 그려서, UI 없는 캡처는 폰 위의 상태 텍스트를
	// 조용히 빠뜨립니다
	FScreenshotRequest::RequestScreenshot(true);
}
