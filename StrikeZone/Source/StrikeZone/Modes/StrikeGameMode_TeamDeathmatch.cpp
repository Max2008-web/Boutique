// STRIKE ZONE - Menu principal et Partie rapide (match à mort par équipes 6v6 avec bots)
#include "Modes/StrikeGameModes.h"
#include "Game/StrikeGameState.h"
#include "Weapons/StrikeWeaponTypes.h"
#include "World/StrikeArenaBuilder.h"
#include "World/StrikePickup.h"

#include "Engine/World.h"

// ===========================================================================
// Menu
// ===========================================================================

AStrikeGameMode_Menu::AStrikeGameMode_Menu()
{
	ModeType = EStrikeModeType::Menu;
	ModeDisplayName = TEXT("Menu");
	ArenaLayout = EStrikeArenaLayout::Arena;
	DefaultPawnClass = nullptr;
	bUseTeams = false;
	TargetCombatants = 0;
	MatchDuration = 0;
}

void AStrikeGameMode_Menu::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// Caméra fixe survolant l'arène en arrière-plan du menu.
	NewPlayer->SetInitialLocationAndRotation(FVector(-2700.f, -2700.f, 1100.f), FRotator(-20.f, 45.f, 0.f));
}

// ===========================================================================
// Partie rapide
// ===========================================================================

AStrikeGameMode_TeamDeathmatch::AStrikeGameMode_TeamDeathmatch()
{
	ModeType = EStrikeModeType::QuickMatch;
	ModeDisplayName = TEXT("PARTIE RAPIDE - Match à mort par équipes");
	ArenaLayout = EStrikeArenaLayout::Arena;
	bUseTeams = true;
	NumTeams = 2;
	TargetCombatants = 12; // 6 contre 6, complété par des bots
	ScoreLimit = 50;
	MatchDuration = 600;
	bAllowRespawn = true;
	RespawnDelay = 3.f;
	DefaultLoadout = { AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_Pistol::StaticClass() };
}

void AStrikeGameMode_TeamDeathmatch::SetupModeActors(AStrikeArenaBuilder* InArena)
{
	// Armes puissantes au centre, réapparition périodique : contrôle de carte.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AStrikePickup* Sniper = GetWorld()->SpawnActor<AStrikePickup>(AStrikePickup::StaticClass(), FVector(0.f, 0.f, 210.f), FRotator::ZeroRotator, Params))
	{
		Sniper->SetupWeapon(AStrikeWeapon_Sniper::StaticClass());
		Sniper->RespawnTime = 30.f;
	}
	const FVector Side[] = { FVector(0.f, 2200.f, 60.f), FVector(0.f, -2200.f, 60.f) };
	for (const FVector& Location : Side)
	{
		if (AStrikePickup* Shotgun = GetWorld()->SpawnActor<AStrikePickup>(AStrikePickup::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Shotgun->SetupWeapon(AStrikeWeapon_Shotgun::StaticClass());
			Shotgun->RespawnTime = 25.f;
		}
		if (AStrikePickup* Armor = GetWorld()->SpawnActor<AStrikePickup>(AStrikePickup::StaticClass(), Location + FVector(300.f, 0.f, 0.f), FRotator::ZeroRotator, Params))
		{
			Armor->SetupArmor(50);
			Armor->RespawnTime = 20.f;
		}
	}
}

void AStrikeGameMode_TeamDeathmatch::OnKill(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot)
{
	if (!Killer || Killer == VictimController)
	{
		return;
	}
	AStrikeGameState* GS = GetStrikeGameState();
	const uint8 Team = GetTeamOfController(Killer);
	if (!GS || !GS->TeamScores.IsValidIndex(Team))
	{
		return;
	}
	++GS->TeamScores[Team];
	if (ScoreLimit > 0 && GS->TeamScores[Team] >= ScoreLimit)
	{
		FinishMatch(Team == 0 ? TEXT("Victoire de l'équipe BLEUE") : TEXT("Victoire de l'équipe ROUGE"));
	}
}
