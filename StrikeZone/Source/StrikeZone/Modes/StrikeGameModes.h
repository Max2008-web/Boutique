// STRIKE ZONE - les modes de jeu : Menu, Zombies, Partie rapide, Battle Royale, Tactique, Extraction, Entraînement
#pragma once

#include "CoreMinimal.h"
#include "Game/StrikeGameMode.h"
#include "AI/StrikeZombie.h"
#include "StrikeGameModes.generated.h"

class AStrikeObjectiveZone;
class AStrikeAIController;

// ===========================================================================
// Menu principal
// ===========================================================================

/** Arène en arrière-plan, aucun pion : le HUD affiche le menu. */
UCLASS()
class STRIKEZONE_API AStrikeGameMode_Menu : public AStrikeGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode_Menu();
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override { return false; }
};

// ===========================================================================
// 1. Zombies
// ===========================================================================

/** Survie coopérative : vagues progressives, types de zombies, boss, achats muraux, monnaie de partie. */
UCLASS(Config = Game)
class STRIKEZONE_API AStrikeGameMode_Zombies : public AStrikeGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode_Zombies();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleMatchHasStarted() override;
	virtual void NotifyDamageDealt(AController* InstigatorController, AActor* Victim, float Damage, EStrikeHitZone Zone) override;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 StartingPoints = 500;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 BaseZombiesPerWave = 6;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 ZombiesPerWaveGrowth = 3;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 MaxSimultaneousZombies = 24;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 IntermissionSeconds = 10;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 BossWaveInterval = 5;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") float HealthGrowthPerWave = 0.12f;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 PointsPerHit = 10;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 PointsPerKill = 60;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Zombies") int32 PointsPerHeadshotKill = 100;

protected:
	virtual void SetupModeActors(AStrikeArenaBuilder* InArena) override;
	virtual void OnMatchTick() override;
	virtual void OnKill(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot) override;
	virtual uint8 ChooseTeamFor(AController* Controller) const override { return 0; }
	virtual bool ShouldRespawn(AController* VictimController) const override { return false; }
	virtual void OnPlayerEliminated(AController* VictimController) override;

	void StartWave();
	void EndWave();
	AStrikeZombie* SpawnZombie(EStrikeZombieType Type);
	int32 CountAliveZombies() const;
	int32 CountAlivePlayers() const;

	int32 CurrentWave = 0;
	int32 ZombiesToSpawn = 0;
	int32 IntermissionLeft = 0;
	bool bInWave = false;
	bool bBossPending = false;
};

// ===========================================================================
// 2. Partie rapide (match à mort par équipes 6v6)
// ===========================================================================

UCLASS(Config = Game)
class STRIKEZONE_API AStrikeGameMode_TeamDeathmatch : public AStrikeGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode_TeamDeathmatch();

protected:
	virtual void OnKill(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot) override;
	virtual void SetupModeActors(AStrikeArenaBuilder* InArena) override;
};

// ===========================================================================
// 3. Battle Royale
// ===========================================================================

USTRUCT()
struct FStrikeZonePhase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Zone") int32 WaitSeconds = 60;
	UPROPERTY(EditAnywhere, Category = "Zone") int32 ShrinkSeconds = 45;
	/** Rayon final en fraction du rayon initial. */
	UPROPERTY(EditAnywhere, Category = "Zone") float RadiusFraction = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Zone") float DamagePerSecond = 2.f;
};

/** Chacun pour soi, jusqu'à 30 combattants, butin au sol, zone qui se resserre. */
UCLASS(Config = Game)
class STRIKEZONE_API AStrikeGameMode_BattleRoyale : public AStrikeGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode_BattleRoyale();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleMatchHasStarted() override;

	UPROPERTY(EditDefaultsOnly, Config, Category = "BattleRoyale") int32 MaxCombatants = 30;
	UPROPERTY(EditDefaultsOnly, Category = "BattleRoyale") TArray<FStrikeZonePhase> Phases;

protected:
	virtual void SetupModeActors(AStrikeArenaBuilder* InArena) override;
	virtual void ConfigureCharacter(AStrikeCharacter* Character, AController* Controller) override;
	virtual TArray<TSubclassOf<AStrikeWeapon>> GetLoadoutFor(AController* Controller) const override;
	virtual void OnMatchTick() override;
	virtual void OnMatchTimeExpired() override;

	void UpdateZone();
	void ApplyZoneDamage();
	void GuideBotsIntoZone();

	int32 PhaseIndex = 0;
	int32 PhaseTimer = 0;
	bool bShrinking = false;
	float InitialRadius = 0.f;
	float StartRadius = 0.f;
	float TargetRadius = 0.f;
	FVector StartCenter = FVector::ZeroVector;
	FVector TargetCenter = FVector::ZeroVector;
	float CurrentDamagePerSecond = 1.f;
	int32 SecondsSinceStart = 0;
};

