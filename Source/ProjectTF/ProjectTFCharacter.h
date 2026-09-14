// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CQB/CQBTypes.h"
#include "Perception/AISightTargetInterface.h"
#include "ProjectTFCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UHealthComponent;
class UWeaponComponent;
class UWeaponVisualComponent;
class ADoorwayMarker;
class AAllyAIController;
class UNavigationInvokerComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character.
 *  Carries the CQB health and hitscan weapon components and handles fire / ADS / reload / lean input.
 */
UCLASS(abstract)
class AProjectTFCharacter : public ACharacter, public ICQBFactionAgent, public IAISightTargetInterface
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Health pool shared with the AI enemies */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UHealthComponent* HealthComponent;

	/** Hitscan weapon shared with the AI enemies */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWeaponComponent* WeaponComponent;

	/** The weapon you can see, as a first person view model */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWeaponVisualComponent* WeaponVisual;

	/** Keeps navmesh tiles generated around this character at runtime */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UNavigationInvokerComponent* NavigationInvoker;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Fire Input Action. Optional: left mouse is bound directly when this is empty. */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBFireAction;

	/** Aim down sights Input Action. Optional: right mouse is bound directly when this is empty. */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBADSAction;

	/** Reload Input Action. Optional: R is bound directly when this is empty. */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBReloadAction;

	/** Lean left Input Action. Optional: Q is bound directly when this is empty. */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBLeanLeftAction;

	/** Lean right Input Action. Optional: E is bound directly when this is empty. */
	UPROPERTY(EditAnywhere, Category ="Input|CQB")
	UInputAction* CQBLeanRightAction;

	/** Camera roll applied at full lean, in degrees */
	UPROPERTY(EditAnywhere, Category ="Lean")
	float LeanRollAngle = 14.0f;

	/** Sideways camera offset applied at full lean, in cm */
	UPROPERTY(EditAnywhere, Category ="Lean")
	float LeanOffsetDistance = 45.0f;

	/** How fast the lean blends in and out */
	UPROPERTY(EditAnywhere, Category ="Lean")
	float LeanInterpSpeed = 8.0f;

public:
	AProjectTFCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** Current camera roll from leaning, in degrees. Read by the camera manager. */
	UFUNCTION(BlueprintPure, Category="Lean")
	float GetLeanRoll() const { return CurrentLeanRoll; }

	/** Current sideways camera offset from leaning, in cm. Read by the camera manager. */
	UFUNCTION(BlueprintPure, Category="Lean")
	float GetLeanOffset() const { return CurrentLeanOffset; }

	UFUNCTION(BlueprintPure, Category="Components")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category="Components")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	//~Begin ICQBFactionAgent
	virtual ECQBFaction GetFaction() const override { return ECQBFaction::Player; }
	//~End ICQBFactionAgent

	//~Begin IAISightTargetInterface
	virtual UAISense_Sight::EVisibilityResult CanBeSeenFrom(const FCanBeSeenFromContext& Context,
		FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested,
		float& OutSightStrength, int32* UserData = nullptr,
		const FOnPendingVisibilityQueryProcessedDelegate* Delegate = nullptr) override;
	//~End IAISightTargetInterface

	/** Doorway the player is currently aiming at, or null. Read by the HUD for the order hint. */
	UFUNCTION(BlueprintPure, Category="Squad")
	ADoorwayMarker* GetAimedDoorway() const { return AimedDoorway; }

	/** Every living squad member, for the HUD */
	TArray<AAllyAIController*> GetSquad() const;

	/** Squad members the current element selection would take an order */
	TArray<AAllyAIController*> GetSelectedSquad() const;

	/** Which element the player is commanding right now */
	UFUNCTION(BlueprintPure, Category="Squad")
	ESquadElement GetSelectedElement() const { return SelectedElement; }

	/** Suspect under the crosshair that could be shouted at, or null */
	UFUNCTION(BlueprintPure, Category="Squad")
	AActor* GetChallengeTarget() const { return ChallengeTarget; }

	/** Issues a named order to the selected element. Used by the input bindings and by tests. */
	UFUNCTION(BlueprintCallable, Category="Squad")
	void IssueSquadOrder(const FString& OrderName, ADoorwayMarker* Doorway);

	/** Shouts at a specific suspect, whatever the crosshair is on */
	UFUNCTION(BlueprintCallable, Category="Squad")
	void IssueChallenge(AActor* Suspect);

protected:

	virtual void BeginPlay() override;

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Pulls the trigger */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoFireStart();

	/** Releases the trigger */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoFireStop();

	/** Starts aiming down sights */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoADSStart();

	/** Stops aiming down sights */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoADSStop();

	/** Requests a reload */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoReload();

	/** Sets the lean target. -1 leans left, 1 leans right, 0 straightens up. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoLean(float Direction);

	//~ Squad commands. Z falls them in, H holds them, and aiming at a doorway turns 1 and 2
	//~ into stack and clear orders for that doorway.
	void CommandFollow();
	void CommandHold();
	void CommandStackOrOne();
	void CommandClearOrTwo();

	/** Orders the selected element to watch the point under the crosshair */
	void CommandWatch();

	/** Shouts at the suspect under the crosshair to give up */
	void CommandChallenge();

	/** Mouse wheel cycles Red, Blue and the whole squad */
	void CycleElementUp();
	void CycleElementDown();

	/** Traces from the camera for a doorway the player might be ordering against */
	void UpdateAimedDoorway();

	/** Traces for a suspect the player could shout at */
	void UpdateChallengeTarget();

	/** Point under the crosshair, for move and watch orders. Returns false when nothing is hit. */
	bool GetAimedPoint(FVector& OutPoint) const;



	/** How far the player can be from a doorway and still give orders about it */
	UPROPERTY(EditDefaultsOnly, Category="Squad")
	float DoorwayAimRange = 1200.0f;

	/** How far off centre the crosshair can be and still count as aiming at a doorway */
	UPROPERTY(EditDefaultsOnly, Category="Squad")
	float DoorwayAimAngle = 18.0f;

	/** Doorway under the crosshair this frame */
	UPROPERTY(Transient)
	TObjectPtr<ADoorwayMarker> AimedDoorway;

	/** Suspect under the crosshair this frame, if one is close enough to shout at */
	UPROPERTY(Transient)
	TObjectPtr<AActor> ChallengeTarget;

	/** Element the orders go to */
	ESquadElement SelectedElement = ESquadElement::All;

	/** How far a shouted demand carries */
	UPROPERTY(EditDefaultsOnly, Category="Squad")
	float ChallengeRange = 1500.0f;

	void LeanLeftStart() { DoLean(-1.0f); }
	void LeanRightStart() { DoLean(1.0f); }
	void LeanStop() { DoLean(0.0f); }

	/** Prints the damage taken and stops the player from acting once dead */
	UFUNCTION()
	void OnHealthChanged(UHealthComponent* HealthComp, float NewHealth, float Delta, AActor* Causer);

	UFUNCTION()
	void OnPlayerDeath(AActor* DeadActor, AActor* Killer);

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	/** Where the lean is heading: -1, 0 or 1 */
	float LeanTarget = 0.0f;

	/** Current interpolated lean roll, in degrees */
	float CurrentLeanRoll = 0.0f;

	/** Current interpolated lean offset, in cm */
	float CurrentLeanOffset = 0.0f;

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Returns the visible weapon **/
	UWeaponVisualComponent* GetWeaponVisual() const { return WeaponVisual; }

};
