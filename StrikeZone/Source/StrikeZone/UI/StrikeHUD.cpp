// STRIKE ZONE - interface de combat, menu principal et commandes tactiles (Canvas, sans asset)
#include "UI/StrikeHUD.h"
#include "UI/StrikeTouchControls.h"
#include "Game/StrikePlayerController.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerState.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/WeaponComponent.h"
#include "Core/StrikeGameInstance.h"
#include "Weapons/StrikeWeapon.h"
#include "World/StrikeInteractables.h"

#include "CanvasItem.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"

namespace StrikeHUDColors
{
	static const FLinearColor Panel(0.f, 0.f, 0.f, 0.45f);
	static const FLinearColor White(1.f, 1.f, 1.f, 1.f);
	static const FLinearColor Dim(0.75f, 0.75f, 0.75f, 1.f);
	static const FLinearColor Accent(1.f, 0.72f, 0.1f, 1.f);
	static const FLinearColor Danger(1.f, 0.2f, 0.15f, 1.f);
	static const FLinearColor Health(0.2f, 0.9f, 0.3f, 1.f);
	static const FLinearColor Armor(0.25f, 0.6f, 1.f, 1.f);
}

namespace StrikeMenu
{
	enum : int32
	{
		ModeZombies = 0, ModeQuick, ModeBR, ModeTactical, ModeExtraction, ModeTraining,
		ToggleHost = 10, CycleDifficulty, CycleQuality, MouseDown, MouseUp, TouchDown, TouchUp,
		ToggleInvert, ToggleTouch, ToggleFPS, Quit, FovDown, FovUp, ToggleDynRes
	};
}

AStrikePlayerController* AStrikeHUD::GetStrikePC() const
{
	return Cast<AStrikePlayerController>(PlayerOwner);
}

FString AStrikeHUD::FormatTime(int32 Seconds)
{
	Seconds = FMath::Max(0, Seconds);
	return FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60);
}

// ---------------------------------------------------------------------------
// Utilitaires de dessin
// ---------------------------------------------------------------------------

void AStrikeHUD::DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentered)
{
	const float FinalScale = Scale * UIScale;
	if (bCentered)
	{
		float W = 0.f;
		float H = 0.f;
		GetTextSize(Text, W, H, Font, FinalScale);
		X -= W * 0.5f;
	}
	// Ombre portée pour la lisibilité sur tous les fonds.
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + 2.f, Y + 2.f, Font, FinalScale);
	DrawText(Text, Color, X, Y, Font, FinalScale);
}

void AStrikeHUD::DrawCircleOutline(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = (static_cast<float>(i) / Segments) * 2.f * PI;
		const float A1 = (static_cast<float>(i + 1) / Segments) * 2.f * PI;
		DrawLine(Center.X + FMath::Cos(A0) * Radius, Center.Y + FMath::Sin(A0) * Radius,
			Center.X + FMath::Cos(A1) * Radius, Center.Y + FMath::Sin(A1) * Radius, Color, Thickness);
	}
}

void AStrikeHUD::DrawFilledCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Segments)
{
	FCanvasNGonItem NGon(Center, FVector2D(Radius, Radius), Segments, Color);
	NGon.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(NGon);
}

void AStrikeHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FLinearColor& Back)
{
	DrawRect(Back, X, Y, W, H);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
}

bool AStrikeHUD::AddMenuButton(const FString& Label, float X, float Y, float W, float H, int32 Action, bool bHighlighted)
{
	FMenuButton Button;
	Button.Position = FVector2D(X, Y);
	Button.Size = FVector2D(W, H);
	Button.Action = Action;
	MenuButtons.Add(Button);

	float MouseX = 0.f;
	float MouseY = 0.f;
	const bool bHover = PlayerOwner && PlayerOwner->GetMousePosition(MouseX, MouseY)
		&& MouseX >= X && MouseX <= X + W && MouseY >= Y && MouseY <= Y + H;

	const FLinearColor Back = bHighlighted ? FLinearColor(0.9f, 0.6f, 0.05f, 0.85f) : (bHover ? FLinearColor(0.25f, 0.25f, 0.3f, 0.9f) : FLinearColor(0.08f, 0.08f, 0.1f, 0.8f));
	DrawRect(Back, X, Y, W, H);
	DrawRect(StrikeHUDColors::Accent, X, Y + H - 3.f * UIScale, W, 3.f * UIScale);

	UFont* Font = GEngine->GetMediumFont();
	float TW = 0.f;
	float TH = 0.f;
	GetTextSize(Label, TW, TH, Font, UIScale);
	DrawTextAt(Label, X + W * 0.5f, Y + (H - TH) * 0.5f, StrikeHUDColors::White, Font, 1.f, true);
	return bHover;
}

// ---------------------------------------------------------------------------
// Point d'entrée
// ---------------------------------------------------------------------------

void AStrikeHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}
	UIScale = FMath::Max(0.5f, Canvas->ClipY / 1080.f);

	const AStrikePlayerController* PC = GetStrikePC();
	if (PC && PC->IsMenuMode())
	{
		DrawMainMenu();
		return;
	}
	MenuButtons.Reset();
	DrawCombatHUD();

	if (PC && PC->GetSettings().bShowFPS)
	{
		const float Delta = GetWorld()->GetDeltaSeconds();
		if (Delta > 0.f)
		{
			SmoothedFPS = FMath::Lerp(SmoothedFPS, 1.f / Delta, 0.05f);
		}
		DrawTextAt(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(SmoothedFPS)), 10.f * UIScale, Canvas->ClipY - 30.f * UIScale, StrikeHUDColors::Dim, GEngine->GetSmallFont());
	}
}