// ===========================================================================
// 4. Tactique (5v5, manches, objectifs)
// ===========================================================================

UENUM()
enum class EStrikeRoundPhase : uint8
{
	PreRound,
	Live,
	PostRound
};

/** Manches sans réapparition : les attaquants posent une charge sur un site, les défenseurs la désamorcent. */
UCLASS(Config = Game)
class STRIKEZONE_API AStrikeGameMode_Tactical : public AStrikeGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode_Tactical();

	virtual void HandleMatchHasStarted() override;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Tactical") int32 RoundsToWin = 7;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Tactical") int32 RoundSeconds = 115;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Tactical") int32 PreRoundSeconds = 5;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Tactical") int32 PostRoundSeconds = 5;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Tactical") int32 DetonationSeconds = 40;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Tactical") float PlantSeconds = 4.f;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Tactical") float DefuseSeconds = 6.f;

protected:
	virtual void SetupModeActors(AStrikeArenaBuilder* InArena) override;
	virtual void ConfigureCharacter(AStrikeCharacter* Character, AController* Controller) override;
	virtual void OnMatchTick() override;
	virtual bool ShouldRespawn(AController* VictimController) const override { return false; }

	void StartRound();
	void EndRound(uint8 WinningTeam, const FString& Reason);
	void ObjectiveTick();
	void SetFrozen(bool bFrozen);
	void UpdateBotObjectives();
	uint8 GetAttackingTeam() const;
	uint8 GetDefendingTeam() const { return GetAttackingTeam() == 0 ? 1 : 0; }

	UPROPERTY(Transient)
	TArray<TObjectPtr<AStrikeObjectiveZone>> Sites;

	UPROPERTY(Transient)
	TObjectPtr<AStrikeObjectiveZone> PlantedSite;

	EStrikeRoundPhase Phase = EStrikeRoundPhase::PreRound;
	int32 PhaseTimer = 0;
	int32 RoundNumber = 0;
	float PlantProgress = 0.f;
	float DefuseProgress = 0.f;
	FTimerHandle ObjectiveTimer;
};

// ===========================================================================
// 5. Extraction
// ===========================================================================

/** JcE coopératif : explorer, piller, survivre aux mercenaires IA et s'extraire pour conserver le butin. */
UCLASS(Config = Game)
class STRIKEZONE_API AStrikeGameMode_Extraction : public AStrikeGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode_Extraction();

	UPROPERTY(EditDefaultsOnly, Config, Category = "Extraction") int32 EnemyCount = 18;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Extraction") int32 ExtractionOpenDelay = 90;
	UPROPERTY(EditDefaultsOnly, Config, Category = "Extraction") int32 ExtractionHoldSeconds = 8;

protected:
	virtual void SetupModeActors(AStrikeArenaBuilder* InArena) override;
	virtual void ConfigureCharacter(AStrikeCharacter* Character, AController* Controller) override;
	virtual void OnMatchTick() override;
	virtual void OnMatchTimeExpired() override;
	virtual uint8 ChooseTeamFor(AController* Controller) const override { return 0; }
	virtual bool ShouldRespawn(AController* VictimController) const override { return false; }

	void SpawnEnemy(const FVector& Location);
	void ExtractPlayer(APlayerController* PC);
	void CheckEndConditions();
	int32 CountAliveEnemies() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AStrikeObjectiveZone>> ExtractionZones;

	UPROPERTY(Transient)
	TArray<FVector> EnemySpawnLocations;

	TMap<TWeakObjectPtr<APlayerController>, int32> ExtractionProgress;
	int32 ElapsedSeconds = 0;
	int32 ExtractedCount = 0;
};

// ===========================================================================
// 6. Entraînement
// ===========================================================================

/** Stand de tir : cibles fixes et mobiles, râteliers d'armes, bots passifs, munitions infinies, statistiques. */
UCLASS(Config = Game)
class STRIKEZONE_API AStrikeGameMode_Training : public AStrikeGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode_Training();

protected:
	virtual void SetupModeActors(AStrikeArenaBuilder* InArena) override;
	virtual void ConfigureCharacter(AStrikeCharacter* Character, AController* Controller) override;
	virtual void OnKill(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot) override;
	virtual uint8 ChooseTeamFor(AController* Controller) const override { return 0; }

	void SpawnTrainingBot(const FVector& Location);
};
