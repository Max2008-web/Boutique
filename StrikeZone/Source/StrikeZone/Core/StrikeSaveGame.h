// STRIKE ZONE - sauvegarde locale : réglages, coffre persistant (Extraction), records
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/StrikeTypes.h"
#include "StrikeSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FStrikeSettings
{
	GENERATED_BODY()

	/** Multiplicateur souris (1 = réglage moteur standard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") float MouseSensitivity = 1.f;
	/** Multiplicateur manette (1 = 180°/s à fond). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") float GamepadSensitivity = 1.f;
	/** Multiplicateur tactile (1 = 0.15° par pixel à 1080p). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") float TouchSensitivity = 1.f;
	/** Multiplicateur de sensibilité en visée ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") float AimSensitivityMultiplier = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") bool bInvertY = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") float FieldOfView = 90.f;
	/** -1 = automatique (profil d'appareil), 0..3 = Bas, Moyen, Élevé, Épique. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") int32 GraphicsQuality = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") bool bDynamicResolution = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") bool bForceTouchControls = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") bool bShowFPS = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings") int32 BotDifficulty = 1;
};

UCLASS()
class STRIKEZONE_API UStrikeSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame) FStrikeSettings Settings;

	/** Coffre persistant alimenté par les extractions réussies. */
	UPROPERTY(SaveGame) TArray<FStrikeLootItem> Stash;
	UPROPERTY(SaveGame) int32 Credits = 0;
	UPROPERTY(SaveGame) int32 SuccessfulExtractions = 0;
	UPROPERTY(SaveGame) int32 BestZombieWave = 0;
};