// ---------------------------------------------------------------------------
// Menu principal
// ---------------------------------------------------------------------------

void AStrikeHUD::DrawMainMenu()
{
	MenuButtons.Reset();
	AStrikePlayerController* PC = GetStrikePC();
	if (!PC)
	{
		return;
	}
	const FStrikeSettings& S = PC->GetSettings();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 0.f, W, H);
	DrawTextAt(TEXT("STRIKE ZONE"), W * 0.5f, 40.f * UIScale, StrikeHUDColors::Accent, Large, 2.2f, true);
	DrawTextAt(TEXT("FPS tactique multiplateforme"), W * 0.5f, 110.f * UIScale, StrikeHUDColors::Dim, Medium, 1.f, true);

	// --- Colonne des modes ---
	struct FModeEntry { const TCHAR* Label; const TCHAR* Desc; int32 Action; };
	const FModeEntry Modes[] = {
		{ TEXT("ZOMBIES"), TEXT("Survie coopérative en vagues, achats d'armes, boss"), StrikeMenu::ModeZombies },
		{ TEXT("PARTIE RAPIDE"), TEXT("Match à mort par équipes 6v6 avec bots"), StrikeMenu::ModeQuick },
		{ TEXT("BATTLE ROYALE"), TEXT("Grande carte, butin, zone dynamique, 30 max"), StrikeMenu::ModeBR },
		{ TEXT("TACTIQUE"), TEXT("5v5 en manches, sans réapparition, objectifs"), StrikeMenu::ModeTactical },
		{ TEXT("EXTRACTION"), TEXT("Explorer, piller, s'extraire vivant"), StrikeMenu::ModeExtraction },
		{ TEXT("ENTRAÎNEMENT"), TEXT("Stand de tir, cibles mobiles, statistiques"), StrikeMenu::ModeTraining },
	};

	const float ColX = W * 0.08f;
	const float BtnW = FMath::Min(W * 0.36f, 520.f * UIScale);
	const float BtnH = 62.f * UIScale;
	float Y = 170.f * UIScale;
	for (const FModeEntry& Mode : Modes)
	{
		AddMenuButton(Mode.Label, ColX, Y, BtnW, BtnH, Mode.Action);
		DrawTextAt(Mode.Desc, ColX + BtnW + 16.f * UIScale, Y + BtnH * 0.3f, StrikeHUDColors::Dim, GEngine->GetSmallFont(), 1.1f);
		Y += BtnH + 14.f * UIScale;
	}
	AddMenuButton(bMenuHost ? TEXT("Héberger en ligne : OUI") : TEXT("Héberger en ligne : NON"), ColX, Y + 10.f * UIScale, BtnW, BtnH * 0.8f, StrikeMenu::ToggleHost, bMenuHost);

	// --- Colonne des réglages ---
	const float SX = W * 0.62f;
	const float SW = FMath::Min(W * 0.32f, 460.f * UIScale);
	const float SH = 48.f * UIScale;
	const float Half = (SW - 10.f * UIScale) * 0.5f;
	float SY = 170.f * UIScale;
	const float Gap = 10.f * UIScale;

	static const TCHAR* Difficulties[] = { TEXT("Facile"), TEXT("Normal"), TEXT("Difficile"), TEXT("Vétéran") };
	static const TCHAR* Qualities[] = { TEXT("Bas"), TEXT("Moyen"), TEXT("Élevé"), TEXT("Épique") };

	DrawTextAt(TEXT("RÉGLAGES"), SX, SY - 40.f * UIScale, StrikeHUDColors::Accent, Medium, 1.2f);
	AddMenuButton(FString::Printf(TEXT("Bots : %s"), Difficulties[FMath::Clamp(S.BotDifficulty, 0, 3)]), SX, SY, SW, SH, StrikeMenu::CycleDifficulty); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("Graphismes : %s"), S.GraphicsQuality < 0 ? TEXT("Auto") : Qualities[FMath::Clamp(S.GraphicsQuality, 0, 3)]), SX, SY, SW, SH, StrikeMenu::CycleQuality); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("Résolution dynamique : %s"), S.bDynamicResolution ? TEXT("OUI") : TEXT("NON")), SX, SY, SW, SH, StrikeMenu::ToggleDynRes); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("Souris - (%.2f)"), S.MouseSensitivity), SX, SY, Half, SH, StrikeMenu::MouseDown);
	AddMenuButton(TEXT("Souris +"), SX + Half + 10.f * UIScale, SY, Half, SH, StrikeMenu::MouseUp); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("Tactile - (%.2f)"), S.TouchSensitivity), SX, SY, Half, SH, StrikeMenu::TouchDown);
	AddMenuButton(TEXT("Tactile +"), SX + Half + 10.f * UIScale, SY, Half, SH, StrikeMenu::TouchUp); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("FOV - (%d)"), FMath::RoundToInt(S.FieldOfView)), SX, SY, Half, SH, StrikeMenu::FovDown);
	AddMenuButton(TEXT("FOV +"), SX + Half + 10.f * UIScale, SY, Half, SH, StrikeMenu::FovUp); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("Inverser Y : %s"), S.bInvertY ? TEXT("OUI") : TEXT("NON")), SX, SY, SW, SH, StrikeMenu::ToggleInvert); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("Commandes tactiles forcées : %s"), S.bForceTouchControls ? TEXT("OUI") : TEXT("NON")), SX, SY, SW, SH, StrikeMenu::ToggleTouch); SY += SH + Gap;
	AddMenuButton(FString::Printf(TEXT("Afficher FPS : %s"), S.bShowFPS ? TEXT("OUI") : TEXT("NON")), SX, SY, SW, SH, StrikeMenu::ToggleFPS); SY += SH + Gap;
	AddMenuButton(TEXT("Quitter"), SX, SY, SW, SH, StrikeMenu::Quit); SY += SH + Gap * 2.f;

	// --- Profil persistant ---
	if (const UStrikeGameInstance* GI = GetGameInstance() ? Cast<UStrikeGameInstance>(GetGameInstance()) : nullptr)
	{
		if (const UStrikeSaveGame* Profile = GI->GetProfile())
		{
			DrawTextAt(FString::Printf(TEXT("Coffre : %d objets  |  Crédits : %d  |  Extractions : %d  |  Record zombies : vague %d"),
				Profile->Stash.Num(), Profile->Credits, Profile->SuccessfulExtractions, Profile->BestZombieWave),
				W * 0.5f, H - 50.f * UIScale, StrikeHUDColors::Dim, GEngine->GetSmallFont(), 1.2f, true);
		}
	}
}

