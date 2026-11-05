// STRIKE ZONE - contrôleur joueur : clavier/souris, manette, tactile, menus, réglages
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/StrikeSaveGame.h"
#include "StrikePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UStrikeTouchControls;
class AStrikeCharacter;
struct FInputActionValue;

/**
 * Contrôleur joueur.
 * - Enhanced Input : actions et contexte créés en C++ à l'exécution (aucun asset requis) ;
 *   les assets IA_* / IMC_* peuvent ensuite remplacer ce mapping (DefaultInputContext).
 * - Tactile : UStrikeTouchControls (joystick + caméra + boutons), activé automatiquement sur mobile.
 * - Réglages : sensibilités souris/manette/tactile, inversion Y, FOV, qualité graphique.
 * - Résolution adaptative de secours sur mobile (r.ScreenPercentage) basée sur les FPS.
 */
UCLASS()
class STRIKEZONE_API AStrikePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AStrikePlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

	/** Hit marker (appelé par l'arme après confirmation serveur). */
	void NotifyHitConfirmed(bool bHeadshot, bool bKill);

	UStrikeTouchControls* GetTouchControls() const { return TouchControls; }
	bool IsUsingTouch() const;
	bool IsScoreboardVisible() const { return bScoreboardVisible; }
	bool IsMenuMode() const;
	AStrikeCharacter* GetStrikeCharacter() const;

	// --- Réglages ---
	const FStrikeSettings& GetSettings() const { return Settings; }
	void UpdateSettings(const FStrikeSettings& NewSettings);

	// --- Navigation entre modes ---
	void StartMode(const FString& ModeAlias, bool bHost);
	void ReturnToMenu();

	/** Serveur -> client : extraction réussie, le butin rejoint le coffre local. */
	UFUNCTION(Client, Reliable)
	void ClientOnExtracted(const TArray<FStrikeLootItem>& Items);

	/** Serveur -> client : enregistre le record de vague. */
	UFUNCTION(Client, Reliable)
	void ClientRecordZombieWave(int32 Wave);

	// --- Commandes console ---
	UFUNCTION(Exec) void StrikeResetStats();
	UFUNCTION(Exec) void StrikeSensitivity(float Mouse, float Touch);
	UFUNCTION(Exec) void StrikeQuality(int32 Level);
	UFUNCTION(Exec) void StrikeTouch(bool bEnable);

	float LastHitMarkerTime = -100.f;
	bool bLastHitHeadshot = false;
	bool bLastHitKill = false;

protected:
	// --- Gestionnaires d'entrée ---
	void OnMove(const FInputActionValue& Value);
	void OnLookMouse(const FInputActionValue& Value);
	void OnLookGamepad(const FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnCrouchStarted();
	void OnCrouchCompleted();
	void OnSprintStarted();
	void OnSprintCompleted();
	void OnSprintToggle();
	void OnFireStarted();
	void OnFireCompleted();
	void OnAimStarted();
	void OnAimCompleted();
	void OnReload();
	void OnInteractStarted();
	void OnInteractCompleted();
	void OnNextWeapon();
	void OnPrevWeapon();
	void OnWeapon1();
	void OnWeapon2();
	void OnWeapon3();
	void OnScoreboardStarted();
	void OnScoreboardCompleted();
	void OnMenuPressed();

	void ApplyLook(float YawDegrees, float PitchDegrees);
	float GetAimSensitivityScale() const;
	void UpdateTouch(float DeltaTime);
	void HandleTouchButton(FName Id, bool bPressed);
	void UpdateAdaptiveResolution(float DeltaTime);
	void RefreshInputMode();
	void BuildDefaultInputMappings();

	UFUNCTION(Server, Reliable)
	void ServerResetStats();

	/** Optionnel : contexte d'entrée défini en asset (remplace le mapping C++). */
	UPROPERTY(EditDefaultsOnly, Category = "Strike|Input")
	TObjectPtr<UInputMappingContext> DefaultInputContext;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RuntimeContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookMouseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookGamepadAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintToggleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AimAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NextWeaponAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PrevWeaponAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Weapon1Action;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Weapon2Action;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Weapon3Action;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ScoreboardAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MenuAction;

	UPROPERTY(Transient) TObjectPtr<UStrikeTouchControls> TouchControls;

	FStrikeSettings Settings;
	bool bScoreboardVisible = false;
	bool bTouchAimToggled = false;
	bool bTouchSprinting = false;
	bool bMenuModeApplied = false;
	bool bInputModeInitialized = false;

	// Résolution adaptative
	float FpsAccumTime = 0.f;
	int32 FpsAccumFrames = 0;
	float AdaptiveScreenPercentage = 100.f;
};
