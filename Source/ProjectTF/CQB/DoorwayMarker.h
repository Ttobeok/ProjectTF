// CQB Sample - a doorway the squad can stack on and clear through.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CQBTypes.h"
#include "DoorwayMarker.generated.h"

class UArrowComponent;
class UBoxComponent;

/**
 *  Marks an opening the squad can work: where to stack, and which way is through.
 *
 *  The actor's forward vector is the entry direction, so rotating it in the editor is all it
 *  takes to point a doorway into the room beyond. Place one in each opening.
 */
UCLASS()
class PROJECTTF_API ADoorwayMarker : public AActor
{
	GENERATED_BODY()

public:

	ADoorwayMarker();

	/** Stack point beside the doorway, on the near side */
	UFUNCTION(BlueprintPure, Category = "Doorway")
	FVector GetStackPoint(EStackSide Side) const;

	/** Point inside the room the doorway leads to */
	UFUNCTION(BlueprintPure, Category = "Doorway")
	FVector GetClearPoint() const;

	/** Name shown when the player aims at this doorway */
	UFUNCTION(BlueprintPure, Category = "Doorway")
	FString GetDisplayName() const { return DoorwayName.IsEmpty() ? GetActorLabelSafe() : DoorwayName; }

	/** How far to either side the stack points sit */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float StackOffset = 100.0f;

	/** How far back from the doorway the squad waits */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float StackSetback = 60.0f;

	/** How far past the doorway the room point sits */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float ClearDistance = 450.0f;

	/** Shown in the aiming hint. Falls back to the actor label. */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	FString DoorwayName;

	/** Draw the stack and clear points while playing */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	bool bDrawDebug = false;

protected:

	virtual void Tick(float DeltaSeconds) override;

	FString GetActorLabelSafe() const;

	/** Entry direction, drawn in the editor */
	UPROPERTY(VisibleAnywhere, Category = "Doorway")
	UArrowComponent* EntryArrow;

	/** What the player's aim trace hits */
	UPROPERTY(VisibleAnywhere, Category = "Doorway")
	UBoxComponent* AimTarget;
};