void AStrikeHUD::HandleMenuClick(const FVector2D& ScreenPosition)
{
	for (const FMenuButton& Button : MenuButtons)
	{
		if (ScreenPosition.X >= Button.Position.X && ScreenPosition.X <= Button.Position.X + Button.Size.X
			&& ScreenPosition.Y >= Button.Position.Y && ScreenPosition.Y <= Button.Position.Y + Button.Size.Y)
		{
			ExecuteMenuAction(Button.Action);
			return;
		}
	}
}

void AStrikeHUD::ExecuteMenuAction(int32 Action)
{
	AStrikePlayerController* PC = GetStrikePC();
	if (!PC)
	{
		return;
	}
	FStrikeSettings S = PC->GetSettings();

	switch (Action)
	{
	case StrikeMenu::ModeZombies: PC->StartMode(TEXT("Zombies"), bMenuHost); return;
	case StrikeMenu::ModeQuick: PC->StartMode(TEXT("QuickMatch"), bMenuHost); return;
	case StrikeMenu::ModeBR: PC->StartMode(TEXT("BattleRoyale"), bMenuHost); return;
	case StrikeMenu::ModeTactical: PC->StartMode(TEXT("Tactical"), bMenuHost); return;
	case StrikeMenu::ModeExtraction: PC->StartMode(TEXT("Extraction"), bMenuHost); return;
	case StrikeMenu::ModeTraining: PC->StartMode(TEXT("Training"), false); return;
	case StrikeMenu::ToggleHost: bMenuHost = !bMenuHost; return;
	case StrikeMenu::CycleDifficulty: S.BotDifficulty = (S.BotDifficulty + 1) % 4; break;
	case StrikeMenu::CycleQuality: S.GraphicsQuality = S.GraphicsQuality >= 3 ? -1 : S.GraphicsQuality + 1; break;
	case StrikeMenu::ToggleDynRes: S.bDynamicResolution = !S.bDynamicResolution; break;
	case StrikeMenu::MouseDown: S.MouseSensitivity = FMath::Max(0.1f, S.MouseSensitivity - 0.1f); break;
	case StrikeMenu::MouseUp: S.MouseSensitivity = FMath::Min(5.f, S.MouseSensitivity + 0.1f); break;
	case StrikeMenu::TouchDown: S.TouchSensitivity = FMath::Max(0.1f, S.TouchSensitivity - 0.1f); break;
	case StrikeMenu::TouchUp: S.TouchSensitivity = FMath::Min(5.f, S.TouchSensitivity + 0.1f); break;
	case StrikeMenu::FovDown: S.FieldOfView = FMath::Max(70.f, S.FieldOfView - 5.f); break;
	case StrikeMenu::FovUp: S.FieldOfView = FMath::Min(110.f, S.FieldOfView + 5.f); break;
	case StrikeMenu::ToggleInvert: S.bInvertY = !S.bInvertY; break;
	case StrikeMenu::ToggleTouch: S.bForceTouchControls = !S.bForceTouchControls; break;
	case StrikeMenu::ToggleFPS: S.bShowFPS = !S.bShowFPS; break;
	case StrikeMenu::Quit: UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false); return;
	default: return;
	}
	PC->UpdateSettings(S);
}

// ---------------------------------------------------------------------------
// HUD de combat
// ---------------------------------------------------------------------------

void AStrikeHUD::DrawCombatHUD()
{
	AStrikePlayerController* PC = GetStrikePC();
	const AStrikeGameState* GS = GetWorld()->GetGameState<AStrikeGameState>();
	AStrikeCharacter* Character = PC ? PC->GetStrikeCharacter() : nullptr;

	if (Character && !Character->IsDead())
	{
		DrawCrosshair(Character);
		DrawDamageIndicator(Character);
		DrawVitals(Character);
		DrawWeaponInfo(Character);
		DrawInteractPrompt(Character);
	}
	else
	{
		DrawDeathScreen(GS);
	}

	if (GS)
	{
		DrawTopBar(GS);
		DrawModePanel(GS, Character);
		DrawKillFeed(GS);

		const float Now = GetWorld()->GetTimeSeconds();
		if (GS->AnnouncementEndTime > Now && !GS->AnnouncementText.IsEmpty())
		{
			DrawTextAt(GS->AnnouncementText, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.28f, StrikeHUDColors::Accent, GEngine->GetLargeFont(), 1.4f, true);
		}
		if (PC && PC->IsScoreboardVisible())
		{
			DrawScoreboard(GS);
		}
		if (GS->bMatchFinished)
		{
			DrawMatchEnd(GS);
		}
	}

	if (PC && PC->IsUsingTouch())
	{
		DrawTouchControls();
	}
}

