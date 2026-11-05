// STRIKE ZONE - état de partie synchronisé (score, chrono, vagues, zone, manches, objectifs)
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AStrikeGameState::AStrikeGameState()
{
	TeamScores.Init(0, 2);
}

void AStrikeGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeGameState, ModeType);
	DOREPLIFETIME(AStrikeGameState, ModeName);
	DOREPLIFETIME(AStrikeGameState, RemainingTime);
	DOREPLIFETIME(AStrikeGameState, TeamScores);
	DOREPLIFETIME(AStrikeGameState, ScoreLimit);
	DOREPLIFETIME(AStrikeGameState, bUseTeams);
	DOREPLIFETIME(AStrikeGameState, bMatchFinished);
	DOREPLIFETIME(AStrikeGameState, WinnerText);
	DOREPLIFETIME(AStrikeGameState, StatusText);
	DOREPLIFETIME(AStrikeGameState, CurrentWave);
	DOREPLIFETIME(AStrikeGameState, EnemiesRemaining);
	DOREPLIFETIME(AStrikeGameState, AliveCount);
	DOREPLIFETIME(AStrikeGameState, ZoneCenter);
	DOREPLIFETIME(AStrikeGameState, ZoneRadius);
	DOREPLIFETIME(AStrikeGameState, NextZoneCenter);
	DOREPLIFETIME(AStrikeGameState, NextZoneRadius);
	DOREPLIFETIME(AStrikeGameState, ZonePhaseTimer);
	DOREPLIFETIME(AStrikeGameState, bZoneShrinking);
	DOREPLIFETIME(AStrikeGameState, RoundNumber);
	DOREPLIFETIME(AStrikeGameState, AttackingTeam);
	DOREPLIFETIME(AStrikeGameState, ObjectiveState);
	DOREPLIFETIME(AStrikeGameState, ObjectiveProgress);
	DOREPLIFETIME(AStrikeGameState, bExtractionOpen);
}

void AStrikeGameState::MulticastKillFeed_Implementation(const FString& Killer, const FString& Victim, const FString& Weapon, bool bHeadshot, uint8 KillerTeam, uint8 VictimTeam)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FStrikeKillFeedEntry Entry;
	Entry.Killer = Killer;
	Entry.Victim = Victim;
	Entry.Weapon = Weapon;
	Entry.bHeadshot = bHeadshot;
	Entry.KillerTeam = KillerTeam;
	Entry.VictimTeam = VictimTeam;
	Entry.Time = GetWorld()->GetTimeSeconds();
	KillFeed.Insert(Entry, 0);
	if (KillFeed.Num() > 6)
	{
		KillFeed.SetNum(6);
	}
}

void AStrikeGameState::MulticastAnnouncement_Implementation(const FString& Message, float Duration)
{
	AnnouncementText = Message;
	AnnouncementEndTime = GetWorld()->GetTimeSeconds() + Duration;
}

TArray<AStrikePlayerState*> AStrikeGameState::GetStrikePlayers() const
{
	TArray<AStrikePlayerState*> Result;
	for (APlayerState* PS : PlayerArray)
	{
		if (AStrikePlayerState* StrikePS = Cast<AStrikePlayerState>(PS))
		{
			Result.Add(StrikePS);
		}
	}
	return Result;
}
