// STRIKE ZONE - statistiques répliquées d'un joueur (ou d'un bot)
#include "Game/StrikePlayerState.h"
#include "Net/UnrealNetwork.h"

AStrikePlayerState::AStrikePlayerState()
{
	SetNetUpdateFrequency(10.f);
}

void AStrikePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikePlayerState, TeamId);
	DOREPLIFETIME(AStrikePlayerState, Kills);
	DOREPLIFETIME(AStrikePlayerState, Deaths);
	DOREPLIFETIME(AStrikePlayerState, HeadshotKills);
	DOREPLIFETIME(AStrikePlayerState, ShotsFired);
	DOREPLIFETIME(AStrikePlayerState, ShotsHit);
	DOREPLIFETIME(AStrikePlayerState, HeadshotHits);
	DOREPLIFETIME(AStrikePlayerState, Points);
	DOREPLIFETIME(AStrikePlayerState, bEliminated);
	DOREPLIFETIME(AStrikePlayerState, bExtracted);
}

void AStrikePlayerState::CopyProperties(APlayerState* PlayerState)
{
	// Conserve les statistiques lors d'une reconnexion (joueur inactif) ou d'un voyage seamless.
	Super::CopyProperties(PlayerState);
	if (AStrikePlayerState* Target = Cast<AStrikePlayerState>(PlayerState))
	{
		Target->TeamId = TeamId;
		Target->Kills = Kills;
		Target->Deaths = Deaths;
		Target->HeadshotKills = HeadshotKills;
		Target->ShotsFired = ShotsFired;
		Target->ShotsHit = ShotsHit;
		Target->HeadshotHits = HeadshotHits;
		Target->Points = Points;
		Target->bEliminated = bEliminated;
	}
}

void AStrikePlayerState::Reset()
{
	Super::Reset();
	ResetCombatStats();
}

void AStrikePlayerState::ResetCombatStats()
{
	Kills = 0;
	Deaths = 0;
	HeadshotKills = 0;
	ShotsFired = 0;
	ShotsHit = 0;
	HeadshotHits = 0;
	bEliminated = false;
	bExtracted = false;
	SetScore(0.f);
}

void AStrikePlayerState::AddKill(bool bHeadshot)
{
	++Kills;
	if (bHeadshot)
	{
		++HeadshotKills;
	}
	SetScore(GetScore() + (bHeadshot ? 150.f : 100.f));
}

void AStrikePlayerState::AddDeath()
{
	++Deaths;
}

void AStrikePlayerState::AddShotHit(bool bHeadshot)
{
	++ShotsHit;
	if (bHeadshot)
	{
		++HeadshotHits;
	}
}

void AStrikePlayerState::AddPoints(int32 Amount)
{
	Points = FMath::Max(0, Points + Amount);
}

bool AStrikePlayerState::SpendPoints(int32 Amount)
{
	if (Amount < 0 || Points < Amount)
	{
		return false;
	}
	Points -= Amount;
	return true;
}