void AStrikeHUD::DrawCrosshair(const AStrikeCharacter* Character)
{
	const AStrikeWeapon* Weapon = Character->GetCurrentWeapon();
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Aim = Character->GetAimAlpha();
	const float Now = GetWorld()->GetTimeSeconds();

	if (Weapon)
	{
		// Écart du réticule proportionnel à la dispersion réelle de l'arme.
		const float FOV = Character->GetFirstPersonCamera()->FieldOfView;
		const float SpreadRad = FMath::DegreesToRadians(Weapon->GetCurrentSpread());
		const float HalfFovRad = FMath::DegreesToRadians(FOV * 0.5f);
		const float Gap = FMath::Max(3.f, FMath::Tan(SpreadRad) / FMath::Tan(HalfFovRad) * Canvas->ClipX * 0.5f);
		const float Len = 10.f * UIScale;
		const FLinearColor Color(1.f, 1.f, 1.f, FMath::Clamp(1.f - Aim * 1.4f, 0.f, 1.f));

		if (Color.A > 0.01f)
		{
			DrawLine(Center.X - Gap - Len, Center.Y, Center.X - Gap, Center.Y, Color, 2.f);
			DrawLine(Center.X + Gap, Center.Y, Center.X + Gap + Len, Center.Y, Color, 2.f);
			DrawLine(Center.X, Center.Y - Gap - Len, Center.X, Center.Y - Gap, Color, 2.f);
			DrawLine(Center.X, Center.Y + Gap, Center.X, Center.Y + Gap + Len, Color, 2.f);
		}

		// Visée ADS : point central, et lunette simplifiée pour le fusil de précision.
		if (Aim > 0.6f)
		{
			DrawRect(FLinearColor(1.f, 0.2f, 0.2f, Aim), Center.X - 2.f, Center.Y - 2.f, 4.f, 4.f);
			if (Weapon->AimFOV < 35.f && Aim > 0.95f)
			{
				const float R = Canvas->ClipY * 0.45f;
				DrawCircleOutline(Center, R, FLinearColor(0.f, 0.f, 0.f, 0.9f), 6.f, 64);
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f), 0.f, 0.f, Center.X - R, Canvas->ClipY);
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f), Center.X + R, 0.f, Canvas->ClipX - Center.X - R, Canvas->ClipY);
				DrawLine(Center.X - R, Center.Y, Center.X + R, Center.Y, FLinearColor::Black, 1.5f);
				DrawLine(Center.X, Center.Y - R, Center.X, Center.Y + R, FLinearColor::Black, 1.5f);
			}
		}
	}

	// Hit marker confirmé par le serveur.
	if (const AStrikePlayerController* PC = GetStrikePC())
	{
		const float Age = Now - PC->LastHitMarkerTime;
		if (Age < 0.25f)
		{
			const FLinearColor Color = PC->bLastHitKill ? StrikeHUDColors::Danger : (PC->bLastHitHeadshot ? StrikeHUDColors::Accent : StrikeHUDColors::White);
			const float S = (PC->bLastHitKill ? 14.f : 10.f) * UIScale;
			const float I = 5.f * UIScale;
			DrawLine(Center.X - S, Center.Y - S, Center.X - I, Center.Y - I, Color, 2.5f);
			DrawLine(Center.X + S, Center.Y - S, Center.X + I, Center.Y - I, Color, 2.5f);
			DrawLine(Center.X - S, Center.Y + S, Center.X - I, Center.Y + I, Color, 2.5f);
			DrawLine(Center.X + S, Center.Y + S, Center.X + I, Center.Y + I, Color, 2.5f);
		}
	}
}

