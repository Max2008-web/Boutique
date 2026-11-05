// STRIKE ZONE - Extraction : exploration, butin, ennemis IA, inventaire persistant, extraction
#include "Modes/StrikeGameModes.h"
#include "AI/StrikeAIController.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/WeaponComponent.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerController.h"
#include "Game/StrikePlayerState.h"
#include "Weapons/StrikeWeaponTypes.h"
#include "World/StrikeArenaBuilder.h"
#include "World/StrikeInteractables.h"
#include "World/StrikePickup.h"

#include "Engine/World.h"
#include "EngineUtils.h"

AStrikeGameMode_Extraction::AStrikeGameMode_Extraction()
{
	ModeType = EStrikeModeType::Extraction;
	ModeDisplayName = TEXT("EXTRACTION");
	ArenaLayout = EStrikeArenaLayout::Large;
	bUseTeams = true;
	NumTeams = 1;
	TargetCombatants = 0;
	bAllowRespawn = false;
	MatchDuration = 1200;
	DefaultLoadout = { AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_Pistol::StaticClass() };
}

void AStrikeGameMode_Extraction::ConfigureCharacter(AStrikeCharacter* Character, AController* Controller)
{
	Super::ConfigureCharacter(Character, Controller);
	UHealthComponent* Health = Character->GetHealthComponent();
	Health->RegenPerSecond = 8.f;
	Health->RegenDelay = 8.f;
	Health->AddArmor(50.f);
}

void AStrikeGameMode_Extraction::SetupModeActors(AStrikeArenaBuilder* InArena)
{
	if (!InArena)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	struct FLootDef { const TCHAR* Id; const TCHAR* Name; int32 Value; };
	const FLootDef LootTable[] = {
		{ TEXT("Drive"), TEXT("Disque dur chiffré"), 400 },
		{ TEXT("Watch"), TEXT("Montre en or"), 250 },
		{ TEXT("Meds"), TEXT("Kit médical militaire"), 120 },
		{ TEXT("Parts"), TEXT("Pièces d'arme"), 180 },
		{ TEXT("Keycard"), TEXT("Carte d'accès"), 600 },
		{ TEXT("Battery"), TEXT("Batterie militaire"), 220 },
		{ TEXT("Gold"), TEXT("Lingot d'or"), 900 },
	};

	for (const FVector& Location : InArena->LootLocations)
	{
		AStrikePickup* Pickup = GetWorld()->SpawnActor<AStrikePickup>(AStrikePickup::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (!Pickup)
		{
			continue;
		}
		const float Roll = FMath::FRand();
		if (Roll < 0.55f)
		{
			// Objets plus rares (et précieux) avec une probabilité décroissante.
			const int32 Index = FMath::Min(static_cast<int32>(UE_ARRAY_COUNT(LootTable)) - 1, FMath::FloorToInt(FMath::Pow(FMath::FRand(), 1.8f) * static_cast<int32>(UE_ARRAY_COUNT(LootTable))));
			FStrikeLootItem Item;
			Item.ItemId = LootTable[Index].Id;
			Item.DisplayName = LootTable[Index].Name;
			Item.Value = LootTable[Index].Value;
			Pickup->SetupLoot(Item);
		}
		else if (Roll < 0.8f)
		{
			Pickup->SetupAmmo(FMath::RandBool() ? EStrikeAmmoType::Rifle : EStrikeAmmoType::Pistol, 45);
		}
		else if (Roll < 0.9f)
		{
			Pickup->SetupHealth(40);
		}
		else
		{
			Pickup->SetupArmor(50);
		}
	}

	for (const FVector& Location : InArena->ExtractionLocations)
	{
		if (AStrikeObjectiveZone* Zone = GetWorld()->SpawnActor<AStrikeObjectiveZone>(AStrikeObjectiveZone::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Zone->ObjectiveType = EStrikeObjectiveType::Extraction;
			Zone->Radius = 500.f;
			Zone->Label = TEXT("EXFIL");
			Zone->SetZoneActive(false);
			ExtractionZones.Add(Zone);
		}
	}

	EnemySpawnLocations = InArena->EnemySpawns;
	for (int32 i = 0; i < EnemyCount && EnemySpawnLocations.Num() > 0; ++i)
	{
		SpawnEnemy(EnemySpawnLocations[i % EnemySpawnLocations.Num()]);
	}
}

void AStrikeGameMode_Extraction::SpawnEnemy(const FVector& Location)
{
	const FTransform SpawnTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Location + FVector(0.f, 0.f, 50.f));
	AStrikeCharacter* Enemy = GetWorld()->SpawnActorDeferred<AStrikeCharacter>(AStrikeCharacter::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return;
	}
	Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
	Enemy->FinishSpawning(SpawnTransform);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStrikeEnemyAIController* AI = GetWorld()->SpawnActor<AStrikeEnemyAIController>(AStrikeEnemyAIController::StaticClass(), Params);
	if (!AI)
	{
		Enemy->Destroy();
		return;
	}
	AI->SetDifficulty(BotDifficulty);
	AI->SetHomeLocation(Location, 1500.f);
	AI->Possess(Enemy);
	Enemy->GetHealthComponent()->bRegenerate = false;
	Enemy->GetWeaponComponent()->GiveWeapon(FMath::RandBool() ? TSubclassOf<AStrikeWeapon>(AStrikeWeapon_AssaultRifle::StaticClass()) : TSubclassOf<AStrikeWeapon>(AStrikeWeapon_SMG::StaticClass()), true);
}

int32 AStrikeGameMode_Extraction::CountAliveEnemies() const
{
	return GetAliveCharacters(StrikeTeams::Hostiles).Num();
}

void AStrikeGameMode_Extraction::OnMatchTick()
{
	Super::OnMatchTick();
	++ElapsedSeconds;
	AStrikeGameState* GS = GetStrikeGameState();

	// Ouverture des zones d'extraction.
	if (GS && !GS->bExtractionOpen)
	{
		const int32 Left = ExtractionOpenDelay - ElapsedSeconds;
		GS->StatusText = FString::Printf(TEXT("Extraction disponible dans %d s - pillez les bâtiments"), FMath::Max(0, Left));
		if (Left <= 0)
		{
			GS->bExtractionOpen = true;
			GS->StatusText = TEXT("Zones d'extraction OUVERTES (balises vertes)");
			GS->MulticastAnnouncement(TEXT("EXTRACTION DISPONIBLE"), 4.f);
			for (AStrikeObjectiveZone* Zone : ExtractionZones)
			{
				Zone->SetZoneActive(true);
			}
		}
	}

	// Progression d'extraction par joueur.
	if (GS && GS->bExtractionOpen)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			const AStrikeCharacter* Character = PC ? Cast<AStrikeCharacter>(PC->GetPawn()) : nullptr;
			if (!Character || Character->IsDead())
			{
				continue;
			}
			bool bInside = false;
			for (const AStrikeObjectiveZone* Zone : ExtractionZones)
			{
				bInside |= Zone->IsInside(Character);
			}
			int32& Progress = ExtractionProgress.FindOrAdd(PC);
			Progress = bInside ? Progress + 1 : 0;
			if (Progress >= ExtractionHoldSeconds)
			{
				ExtractPlayer(PC);
			}
		}
	}

	// Renforts ennemis.
	if (ElapsedSeconds % 30 == 0 && CountAliveEnemies() < EnemyCount / 2 && EnemySpawnLocations.Num() > 0)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			SpawnEnemy(EnemySpawnLocations[FMath::RandRange(0, EnemySpawnLocations.Num() - 1)]);
		}
	}

	CheckEndConditions();
}

