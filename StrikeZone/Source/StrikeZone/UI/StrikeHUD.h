// STRIKE ZONE - interface de combat, menu principal et commandes tactiles (Canvas, sans asset)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "StrikeHUD.generated.h"

class AStrikePlayerController;
class AStrikeCharacter;
class AStrikeGameState;
class UFont;

/**
 * HUD dessiné au Canvas : fonctionne immédiatement sur PC et mobile, sans widget UMG.
 * L'échelle suit la hauteur d'écran (référence 1080p) pour s'adapter à toutes les résolutions.
 * Une version UMG / Common UI peut le remplacer progressivement (voir README).
 */
UCLASS()
class STRIKEZONE_API AStrikeHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Clic souris ou toucher dans le menu principal. */
	void HandleMenuClick(const FVector2D& ScreenPosition);

protected:
	struct FMenuButton
	{
		FVector2D Position;
		FVector2D Size;
		int32 Action = 0;
	};

	// --- Écrans ---
	void DrawMainMenu();
	void DrawCombatHUD();
	void DrawCrosshair(const AStrikeCharacter* Character);
	void DrawVitals(const AStrikeCharacter* Character);
	void DrawWeaponInfo(const AStrikeCharacter* Character);
	void DrawTopBar(const AStrikeGameState* GS);
	void DrawModePanel(const AStrikeGameState* GS, const AStrikeCharacter* Character);
	void DrawKillFeed(const AStrikeGameState* GS);
	void DrawDamageIndicator(const AStrikeCharacter* Character);
	void DrawInteractPrompt(AStrikeCharacter* Character);
	void DrawScoreboard(const AStrikeGameState* GS);
	void DrawDeathScreen(const AStrikeGameState* GS);
	void DrawMatchEnd(const AStrikeGameState* GS);
	void DrawTouchControls();

	// --- Utilitaires ---
	void DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale = 1.f, bool bCentered = false);
	void DrawCircleOutline(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness = 2.f, int32 Segments = 32);
	void DrawFilledCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Segments = 24);
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FLinearColor& Back);
	bool AddMenuButton(const FString& Label, float X, float Y, float W, float H, int32 Action, bool bHighlighted = false);
	void ExecuteMenuAction(int32 Action);
	static FString FormatTime(int32 Seconds);

	AStrikePlayerController* GetStrikePC() const;

	TArray<FMenuButton> MenuButtons;
	bool bMenuHost = false;

	float UIScale = 1.f;
	FString CachedPrompt;
	float NextPromptUpdate = 0.f;
	float SmoothedFPS = 60.f;
};