void AStrikeHUD::DrawDamageIndicator(const AStrikeCharacter* Character)
{
	const UHealthComponent* Health = Character->GetHealthComponent();
	const float Age = GetWorld()->GetTimeSeconds() - Health->LastDamageClientTime;
	if (Age > 1.2f)
	{
		return;
	}
	const FVector ToSource = Health->LastDamageSource - Character->GetActorLocation();
	const float ViewYaw = Character->GetControlRotation().Yaw;
	const float Angle = FMath::DegreesToRadians(ToSource.Rotation().Yaw - ViewYaw - 90.f);
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float R = 140.f * UIScale;
	const FLinearColor Color(1.f, 0.1f, 0.05f, 1.f - Age / 1.2f);

	for (int32 i = -3; i <= 3; ++i)
	{
		const float A0 = Angle + i * 0.06f;
		const float A1 = Angle + (i + 1) * 0.06f;
		DrawLine(Center.X + FMath::Cos(A0) * R, Center.Y + FMath::Sin(A0) * R, Center.X + FMath::Cos(A1) * R, Center.Y + FMath::Sin(A1) * R, Color, 6.f);
	}
	// Vignette rouge lorsque la santé est basse.
	if (Health->GetHealthFraction() < 0.35f)
	{
		DrawRect(FLinearColor(0.6f, 0.f, 0.f, 0.18f * (1.f - Age / 1.2f)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
}

void AStrikeHUD::DrawVitals(const AStrikeCharacter* Character)
{
	const UHealthComponent* Health = Character->GetHealthComponent();
	const float X = 30.f * UIScale;
	const float Y = Canvas->ClipY - 110.f * UIScale;
	const float W = 320.f * UIScale;

	DrawRect(StrikeHUDColors::Panel, X - 10.f * UIScale, Y - 10.f * UIScale, W + 20.f * UIScale, 90.f * UIScale);
	DrawBar(X, Y, W, 22.f * UIScale, Health->GetHealthFraction(), StrikeHUDColors::Health, FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
	DrawTextAt(FString::Printf(TEXT("%d"), FMath::CeilToInt(Health->GetHealth())), X + 8.f * UIScale, Y, StrikeHUDColors::White, GEngine->GetSmallFont(), 1.2f);
	DrawBar(X, Y + 30.f * UIScale, W, 14.f * UIScale, Health->MaxArmor > 0.f ? Health->GetArmor() / Health->MaxArmor : 0.f, StrikeHUDColors::Armor, FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));

	FString State;
	if (Character->IsSliding())
	{
		State = TEXT("GLISSADE");
	}
	else if (Character->IsSprinting())
	{
		State = TEXT("SPRINT");
	}
	else if (Character->bIsCrouched)
	{
		State = TEXT("ACCROUPI");
	}
	DrawTextAt(State, X, Y + 50.f * UIScale, StrikeHUDColors::Dim, GEngine->GetSmallFont(), 1.1f);
}

void AStrikeHUD::DrawWeaponInfo(const AStrikeCharacter* Character)
{
	const AStrikeWeapon* Weapon = Character->GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}
	const float X = Canvas->ClipX - 360.f * UIScale;
	const float Y = Canvas->ClipY - 120.f * UIScale;

	DrawRect(StrikeHUDColors::Panel, X - 10.f * UIScale, Y - 10.f * UIScale, 350.f * UIScale, 100.f * UIScale);
	const int32 Ammo = Weapon->GetCurrentAmmo();
	const FLinearColor AmmoColor = Ammo <= Weapon->MagazineSize / 4 ? StrikeHUDColors::Danger : StrikeHUDColors::White;
	DrawTextAt(FString::Printf(TEXT("%d"), Ammo), X, Y, AmmoColor, GEngine->GetLargeFont(), 1.4f);
	DrawTextAt(FString::Printf(TEXT("/ %d  %s"), Weapon->GetReserveAmmo(), *StrikeUtils::AmmoTypeToString(Weapon->AmmoType)), X + 90.f * UIScale, Y + 14.f * UIScale, StrikeHUDColors::Dim, GEngine->GetMediumFont());
	DrawTextAt(Weapon->WeaponName, X, Y + 50.f * UIScale, StrikeHUDColors::Accent, GEngine->GetSmallFont(), 1.2f);

	if (Weapon->IsReloading())
	{
		DrawTextAt(TEXT("RECHARGEMENT..."), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.58f, StrikeHUDColors::Accent, GEngine->GetMediumFont(), 1.f, true);
	}
	else if (Ammo == 0 && Weapon->GetReserveAmmo() == 0)
	{
		DrawTextAt(TEXT("PLUS DE MUNITIONS"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.58f, StrikeHUDColors::Danger, GEngine->GetMediumFont(), 1.f, true);
	}
	else if (Ammo <= Weapon->MagazineSize / 4)
	{
		DrawTextAt(TEXT("RECHARGER [R]"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.58f, StrikeHUDColors::White, GEngine->GetSmallFont(), 1.2f, true);
	}

	// Emplacements d'armes.
	const TArray<TObjectPtr<AStrikeWeapon>>& Weapons = Character->GetWeaponComponent()->GetWeapons();
	for (int32 i = 0; i < Weapons.Num(); ++i)
	{
		if (Weapons[i])
		{
			const bool bCurrent = Weapons[i].Get() == Weapon;
			DrawTextAt(FString::Printf(TEXT("%d  %s"), i + 1, *Weapons[i]->WeaponName), X, Y - (30.f + 24.f * (Weapons.Num() - i)) * UIScale,
				bCurrent ? StrikeHUDColors::Accent : StrikeHUDColors::Dim, GEngine->GetSmallFont(), 1.05f);
		}
	}
}

void AStrikeHUD::DrawInteractPrompt(AStrikeCharacter* Character)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextPromptUpdate)
	{
		NextPromptUpdate = Now + 0.15f;
		CachedPrompt.Reset();
		if (const IStrikeInteractable* Interactable = Cast<IStrikeInteractable>(Character->FindInteractable()))
		{
			CachedPrompt = Interactable->GetInteractPrompt(Character);
		}
	}
	if (!CachedPrompt.IsEmpty())
	{
		const AStrikePlayerController* PC = GetStrikePC();
		const FString Key = (PC && PC->IsUsingTouch()) ? TEXT("[UTIL.]") : TEXT("[E]");
		DrawTextAt(FString::Printf(TEXT("%s %s"), *Key, *CachedPrompt), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.64f, StrikeHUDColors::White, GEngine->GetMediumFont(), 1.f, true);
	}
}

void AStrikeHUD::DrawTopBar(const AStrikeGameState* GS)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = 16.f * UIScale;
	UFont* Medium = GEngine->GetMediumFont();

	DrawRect(StrikeHUDColors::Panel, CX - 260.f * UIScale, Y - 6.f * UIScale, 520.f * UIScale, 70.f * UIScale);
	DrawTextAt(GS->ModeName, CX, Y, StrikeHUDColors::Dim, GEngine->GetSmallFont(), 1.1f, true);

	FString Main;
	switch (GS->ModeType)
	{
	case EStrikeModeType::Zombies:
		Main = FString::Printf(TEXT("VAGUE %d   |   Zombies : %d"), GS->CurrentWave, GS->EnemiesRemaining);
		break;
	case EStrikeModeType::QuickMatch:
		Main = FString::Printf(TEXT("BLEUS %d  -  %d ROUGES   |   %s"), GS->GetTeamScore(0), GS->GetTeamScore(1), *FormatTime(GS->RemainingTime));
		break;
	case EStrikeModeType::BattleRoyale:
		Main = FString::Printf(TEXT("Survivants : %d   |   Zone : %s%s"), GS->AliveCount, *FormatTime(GS->ZonePhaseTimer), GS->bZoneShrinking ? TEXT(" (réduction)") : TEXT(""));
		break;
	case EStrikeModeType::Tactical:
		Main = FString::Printf(TEXT("Manche %d   |   BLEUS %d - %d ROUGES   |   %s"), GS->RoundNumber, GS->GetTeamScore(0), GS->GetTeamScore(1), *FormatTime(GS->RemainingTime));
		break;
	case EStrikeModeType::Extraction:
		Main = FString::Printf(TEXT("%s   |   Extraction %s"), *FormatTime(GS->RemainingTime), GS->bExtractionOpen ? TEXT("OUVERTE") : TEXT("fermée"));
		break;
	case EStrikeModeType::Training:
		Main = TEXT("Stand de tir   |   strikeresetstats : remise à zéro");
		break;
	default:
		break;
	}
	DrawTextAt(Main, CX, Y + 26.f * UIScale, StrikeHUDColors::White, Medium, 1.f, true);

	if (!GS->StatusText.IsEmpty())
	{
		DrawTextAt(GS->StatusText, CX, Y + 72.f * UIScale, StrikeHUDColors::Accent, GEngine->GetSmallFont(), 1.2f, true);
	}
}

void AStrikeHUD::DrawModePanel(const AStrikeGameState* GS, const AStrikeCharacter* Character)
{
	const AStrikePlayerController* PC = GetStrikePC();
	const AStrikePlayerState* PS = PC ? PC->GetPlayerState<AStrikePlayerState>() : nullptr;
	const float X = 30.f * UIScale;
	float Y = Canvas->ClipY - 160.f * UIScale;
	UFont* Small = GEngine->GetSmallFont();

	switch (GS->ModeType)
	{
	case EStrikeModeType::Zombies:
		if (PS)
		{
			DrawTextAt(FString::Printf(TEXT("Points : %d"), PS->GetPoints()), X, Y, StrikeHUDColors::Accent, GEngine->GetMediumFont());
		}
		break;

	case EStrikeModeType::Training:
		if (PS)
		{
			const float HeadPct = PS->GetShotsHit() > 0 ? 100.f * PS->GetHeadshotHits() / PS->GetShotsHit() : 0.f;
			const float PanelY = Canvas->ClipY * 0.3f;
			DrawRect(StrikeHUDColors::Panel, X - 10.f * UIScale, PanelY - 10.f * UIScale, 300.f * UIScale, 150.f * UIScale);
			DrawTextAt(TEXT("STATISTIQUES"), X, PanelY, StrikeHUDColors::Accent, Small, 1.2f);
			DrawTextAt(FString::Printf(TEXT("Tirs : %d"), PS->GetShotsFired()), X, PanelY + 28.f * UIScale, StrikeHUDColors::White, Small, 1.1f);
			DrawTextAt(FString::Printf(TEXT("Touches : %d"), PS->GetShotsHit()), X, PanelY + 52.f * UIScale, StrikeHUDColors::White, Small, 1.1f);
			DrawTextAt(FString::Printf(TEXT("Précision : %.1f %%"), PS->GetAccuracy() * 100.f), X, PanelY + 76.f * UIScale, StrikeHUDColors::White, Small, 1.1f);
			DrawTextAt(FString::Printf(TEXT("Tête : %.1f %%   Éliminations : %d"), HeadPct, PS->GetKills()), X, PanelY + 100.f * UIScale, StrikeHUDColors::White, Small, 1.1f);
		}
		break;

	case EStrikeModeType::BattleRoyale:
		if (Character && GS->ZoneRadius > 0.f)
		{
			const float Distance = FVector::Dist2D(Character->GetActorLocation(), GS->ZoneCenter);
			if (Distance > GS->ZoneRadius)
			{
				DrawTextAt(FString::Printf(TEXT("HORS DE LA ZONE - %d m"), FMath::RoundToInt((Distance - GS->ZoneRadius) / 100.f)),
					Canvas->ClipX * 0.5f, Canvas->ClipY * 0.2f, StrikeHUDColors::Danger, GEngine->GetMediumFont(), 1.2f, true);
			}
		}
		break;

	case EStrikeModeType::Tactical:
	{
		const bool bAttacking = PS && PS->GetTeamId() == GS->AttackingTeam;
		DrawTextAt(bAttacking ? TEXT("ATTAQUE : posez la charge sur un site (maintenir [E])") : TEXT("DÉFENSE : protégez les sites, désamorcez (maintenir [E])"),
			X, Y, StrikeHUDColors::Accent, Small, 1.15f);
		if (GS->ObjectiveState == 1 || GS->ObjectiveState == 3)
		{
			const float W = 400.f * UIScale;
			DrawBar(Canvas->ClipX * 0.5f - W * 0.5f, Canvas->ClipY * 0.7f, W, 16.f * UIScale, GS->ObjectiveProgress, StrikeHUDColors::Accent, FLinearColor(0.f, 0.f, 0.f, 0.6f));
			DrawTextAt(GS->ObjectiveState == 1 ? TEXT("POSE DE LA CHARGE") : TEXT("DÉSAMORÇAGE"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.7f - 30.f * UIScale, StrikeHUDColors::White, Small, 1.2f, true);
		}
		else if (GS->ObjectiveState == 2)
		{
			DrawTextAt(TEXT("CHARGE POSÉE !"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.16f, StrikeHUDColors::Danger, GEngine->GetMediumFont(), 1.2f, true);
		}
		break;
	}

	case EStrikeModeType::Extraction:
		if (Character)
		{
			DrawTextAt(FString::Printf(TEXT("Butin transporté : %d objets (valeur %d)"),
				Character->GetInventoryComponent()->GetLoot().Num(), Character->GetInventoryComponent()->GetLootValue()), X, Y, StrikeHUDColors::Accent, Small, 1.2f);
			for (TActorIterator<AStrikeObjectiveZone> It(GetWorld()); It; ++It)
			{
				if (It->ObjectiveType == EStrikeObjectiveType::Extraction && It->IsZoneActive() && It->IsInside(Character))
				{
					DrawTextAt(TEXT("EXTRACTION EN COURS - restez dans la zone"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.7f, StrikeHUDColors::Health, GEngine->GetMediumFont(), 1.f, true);
				}
			}
		}
		break;

	default:
		break;
	}
}

void AStrikeHUD::DrawKillFeed(const AStrikeGameState* GS)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float X = Canvas->ClipX - 30.f * UIScale;
	float Y = 20.f * UIScale;
	UFont* Font = GEngine->GetSmallFont();

	for (const FStrikeKillFeedEntry& Entry : GS->KillFeed)
	{
		if (Now - Entry.Time > 6.f)
		{
			continue;
		}
		const FString Weapon = Entry.Weapon.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" [%s]"), *Entry.Weapon);
		const FString Line = Entry.Killer.IsEmpty()
			? FString::Printf(TEXT("%s est mort"), *Entry.Victim)
			: FString::Printf(TEXT("%s%s %s %s"), *Entry.Killer, *Weapon, Entry.bHeadshot ? TEXT("(tête) >") : TEXT(">"), *Entry.Victim);

		float W = 0.f;
		float H = 0.f;
		GetTextSize(Line, W, H, Font, UIScale * 1.1f);
		DrawRect(StrikeHUDColors::Panel, X - W - 10.f * UIScale, Y - 2.f * UIScale, W + 14.f * UIScale, H + 4.f * UIScale);
		DrawTextAt(Line, X - W - 4.f * UIScale, Y, StrikeUtils::TeamColor(Entry.KillerTeam), Font, 1.1f);
		Y += H + 8.f * UIScale;
	}
}

void AStrikeHUD::DrawScoreboard(const AStrikeGameState* GS)
{
	TArray<AStrikePlayerState*> Players = GS->GetStrikePlayers();
	Players.Sort([](const AStrikePlayerState& A, const AStrikePlayerState& B)
	{
		if (A.GetTeamId() != B.GetTeamId())
		{
			return A.GetTeamId() < B.GetTeamId();
		}
		return A.GetScore() > B.GetScore();
	});

	const float W = FMath::Min(Canvas->ClipX * 0.8f, 1100.f * UIScale);
	const float X = (Canvas->ClipX - W) * 0.5f;
	float Y = Canvas->ClipY * 0.15f;
	const float RowH = 30.f * UIScale;
	UFont* Font = GEngine->GetSmallFont();

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), X, Y, W, RowH * (Players.Num() + 2) + 20.f * UIScale);
	Y += 10.f * UIScale;
	const bool bZombies = GS->ModeType == EStrikeModeType::Zombies;
	DrawTextAt(TEXT("JOUEUR"), X + 20.f * UIScale, Y, StrikeHUDColors::Accent, Font, 1.1f);
	DrawTextAt(TEXT("ÉLIM."), X + W * 0.55f, Y, StrikeHUDColors::Accent, Font, 1.1f);
	DrawTextAt(TEXT("MORTS"), X + W * 0.65f, Y, StrikeHUDColors::Accent, Font, 1.1f);
	DrawTextAt(TEXT("PRÉC."), X + W * 0.75f, Y, StrikeHUDColors::Accent, Font, 1.1f);
	DrawTextAt(bZombies ? TEXT("POINTS") : TEXT("SCORE"), X + W * 0.86f, Y, StrikeHUDColors::Accent, Font, 1.1f);
	Y += RowH;

	const APlayerState* LocalPS = PlayerOwner ? PlayerOwner->PlayerState.Get() : nullptr;
	for (const AStrikePlayerState* PS : Players)
	{
		if (PS == LocalPS)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.1f), X, Y - 2.f * UIScale, W, RowH);
		}
		const FLinearColor Color = GS->bUseTeams ? StrikeUtils::TeamColor(PS->GetTeamId()) : StrikeHUDColors::White;
		FString Name = PS->GetPlayerName();
		if (PS->IsEliminated())
		{
			Name += TEXT("  (éliminé)");
		}
		DrawTextAt(Name, X + 20.f * UIScale, Y, Color, Font, 1.1f);
		DrawTextAt(FString::FromInt(PS->GetKills()), X + W * 0.55f, Y, StrikeHUDColors::White, Font, 1.1f);
		DrawTextAt(FString::FromInt(PS->GetDeaths()), X + W * 0.65f, Y, StrikeHUDColors::White, Font, 1.1f);
		DrawTextAt(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(PS->GetAccuracy() * 100.f)), X + W * 0.75f, Y, StrikeHUDColors::White, Font, 1.1f);
		DrawTextAt(FString::FromInt(bZombies ? PS->GetPoints() : FMath::RoundToInt(PS->GetScore())), X + W * 0.86f, Y, StrikeHUDColors::White, Font, 1.1f);
		Y += RowH;
	}
}

