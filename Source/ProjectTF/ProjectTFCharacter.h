// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Variant_Shooter/Weapons/ShooterWeaponHolder.h"
#include "ProjectTFCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UHealthComponent;
class UWeaponComponent;
class UAnimInstance;
class AShooterWeapon;
class UNavigationInvokerComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character.
 *  Carries the CQB health and hitscan weapon components and handles fire / ADS / reload / lean input.
 */
UCLASS(abstract)
class AProjectTFCharacter : public ACharacter, public IShooterWeaponHolder
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

	/**
	 *  Weapon actor used for the visuals: meshes, hand poses and the firing montage.
	 *  The gameplay lives in UWeaponComponent; this only supplies what the player sees.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	TSubclassOf<AShooterWeapon> WeaponVisualClass;

	/** Loaded on BeginPlay when no class is set. Never load this from the constructor. */
	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	TSoftClassPtr<AShooterWeapon> WeaponVisualAsset = TSoftClassPtr<AShooterWeapon>(FSoftObjectPath(TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Rifle.BP_ShooterWeapon_Rifle_C")));

	/** Socket the third person weapon mesh attaches to */
	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	FName WeaponSocket = FName("HandGrip_R");

	/** Where the weapon sits relative to the camera */
	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	FVector WeaponViewOffset = FVector(42.0f, 13.0f, -16.0f);

	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	// the weapon mesh is authored for a hand socket, so its muzzle needs turning to face forward
	FRotator WeaponViewRotation = FRotator(0.0f, 90.0f, 0.0f);

	/** How far the weapon is shoved back on each shot, in cm */
	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	float FireKickDistance = 5.0f;

	/** How far the muzzle rises on each shot, in degrees */
	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	float FireKickPitch = 6.0f;

	/** How fast the weapon settles back after a shot */
	UPROPERTY(EditDefaultsOnly, Category="Weapon Visual")
	float FireKickRecoverySpeed = 9.0f;

	/** Spawned weapon actor */
	UPROPERTY(Transient)
	TObjectPtr<AShooterWeapon> WeaponVisual;

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

	/** Called on every shot so the weapon can be kicked back */
	UFUNCTION()
	void OnWeaponAmmoChanged(int32 CurrentAmmo, int32 MagSize);

	/** Current amount of fire kick left, 0 to 1 */
	float FireKickAlpha = 0.0f;

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

	//~Begin IShooterWeaponHolder interface. Only the visual half is used; the firing logic,
	//~ammo and recoil all live in UWeaponComponent.
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;
	virtual void AddWeaponRecoil(float Recoil) override;
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;
	virtual FVector GetWeaponTargetLocation() override;
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;
	virtual void OnSemiWeaponRefire() override;
	//~End IShooterWeaponHolder interface

};
