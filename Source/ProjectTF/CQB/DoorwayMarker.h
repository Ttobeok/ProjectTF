// CQB Sample - a doorway the squad can stack on and clear through.
// CQB 샘플 - 분대가 붙어 대기하고 통과해 소타할 수 있는 문.

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
 *
 *  분대가 다룰 수 있는 개구부를 표시합니다 — 어디에 붙을지, 어느 쪽이 안쪽인지.
 *
 *  액터의 forward 벡터가 진입 방향입니다. 에디터에서 회전만 시키면 문이 그 너머
 *  방을 향하게 됩니다. 개구부마다 하나씩 배치하세요.
 */
UCLASS()
class PROJECTTF_API ADoorwayMarker : public AActor
{
	GENERATED_BODY()

public:

	ADoorwayMarker();

	/**
	 *  Stack point beside the doorway, on the near side
	 *  문 옆, 이쪽 면의 대기 지점
	 */
	UFUNCTION(BlueprintPure, Category = "Doorway")
	FVector GetStackPoint(EStackSide Side) const;

	/**
	 *  Point inside the room the doorway leads to
	 *  문이 이어지는 방 안의 지점
	 */
	UFUNCTION(BlueprintPure, Category = "Doorway")
	FVector GetClearPoint() const;

	/**
	 *  Name shown when the player aims at this doorway
	 *  플레이어가 이 문을 조준했을 때 표시되는 이름
	 */
	UFUNCTION(BlueprintPure, Category = "Doorway")
	FString GetDisplayName() const { return DoorwayName.IsEmpty() ? GetActorLabelSafe() : DoorwayName; }

	/**
	 *  How far to either side the stack points sit.
	 *  A 2 m opening leaves no room to stand beside the frame, so this stays well inside half
	 *  the corridor width; going wider puts the squad inside the wall.
	 *
	 *  대기 지점이 좌우로 얼마나 벌어지는지.
	 *  2m 개구부는 문틀 옆에 설 자리가 없으므로, 복도 폭의 절반보다 훨씬 안쪽으로
	 *  잡습니다. 더 벌리면 분대가 벽 속에 서게 됩니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float StackOffset = 55.0f;

	/**
	 *  How far back from the doorway the squad waits
	 *  분대가 문에서 얼마나 물러나 대기하는지
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float StackSetback = 110.0f;

	/**
	 *  How far past the doorway the room point sits
	 *  방 안 지점이 문에서 얼마나 안쪽에 있는지
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	float ClearDistance = 450.0f;

	/**
	 *  Shown in the aiming hint. Falls back to the actor label.
	 *  조준 힌트에 표시됩니다. 비워두면 액터 라벨을 씁니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	FString DoorwayName;

	/**
	 *  Draw the stack and clear points while playing
	 *  플레이 중 대기·소타 지점을 그릴지
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway")
	bool bDrawDebug = false;

	/**
	 *  Told every frame by the player whether its crosshair is on this doorway.
	 *
	 *  The marker does not go looking for the player: a doorway that knew about the player
	 *  character would drag the whole player class into a file that otherwise only knows about
	 *  points in space.
	 *
	 *  플레이어가 매 프레임 이 문을 조준 중인지 알려줍니다.
	 *
	 *  마커가 플레이어를 찾아가지 않습니다. 문이 플레이어 캐릭터를 알게 되면,
	 *  공간상의 점만 알면 되는 파일에 플레이어 클래스 전체가 딸려 들어옵니다.
	 */
	void SetAimedAt(bool bInAimedAt) { bAimedAt = bInAimedAt; }

	/**
	 *  Outline the opening so the player can see which doorways take orders
	 *  개구부를 테두리로 그려, 어느 문에 명령을 내릴 수 있는지 보여줍니다
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	bool bHighlight = true;

	/**
	 *  Half the width of the drawn opening, in cm. The level's doorways are 2 m.
	 *  그려지는 개구부 폭의 절반(cm). 이 레벨의 문은 2m입니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	float FrameHalfWidth = 88.0f;

	/**
	 *  Height of the drawn opening, in cm
	 *  그려지는 개구부의 높이(cm)
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	float FrameHeight = 210.0f;

	/**
	 *  Colour when the doorway is just sitting there, available
	 *  그냥 놓여 있을 때의 색. 명령 가능하다는 표시입니다
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	FColor IdleColour = FColor(0, 120, 255);

	/**
	 *  Colour when the player is aiming at it, matching the hint text on the HUD
	 *  플레이어가 조준 중일 때의 색. HUD 힌트 문구와 맞춥니다
	 */
	UPROPERTY(EditAnywhere, Category = "Doorway|Highlight")
	FColor AimedColour = FColor(255, 130, 0);

protected:

	virtual void Tick(float DeltaSeconds) override;

	FString GetActorLabelSafe() const;

	/**
	 *  Draws the opening, and where an order would send the squad
	 *  개구부와, 명령이 분대를 보낼 위치를 그립니다
	 */
	void DrawHighlight(float DeltaSeconds) const;

	/**
	 *  True while the player's crosshair is on this doorway
	 *  플레이어 크로스헤어가 이 문에 있는 동안 true
	 */
	bool bAimedAt = false;

	/**
	 *  Entry direction, drawn in the editor
	 *  진입 방향. 에디터에서 그려집니다
	 */
	UPROPERTY(VisibleAnywhere, Category = "Doorway")
	UArrowComponent* EntryArrow;
};