void AStrikeHUD::DrawDeathScreen(const AStrikeGameState* GS)
{
	if (GS && GS->bMatchFinished)
	{
		return;
	}
	const AStrikePlayerController* PC = GetStrikePC();
	const AStrikePlayerState* PS = PC ? PC->GetPlayerState<AStrikePlayerState>() : nullptr;

	DrawRect(FLinearColor(0.3f, 0.f, 0.f, 0.25f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	FString Text = TEXT("ÉLIMINÉ - réapparition imminente");
	if (PS && PS->IsExtracted())
	{
		Text = TEXT("EXTRAIT ! Butin transféré au coffre");
	}
	else if (PS && PS->IsEliminated())
	{
		Text = TEXT("ÉLIMINÉ - mode spectateur");
	}
	else if (GS && GS->ModeType == EStrikeModeType::Zombies)
	{
		Text = TEXT("À TERRE - retour à la fin de la vague");
	}
	DrawTextAt(Text, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.42f, StrikeHUDColors::White, GEngine->GetLargeFont(), 1.1f, true);
}

void AStrikeHUD::DrawMatchEnd(const AStrikeGameState* GS)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, Canvas->ClipY * 0.35f, Canvas->ClipX, Canvas->ClipY * 0.22f);
	DrawTextAt(TEXT("FIN DE PARTIE"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.37f, StrikeHUDColors::Dim, GEngine->GetMediumFont(), 1.2f, true);
	DrawTextAt(GS->WinnerText, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.42f, StrikeHUDColors::Accent, GEngine->GetLargeFont(), 1.6f, true);
	DrawTextAt(TEXT("Nouvelle partie dans quelques secondes  -  [Échap] menu"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.51f, StrikeHUDColors::White, GEngine->GetSmallFont(), 1.2f, true);
}

void AStrikeHUD::DrawTouchControls()
{
	const AStrikePlayerController* PC = GetStrikePC();
	const UStrikeTouchControls* Touch = PC ? PC->GetTouchControls() : nullptr;
	if (!Touch)
	{
		return;
	}
	const FVector2D Viewport(Canvas->ClipX, Canvas->ClipY);
	UFont* Font = GEngine->GetSmallFont();

	// Joystick (position par défaut si inactif).
	const float JR = Touch->GetJoystickRadiusPixels(Viewport);
	const FVector2D Base = Touch->IsJoystickActive() ? Touch->GetJoystickOrigin() : Touch->GetDefaultJoystickCenter(Viewport);
	const FVector2D Thumb = Touch->IsJoystickActive() ? Touch->GetJoystickThumb() : Base;
	DrawFilledCircle(Base, JR, FLinearColor(0.f, 0.f, 0.f, 0.25f));
	DrawCircleOutline(Base, JR, FLinearColor(1.f, 1.f, 1.f, 0.4f), 2.f);
	DrawFilledCircle(Thumb, JR * 0.42f, FLinearColor(1.f, 1.f, 1.f, Touch->IsJoystickActive() ? 0.55f : 0.3f));
	if (Touch->WantsSprint())
	{
		DrawTextAt(TEXT("SPRINT"), Base.X, Base.Y - JR - 30.f * UIScale, StrikeHUDColors::Accent, Font, 1.1f, true);
	}

	for (const FStrikeTouchButton& Button : Touch->GetButtons())
	{
		const FVector2D Center(Button.Center.X * Viewport.X, Button.Center.Y * Viewport.Y);
		const float Radius = Button.Radius * Viewport.Y;
		bool bActive = Button.bPressed;
		if (Button.Id == TEXT("Aim") && PC->GetStrikeCharacter())
		{
			bActive = PC->GetStrikeCharacter()->IsAiming();
		}
		else if (Button.Id == TEXT("Crouch") && PC->GetStrikeCharacter())
		{
			bActive = PC->GetStrikeCharacter()->bIsCrouched;
		}
		const bool bFire = Button.Id == TEXT("Fire") || Button.Id == TEXT("FireLeft");
		const FLinearColor Fill = bActive ? FLinearColor(1.f, 0.7f, 0.1f, 0.55f) : (bFire ? FLinearColor(0.8f, 0.1f, 0.1f, 0.35f) : FLinearColor(0.f, 0.f, 0.f, 0.35f));
		DrawFilledCircle(Center, Radius, Fill);
		DrawCircleOutline(Center, Radius, FLinearColor(1.f, 1.f, 1.f, 0.5f), 2.f);

		float TW = 0.f;
		float TH = 0.f;
		GetTextSize(Button.Label, TW, TH, Font, UIScale);
		DrawTextAt(Button.Label, Center.X, Center.Y - TH * 0.5f, StrikeHUDColors::White, Font, 1.f, true);
	}
}
