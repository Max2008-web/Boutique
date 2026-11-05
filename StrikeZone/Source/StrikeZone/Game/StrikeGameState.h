// STRIKE ZONE - état de partie synchronisé (score, chrono, vagues, zone, manches, objectifs)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/StrikeTypes.h"
#include "StrikeGameState.generated.h"

class AStrikePlayerState;

/** Entrée locale du fil des éliminations. */
struct FStrikeKillFeedEntry
{
	FString Killer;
	FString Victim;
	FString Weapon;
	bool bHeadshot = false;
	uint8 KillerTeam = StrikeTeams::NoTeam;
	uint8 VictimTeam = StrikeTeams::NoTeam;
	float Time = 0.f;
};

UCLASS()
class STRIKEZONE_API AStrikeGameState : public AGameState
{
	GENERATED_BODY()

public:
	AStrikeGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Serveur -> tous : ajoute une ligne au fil des éliminations. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKillFeed(const FString& Killer, const FString& Victim, const FString& Weapon, bool bHeadshot, uint8 KillerTeam, uint8 VictimTeam);

	/** Serveur -> tous : message d'annonce temporaire (vague, manche, zone...). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAnnouncement(const FString& Message, float Duration);

	TArray<AStrikePlayerState*> GetStrikePlayers() const;
	int32 GetTeamScore(int32 Team) const { return TeamScores.IsValidIndex(Team) ? TeamScores[Team] : 0; }

	// --- Général ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") EStrikeModeType ModeType = EStrikeModeType::Menu;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") FString ModeName;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") int32 RemainingTime = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") TArray<int32> TeamScores;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") int32 ScoreLimit = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") bool bUseTeams = true;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") bool bMatchFinished = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") FString WinnerText;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike") FString StatusText;

	// --- Zombies ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|Zombies") int32 CurrentWave = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|Zombies") int32 EnemiesRemaining = 0;

	// --- Battle Royale ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|BR") int32 AliveCount = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|BR") FVector_NetQuantize ZoneCenter;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|BR") float ZoneRadius = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|BR") FVector_NetQuantize NextZoneCenter;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|BR") float NextZoneRadius = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|BR") int32 ZonePhaseTimer = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|BR") bool bZoneShrinking = false;

	// --- Tactique ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|Tactical") int32 RoundNumber = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|Tactical") uint8 AttackingTeam = 0;
	/** 0 = aucun, 1 = pose en cours, 2 = charge posée, 3 = désamorçage en cours. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|Tactical") uint8 ObjectiveState = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|Tactical") float ObjectiveProgress = 0.f;

	// --- Extraction ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Strike|Extraction") bool bExtractionOpen = false;

	// --- Local (non répliqué) ---
	TArray<FStrikeKillFeedEntry> KillFeed;
	FString AnnouncementText;
	float AnnouncementEndTime = 0.f;
};
