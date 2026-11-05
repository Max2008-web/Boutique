// STRIKE ZONE - commandes tactiles (joystick virtuel, caméra tactile, boutons) sans asset UMG
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StrikeTouchControls.generated.h"

class APlayerController;

/** Bouton tactile circulaire (coordonnées normalisées : X = largeur, Y = hauteur, rayon = hauteur). */
USTRUCT()
struct FStrikeTouchButton
{
	GENERATED_BODY()

	UPROPERTY() FName Id;
	UPROPERTY() FString Label;
	UPROPERTY() FVector2D Center = FVector2D::ZeroVector;
	UPROPERTY() float Radius = 0.05f;
	/** Un glisser du doigt sur ce bouton fait aussi tourner la caméra (tir + visée simultanés). */
	UPROPERTY() bool bAllowsLook = false;

	bool bPressed = false;
	int32 Finger = INDEX_NONE;
};

/** Événement bouton produit par la mise à jour (pression ou relâchement). */
struct FStrikeTouchEvent
{
	FName Id;
	bool bPressed = false;
};

/**
 * Lecture des doigts par sondage (APlayerController::GetInputTouchState), indépendante du système d'entrée.
 * - Moitié gauche : joystick dynamique (apparaît sous le doigt). Poussé à fond vers l'avant = sprint.
 * - Moitié droite : glisser = caméra. Les boutons ont priorité sur les zones.
 * Le HUD dessine l'interface à partir de cet état ; l'échelle suit la hauteur de l'écran (interface adaptative).
 */
UCLASS()
class STRIKEZONE_API UStrikeTouchControls : public UObject
{
	GENERATED_BODY()

public:
	UStrikeTouchControls();

	/** Met à jour l'état des doigts. Retourne les événements de boutons de cette frame. */
	void Update(APlayerController* PC, const FVector2D& ViewportSize, TArray<FStrikeTouchEvent>& OutEvents);

	/** Réinitialise tous les doigts (perte de focus, changement de pion). */
	void ResetAll();

	FVector2D GetMoveAxis() const { return MoveAxis; }
	/** Déplacement du doigt de caméra cette frame, en pixels (Y écran vers le bas). */
	FVector2D ConsumeLookDelta();
	bool WantsSprint() const { return MoveAxis.Y > 0.92f; }

	// --- Lecture pour le HUD ---
	const TArray<FStrikeTouchButton>& GetButtons() const { return Buttons; }
	bool IsJoystickActive() const { return JoystickFinger != INDEX_NONE; }
	FVector2D GetJoystickOrigin() const { return JoystickOrigin; }
	FVector2D GetJoystickThumb() const { return JoystickThumb; }
	float GetJoystickRadiusPixels(const FVector2D& ViewportSize) const { return JoystickRadius * ViewportSize.Y; }
	FVector2D GetDefaultJoystickCenter(const FVector2D& ViewportSize) const { return FVector2D(0.15f * ViewportSize.X, 0.72f * ViewportSize.Y); }
	bool HasSeenTouch() const { return bHasSeenTouch; }

	/** Le premier doigt posé cette frame (menus). */
	bool ConsumeTap(FVector2D& OutPosition);

protected:
	int32 HitButton(const FVector2D& Position, const FVector2D& ViewportSize) const;

	UPROPERTY()
	TArray<FStrikeTouchButton> Buttons;

	static constexpr int32 MaxFingers = 10;
	bool bFingerDown[MaxFingers];
	FVector2D FingerLast[MaxFingers];

	int32 JoystickFinger = INDEX_NONE;
	int32 LookFinger = INDEX_NONE;
	FVector2D JoystickOrigin = FVector2D::ZeroVector;
	FVector2D JoystickThumb = FVector2D::ZeroVector;
	float JoystickRadius = 0.11f;
	FVector2D MoveAxis = FVector2D::ZeroVector;
	FVector2D LookDelta = FVector2D::ZeroVector;
	bool bHasSeenTouch = false;
	bool bTapPending = false;
	FVector2D TapPosition = FVector2D::ZeroVector;
};
