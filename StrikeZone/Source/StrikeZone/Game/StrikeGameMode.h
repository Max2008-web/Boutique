// STRIKE ZONE - règles de partie communes (base de tous les modes)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/StrikeTypes.h"
#include "StrikeGameMode.generated.h"

class AStrikeCharacter;
class AStrikeWeapon;
class AStrikeAIController;
class AStrikeArenaBuilder;
class AStrikeGameState;
class AStrikePlayerState;

/**
 * Mode de jeu de base (serveur uniquement).
 * Gère : équipes, apparition, bots, chronomètre, scores, éliminations, réapparition,
 * fin de partie, déconnexions et construction automatique de l'arène.
 * Chaque mode (Zombies, Partie rapide, BR, Tactique, Extraction, Entraînement) en dérive.
 */
UCLASS(Config = Game)
class STRIKEZONE_API AStrikeGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AStrikeGameMode();

	// --- AGameModeBase / AGameMode ---
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;

	// --- Règles de combat (appelées par UHealthComponent / AStrikeCharacter) ---
	virtual bool CanDealDamage(AController* InstigatorController, AActor* Victim) const;
	virtual float ModifyDamage(float Damage, AController* InstigatorController, AActor* Victim) const { return Damage; }
	virtual void NotifyDamageDealt(AController* InstigatorController, AActor* Victim, float Damage, EStrikeHitZone Zone) {}
	void NotifyKilled(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot, AActor* DamageCauser);

	// --- Outils ---
	static uint8 GetTeamOfController(const AController* Controller);
	static FString GetDisplayName(const AController* Controller, const APawn* Pawn);
	EStrikeModeType GetModeType() const { return ModeType; }
	AStrikeArenaBuilder* GetArena() const { return Arena; }
	AStrikeGameState* GetStrikeGameState() const;
	TArray<AStrikeCharacter*> GetAliveCharacters(uint8 TeamFilter = StrikeTeams::NoTeam) const;
	TArray<AController*> GetCombatants() const;
	void RequestRespawn(AController* Controller, float Delay);
	bool IsMatchFinished() const;

	// --- Réglages communs ---
	UPROPERTY(EditDefaultsOnly, Category = "Strike|Mode")
	EStrikeModeType ModeType = EStrikeModeType::QuickMatch;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Mode")
	FString ModeDisplayName = TEXT("Partie");

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Mode")
	EStrikeArenaLayout ArenaLayout = EStrikeArenaLayout::Arena;

	/** Construit une arène procédurale si le niveau ne contient pas de points d'apparition. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Strike|Mode")
	bool bAutoBuildArena = true;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Teams")
	bool bUseTeams = true;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Teams")
	int32 NumTeams = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Teams")
	bool bFriendlyFire = false;

	/** Nombre total de combattants visé (joueurs + bots). 0 = pas de bots. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Strike|Bots")
	int32 TargetCombatants = 0;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Strike|Bots")
	EStrikeAIDifficulty BotDifficulty = EStrikeAIDifficulty::Normal;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Bots")
	TSubclassOf<AStrikeAIController> BotControllerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Bots")
	TSubclassOf<APawn> BotPawnClass;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Match")
	bool bAllowRespawn = true;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Match")
	float RespawnDelay = 3.f;

	/** Durée de partie en secondes (0 = illimitée). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Strike|Match")
	int32 MatchDuration = 600;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Strike|Match")
	int32 ScoreLimit = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Match")
	float PostMatchDelay = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Strike|Loadout")
	TArray<TSubclassOf<AStrikeWeapon>> DefaultLoadout;

protected:
	// --- Points d'extension des modes ---
	virtual void SetupModeActors(AStrikeArenaBuilder* InArena) {}
	virtual void OnMatchTick();
	virtual void OnMatchTimeExpired();
	virtual void OnKill(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot) {}
	virtual void ConfigureCharacter(AStrikeCharacter* Character, AController* Controller);
	virtual TArray<TSubclassOf<AStrikeWeapon>> GetLoadoutFor(AController* Controller) const { return DefaultLoadout; }
	virtual uint8 ChooseTeamFor(AController* Controller) const;
	virtual int32 GetDesiredBotCount() const;
	virtual bool ShouldRespawn(AController* VictimController) const { return bAllowRespawn; }
	virtual void OnPlayerEliminated(AController* VictimController);

	void EnsureArena();
	void BalanceBots();
	AStrikeAIController* SpawnBot();
	void RemoveBot(AStrikeAIController* Bot);
	void AssignTeam(AController* Controller, uint8 Team);
	void FinishMatch(const FString& Text);
	void SpectateSomeone(APlayerController* PC);
	void MatchTimerTick();
	int32 CountTeamMembers(uint8 Team) const;

	UPROPERTY(Transient)
	TObjectPtr<AStrikeArenaBuilder> Arena;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AStrikeAIController>> Bots;

	FTimerHandle MatchTimerHandle;
	FTimerHandle PostMatchHandle;
	int32 BotNameCounter = 0;
	mutable int32 NextFreeTeam = 0;
};