void AStrikeGameMode_Extraction::ExtractPlayer(APlayerController* PC)
{
	AStrikeCharacter* Character = Cast<AStrikeCharacter>(PC->GetPawn());
	if (!Character)
	{
		return;
	}
	const TArray<FStrikeLootItem> Items = Character->GetInventoryComponent()->GetLoot();
	const int32 Value = Character->GetInventoryComponent()->GetLootValue();

	if (AStrikePlayerController* StrikePC = Cast<AStrikePlayerController>(PC))
	{
		StrikePC->ClientOnExtracted(Items);
	}
	if (AStrikePlayerState* PS = PC->GetPlayerState<AStrikePlayerState>())
	{
		PS->SetExtracted(true);
		PS->SetScore(PS->GetScore() + Value);
	}
	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->MulticastAnnouncement(FString::Printf(TEXT("%s s'est extrait (%d objets, valeur %d)"), *GetDisplayName(PC, Character), Items.Num(), Value), 4.f);
	}

	++ExtractedCount;
	ExtractionProgress.Remove(PC);
	PC->UnPossess();
	Character->Destroy();
	SpectateSomeone(PC);
}

void AStrikeGameMode_Extraction::CheckEndConditions()
{
	if (IsMatchFinished() || GetNumPlayers() == 0)
	{
		return;
	}
	int32 StillPlaying = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const AStrikeCharacter* Character = PC ? Cast<AStrikeCharacter>(PC->GetPawn()) : nullptr;
		if (Character && !Character->IsDead())
		{
			++StillPlaying;
		}
	}
	if (StillPlaying == 0)
	{
		FinishMatch(ExtractedCount > 0 ? FString::Printf(TEXT("Raid terminé : %d extrait(s)"), ExtractedCount) : TEXT("Raid échoué : aucun survivant"));
	}
}

void AStrikeGameMode_Extraction::OnMatchTimeExpired()
{
	FinishMatch(FString::Printf(TEXT("Temps écoulé : %d extrait(s), le butin des autres est perdu"), ExtractedCount));
}
