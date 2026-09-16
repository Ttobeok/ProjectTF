// CQB Sample - a doorway the squad can stack on and clear through.

#include "DoorwayMarker.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h"
#include "ProjectTF.h"

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

	if (bHighlight)
	{
		DrawHighlight(DeltaSeconds);
	}

	if (!bDrawDebug)
	{
		return;
	}

	const UWorld* World = GetWorld();
	DrawDebugSphere(World, GetStackPoint(EStackSide::Left), 25.0f, 8, FColor::Green, false, -1.0f);
	DrawDebugSphere(World, GetStackPoint(EStackSide::Right), 25.0f, 8, FColor::Blue, false, -1.0f);
	DrawDebugSphere(World, GetClearPoint(), 35.0f, 8, FColor::Yellow, false, -1.0f);
	DrawDebugLine(World, GetActorLocation(), GetClearPoint(), FColor::Yellow, false, -1.0f);
}

void ADoorwayMarker::DrawHighlight(float DeltaSeconds) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor Colour = bAimedAt ? AimedColour : IdleColour;
	const float Thickness = bAimedAt ? 4.0f : 2.0f;

	// the opening itself: a rectangle standing in the doorway, across the entry direction
	const FVector Base = GetActorLocation();
	const FVector Side = GetActorRightVector() * FrameHalfWidth;
	const FVector Up = FVector::UpVector * FrameHeight;

	const FVector BottomLeft = Base - Side;
	const FVector BottomRight = Base + Side;
	const FVector TopLeft = BottomLeft + Up;
	const FVector TopRight = BottomRight + Up;

	DrawDebugLine(World, BottomLeft, TopLeft, Colour, false, -1.0f, 0, Thickness);
	DrawDebugLine(World, BottomRight, TopRight, Colour, false, -1.0f, 0, Thickness);
	DrawDebugLine(World, TopLeft, TopRight, Colour, false, -1.0f, 0, Thickness);
	DrawDebugLine(World, BottomLeft, BottomRight, Colour, false, -1.0f, 0, Thickness);

	if (!bAimedAt)
	{
		return;
	}

	// aimed at, so show what an order would actually do: where each half of the squad would
	// stack, and where Clear would send them
	DrawDebugSphere(World, GetStackPoint(EStackSide::Left), 22.0f, 12, AimedColour, false, -1.0f, 0, 2.0f);
	DrawDebugSphere(World, GetStackPoint(EStackSide::Right), 22.0f, 12, AimedColour, false, -1.0f, 0, 2.0f);

	DrawDebugLine(World, Base, GetClearPoint(), AimedColour, false, -1.0f, 0, 2.0f);
	DrawDebugSphere(World, GetClearPoint(), 30.0f, 12, AimedColour, false, -1.0f, 0, 2.0f);
}
