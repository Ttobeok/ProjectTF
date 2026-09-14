// CQB Sample - a doorway the squad can stack on and clear through.

#include "DoorwayMarker.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h"

ADoorwayMarker::ADoorwayMarker()
{
	PrimaryActorTick.bCanEverTick = true;

	EntryArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Entry Arrow"));
	RootComponent = EntryArrow;
	EntryArrow->ArrowColor = FColor::Cyan;
	EntryArrow->ArrowSize = 2.5f;

}

FVector ADoorwayMarker::GetStackPoint(EStackSide Side) const
{
	const float SideSign = (Side == EStackSide::Left) ? -1.0f : 1.0f;

	// beside the opening and a step back from it, so the squad is not standing in the doorway
	return GetActorLocation()
		+ GetActorRightVector() * (StackOffset * SideSign)
		- GetActorForwardVector() * StackSetback;
}

FVector ADoorwayMarker::GetClearPoint() const
{
	return GetActorLocation() + GetActorForwardVector() * ClearDistance;
}

FString ADoorwayMarker::GetActorLabelSafe() const
{
#if WITH_EDITOR
	return GetActorLabel();
#else
	return GetName();
#endif
}

void ADoorwayMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bDrawDebug)
	{
		return;
	}

	const UWorld* World = GetWorld();
	DrawDebugSphere(World, GetStackPoint(EStackSide::Left), 25.0f, 8, FColor::Green, false, DeltaSeconds);
	DrawDebugSphere(World, GetStackPoint(EStackSide::Right), 25.0f, 8, FColor::Blue, false, DeltaSeconds);
	DrawDebugSphere(World, GetClearPoint(), 35.0f, 8, FColor::Yellow, false, DeltaSeconds);
	DrawDebugLine(World, GetActorLocation(), GetClearPoint(), FColor::Yellow, false, DeltaSeconds);
}
