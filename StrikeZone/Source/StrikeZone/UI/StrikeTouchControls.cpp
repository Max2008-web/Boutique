// STRIKE ZONE - commandes tactiles (joystick virtuel, caméra tactile, boutons) sans asset UMG
#include "UI/StrikeTouchControls.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UStrikeTouchControls::UStrikeTouchControls()
{
	for (int32 i = 0; i < MaxFingers; ++i)
	{
		bFingerDown[i] = false;
		FingerLast[i] = FVector2D::ZeroVector;
	}

	const auto Add = [this](FName Id, const TCHAR* Label, float X, float Y, float R, bool bLook)
	{
		FStrikeTouchButton Button;
		Button.Id = Id;
		Button.Label = Label;
		Button.Center = FVector2D(X, Y);
		Button.Radius = R;
		Button.bAllowsLook = bLook;
		Buttons.Add(Button);
	};

	// Disposition paysage, pouce droit = tir/visée/saut, pouce gauche = déplacement + tir secondaire.
	Add(TEXT("Fire"), TEXT("FEU"), 0.875f, 0.60f, 0.085f, true);
	Add(TEXT("FireLeft"), TEXT("FEU"), 0.075f, 0.38f, 0.055f, false);
	Add(TEXT("Aim"), TEXT("VISER"), 0.745f, 0.74f, 0.058f, true);
	Add(TEXT("Jump"), TEXT("SAUT"), 0.94f, 0.86f, 0.055f, false);
	Add(TEXT("Crouch"), TEXT("ACCR."), 0.84f, 0.88f, 0.052f, false);
	Add(TEXT("Reload"), TEXT("RECH."), 0.75f, 0.52f, 0.048f, false);
	Add(TEXT("Interact"), TEXT("UTIL."), 0.63f, 0.60f, 0.048f, false);
	Add(TEXT("Swap"), TEXT("ARME"), 0.64f, 0.86f, 0.048f, false);
	Add(TEXT("Score"), TEXT("SCORE"), 0.56f, 0.06f, 0.035f, false);
	Add(TEXT("Menu"), TEXT("II"), 0.035f, 0.07f, 0.035f, false);
}

void UStrikeTouchControls::ResetAll()
{
	for (int32 i = 0; i < MaxFingers; ++i)
	{
		bFingerDown[i] = false;
	}
	for (FStrikeTouchButton& Button : Buttons)
	{
		Button.bPressed = false;
		Button.Finger = INDEX_NONE;
	}
	JoystickFinger = INDEX_NONE;
	LookFinger = INDEX_NONE;
	MoveAxis = FVector2D::ZeroVector;
	LookDelta = FVector2D::ZeroVector;
}

FVector2D UStrikeTouchControls::ConsumeLookDelta()
{
	const FVector2D Result = LookDelta;
	LookDelta = FVector2D::ZeroVector;
	return Result;
}

bool UStrikeTouchControls::ConsumeTap(FVector2D& OutPosition)
{
	if (!bTapPending)
	{
		return false;
	}
	bTapPending = false;
	OutPosition = TapPosition;
	return true;
}

int32 UStrikeTouchControls::HitButton(const FVector2D& Position, const FVector2D& ViewportSize) const
{
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		const FStrikeTouchButton& Button = Buttons[i];
		const FVector2D Center(Button.Center.X * ViewportSize.X, Button.Center.Y * ViewportSize.Y);
		const float Radius = Button.Radius * ViewportSize.Y * 1.15f; // tolérance au doigt
		if (FVector2D::DistSquared(Center, Position) <= Radius * Radius)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UStrikeTouchControls::Update(APlayerController* PC, const FVector2D& ViewportSize, TArray<FStrikeTouchEvent>& OutEvents)
{
	if (!PC || ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f)
	{
		return;
	}

	for (int32 Finger = 0; Finger < MaxFingers; ++Finger)
	{
		float X = 0.f;
		float Y = 0.f;
		bool bDown = false;
		PC->GetInputTouchState(static_cast<ETouchIndex::Type>(Finger), X, Y, bDown);
		const FVector2D Position(X, Y);

		if (bDown && !bFingerDown[Finger])
		{
			// --- Nouveau doigt ---
			bHasSeenTouch = true;
			bTapPending = true;
			TapPosition = Position;

			const int32 ButtonIndex = HitButton(Position, ViewportSize);
			if (ButtonIndex != INDEX_NONE && Buttons[ButtonIndex].Finger == INDEX_NONE)
			{
				Buttons[ButtonIndex].Finger = Finger;
				Buttons[ButtonIndex].bPressed = true;
				OutEvents.Add({ Buttons[ButtonIndex].Id, true });
			}
			else if (Position.X < ViewportSize.X * 0.42f && JoystickFinger == INDEX_NONE)
			{
				JoystickFinger = Finger;
				JoystickOrigin = Position;
				JoystickThumb = Position;
			}
			else if (LookFinger == INDEX_NONE)
			{
				LookFinger = Finger;
			}
		}
		else if (bDown && bFingerDown[Finger])
		{
			// --- Doigt qui bouge ---
			const FVector2D Delta = Position - FingerLast[Finger];
			if (Finger == JoystickFinger)
			{
				const float RadiusPx = JoystickRadius * ViewportSize.Y;
				FVector2D Offset = Position - JoystickOrigin;
				if (Offset.Size() > RadiusPx)
				{
					Offset = Offset.GetSafeNormal() * RadiusPx;
				}
				JoystickThumb = JoystickOrigin + Offset;
				MoveAxis = FVector2D(Offset.X / RadiusPx, -Offset.Y / RadiusPx);
			}
			else if (Finger == LookFinger)
			{
				LookDelta += Delta;
			}
			else
			{
				for (const FStrikeTouchButton& Button : Buttons)
				{
					if (Button.Finger == Finger && Button.bAllowsLook)
					{
						LookDelta += Delta;
					}
				}
			}
		}
		else if (!bDown && bFingerDown[Finger])
		{
			// --- Doigt levé ---
			if (Finger == JoystickFinger)
			{
				JoystickFinger = INDEX_NONE;
				MoveAxis = FVector2D::ZeroVector;
			}
			if (Finger == LookFinger)
			{
				LookFinger = INDEX_NONE;
			}
			for (FStrikeTouchButton& Button : Buttons)
			{
				if (Button.Finger == Finger)
				{
					Button.Finger = INDEX_NONE;
					Button.bPressed = false;
					OutEvents.Add({ Button.Id, false });
				}
			}
		}

		bFingerDown[Finger] = bDown;
		FingerLast[Finger] = Position;
	}
}
