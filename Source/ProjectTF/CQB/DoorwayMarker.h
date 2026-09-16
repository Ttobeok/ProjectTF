// CQB Sample - a doorway the squad can stack on and clear through.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CQBTypes.h"
#include "DoorwayMarker.generated.h"

class UArrowComponent;

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

	/**
	 *  How far to either side the stack points sit.
	 *  A 2 m opening leaves no room to stand beside the frame, so this stays well inside half
	 *  the corridor width; going wider puts the squad inside the wall.
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float StackOffset = 55.0f;

	/** How far back from the doorway the squad waits */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float StackSetback = 110.0f;

	/** How far past the doorway the room point sits */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float ClearDistance = 450.0f;

	/** Shown in the aiming hint. Falls back to the actor label. */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	FString DoorwayName;

	/** Draw the stack and clear points while playing */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	bool bDrawDebug = false;

	/**
	 *  Told every frame by the player whether its crosshair is on this doorway.
	 *
	 *  The marker does not go looking for the player: a doorway that knew about the player
	 *  character would drag the whole player class into a file that otherwise only knows about
	 *  points in space.
	 */
	void SetAimedAt(bool bInAimedAt) { bAimedAt = bInAimedAt; }

	/** Outline the opening so the player can see which doorways take orders */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	bool bHighlight = true;

	/** Half the width of the drawn opening, in cm. The level's doorways are 2 m. */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	float FrameHalfWidth = 88.0f;

	/** Height of the drawn opening, in cm */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	float FrameHeight = 210.0f;

	/** Colour when the doorway is just sitting there, available */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	FColor IdleColour = FColor(0, 120, 255);

	/** Colour when the player is aiming at it, matching the hint text on the HUD */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	FColor AimedColour = FColor(255, 130, 0);

protected:

	virtual void Tick(float DeltaSeconds) override;

	FString GetActorLabelSafe() const;

	/** Draws the opening, and where an order would send the squad */
	void DrawHighlight(float DeltaSeconds) const;

	/** True while the player's crosshair is on this doorway */
	bool bAimedAt = false;

	/** Entry direction, drawn in the editor */
	UPROPERTY(VisibleAnywhere, Category = "Doorway")
	UArrowComponent* EntryArrow;
};
