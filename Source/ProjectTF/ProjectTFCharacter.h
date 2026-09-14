// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ProjectTFCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UHealthComponent;
class UWeaponComponent;
class UStaticMeshComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character.
 *  Carries the CQB health and hitscan weapon components and handles fire / ADS / reload / lean input.
 */
UCLASS(abstract)
class AProjectTFCharacter : public ACharacter
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

	/** Visible weapon, attached to the first person hands */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WeaponMesh;

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

	/** Returns the visible weapon mesh **/
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

};
