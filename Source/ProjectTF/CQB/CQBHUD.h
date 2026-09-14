// CQB Sample - crosshair and ammo readout, drawn from C++.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CQBHUD.generated.h"

class UWeaponComponent;
class UHealthComponent;

/**
 *  Draws the crosshair and a small ammo and health readout.
 *  Everything is drawn with the canvas, so the HUD needs no assets or widgets.
 */
UCLASS()
class PROJECTTF_API ACQBHUD : public AHUD
{
	GENERATED_BODY()

public:

	ACQBHUD();

	virtual void DrawHUD() override;

protected:

	/** Four ticks around a centre dot. The gap grows with spread. */
	void DrawCrosshair(const UWeaponComponent* Weapon);

	/** Ammo counter bottom right, health bottom left */
	void DrawReadout(const UWeaponComponent* Weapon, const UHealthComponent* Health);

	/** Squad orders along the bottom, and the doorway hint when one is under the crosshair */
	void DrawSquadBar();

	/** Length of each crosshair tick, in pixels */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float TickLength = 10.0f;

	/** Thickness of each crosshair tick, in pixels */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float TickThickness = 2.0f;

	/** Gap from the centre while hip firing, in pixels */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float HipGap = 14.0f;

	/** Gap from the centre while aiming, in pixels */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float ADSGap = 5.0f;

	/** Extra gap added briefly after each shot, in pixels */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float FireKickGap = 12.0f;

	/** How fast the gap settles back down */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float GapInterpSpeed = 9.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	/** Turns red for a moment when a shot connects */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor HitMarkerColor = FLinearColor(1.0f, 0.25f, 0.2f, 1.0f);

	/** Called when the player weapon hits something, to flash the hit marker */
	UFUNCTION()
	void OnWeaponHit(AActor* HitActor, float DamageDealt);

	/** Called when the player weapon ammo changes, to kick the crosshair open */
	UFUNCTION()
	void OnAmmoChanged(int32 CurrentAmmo, int32 MagSize);

	/** Hooks the player weapon delegates once the pawn exists */
	void BindToPlayerWeapon();

	/** Current interpolated crosshair gap */
	float CurrentGap = 14.0f;

	/** Extra gap left over from the last shot */
	float FireKick = 0.0f;

	/** Seconds of hit marker left */
	float HitMarkerTime = 0.0f;

	/** Weapon the delegates are bound to */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWeaponComponent> BoundWeapon;
};
