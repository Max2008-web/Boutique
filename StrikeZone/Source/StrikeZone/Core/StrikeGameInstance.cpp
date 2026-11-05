// STRIKE ZONE - instance de jeu : profil local, réglages graphiques, coffre persistant
#include "Core/StrikeGameInstance.h"
#include "StrikeZone.h"

#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

const FString UStrikeGameInstance::SlotName = TEXT("StrikeZoneProfile");

void UStrikeGameInstance::Init()
{
	Super::Init();

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Profile = Cast<UStrikeSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
	if (!Profile)
	{
		Profile = Cast<UStrikeSaveGame>(UGameplayStatics::CreateSaveGameObject(UStrikeSaveGame::StaticClass()));
#if PLATFORM_ANDROID || PLATFORM_IOS
		Profile->Settings.FieldOfView = 80.f;
#endif
	}
	ApplyGraphicsSettings();
}

const FStrikeSettings& UStrikeGameInstance::GetSettings() const
{
	static const FStrikeSettings Defaults;
	return Profile ? Profile->Settings : Defaults;
}

void UStrikeGameInstance::SetSettings(const FStrikeSettings& NewSettings)
{
	if (!Profile)
	{
		return;
	}
	const bool bGraphicsChanged = Profile->Settings.GraphicsQuality != NewSettings.GraphicsQuality
		|| Profile->Settings.bDynamicResolution != NewSettings.bDynamicResolution;
	Profile->Settings = NewSettings;
	SaveProfile();
	if (bGraphicsChanged)
	{
		ApplyGraphicsSettings();
	}
}

void UStrikeGameInstance::ApplyGraphicsSettings()
{
	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (!UserSettings || !Profile)
	{
		return;
	}
	// -1 : on conserve la scalabilité fixée par le profil d'appareil (DefaultDeviceProfiles.ini).
	if (Profile->Settings.GraphicsQuality >= 0)
	{
		UserSettings->SetOverallScalabilityLevel(FMath::Clamp(Profile->Settings.GraphicsQuality, 0, 3));
	}
	UserSettings->SetDynamicResolutionEnabled(Profile->Settings.bDynamicResolution);
	UserSettings->ApplySettings(false);
}

void UStrikeGameInstance::AddToStash(const TArray<FStrikeLootItem>& Items)
{
	if (!Profile)
	{
		return;
	}
	for (const FStrikeLootItem& Item : Items)
	{
		Profile->Stash.Add(Item);
		Profile->Credits += Item.Value;
	}
	++Profile->SuccessfulExtractions;
	SaveProfile();
}

void UStrikeGameInstance::RecordZombieWave(int32 Wave)
{
	if (Profile && Wave > Profile->BestZombieWave)
	{
		Profile->BestZombieWave = Wave;
		SaveProfile();
	}
}

void UStrikeGameInstance::SaveProfile()
{
	if (Profile && !UGameplayStatics::SaveGameToSlot(Profile, SlotName, 0))
	{
		UE_LOG(LogStrike, Warning, TEXT("Échec de la sauvegarde du profil"));
	}
}
