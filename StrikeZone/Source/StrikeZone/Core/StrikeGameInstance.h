// STRIKE ZONE - instance de jeu : profil local, réglages graphiques, coffre persistant
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/StrikeSaveGame.h"
#include "StrikeGameInstance.generated.h"

UCLASS()
class STRIKEZONE_API UStrikeGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	const FStrikeSettings& GetSettings() const;
	void SetSettings(const FStrikeSettings& NewSettings);

	/** Applique la qualité graphique (scalabilité) et la résolution dynamique. */
	void ApplyGraphicsSettings();

	/** Extraction réussie : ajoute le butin au coffre et sauvegarde. */
	void AddToStash(const TArray<FStrikeLootItem>& Items);
	void RecordZombieWave(int32 Wave);

	const UStrikeSaveGame* GetProfile() const { return Profile; }
	void SaveProfile();

	static const FString SlotName;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UStrikeSaveGame> Profile;
};
