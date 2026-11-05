// STRIKE ZONE - statistiques répliquées d'un joueur (ou d'un bot)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/StrikeTypes.h"
#include "StrikePlayerState.generated.h"

UCLASS()
class STRIKEZONE_API AStrikePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AStrikePlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void Reset() override;

	// --- Serveur ---
	void SetTeamId(uint8 NewTeam) { TeamId = NewTeam; }
	void AddKill(bool bHeadshot);
	void AddDeath();
	void AddShotFired() { ++ShotsFired; }
	void AddShotHit(bool bHeadshot);
	void AddPoints(int32 Amount);
	bool SpendPoints(int32 Amount);
	void SetEliminated(bool bValue) { bEliminated = bValue; }
	void SetExtracted(bool bValue) { bExtracted = bValue; }
	void ResetCombatStats();

	// --- Lecture ---
	UFUNCTION(BlueprintPure, Category = "Strike") uint8 GetTeamId() const { return TeamId; }
	UFUNCTION(BlueprintPure, Category = "Strike") int32 GetKills() const { return Kills; }
	UFUNCTION(BlueprintPure, Category = "Strike") int32 GetDeaths() const { return Deaths; }
	UFUNCTION(BlueprintPure, Category = "Strike") int32 GetHeadshotKills() const { return HeadshotKills; }
	UFUNCTION(BlueprintPure, Category = "Strike") int32 GetShotsFired() const { return ShotsFired; }
	UFUNCTION(BlueprintPure, Category = "Strike") int32 GetShotsHit() const { return ShotsHit; }
	UFUNCTION(BlueprintPure, Category = "Strike") int32 GetHeadshotHits() const { return HeadshotHits; }
	UFUNCTION(BlueprintPure, Category = "Strike") int32 GetPoints() const { return Points; }
	UFUNCTION(BlueprintPure, Category = "Strike") bool IsEliminated() const { return bEliminated; }
	UFUNCTION(BlueprintPure, Category = "Strike") bool IsExtracted() const { return bExtracted; }
	UFUNCTION(BlueprintPure, Category = "Strike") float GetAccuracy() const { return ShotsFired > 0 ? static_cast<float>(ShotsHit) / ShotsFired : 0.f; }

protected:
	UPROPERTY(Replicated) uint8 TeamId = StrikeTeams::NoTeam;
	UPROPERTY(Replicated) int32 Kills = 0;
	UPROPERTY(Replicated) int32 Deaths = 0;
	UPROPERTY(Replicated) int32 HeadshotKills = 0;
	UPROPERTY(Replicated) int32 ShotsFired = 0;
	UPROPERTY(Replicated) int32 ShotsHit = 0;
	UPROPERTY(Replicated) int32 HeadshotHits = 0;
	/** Monnaie de partie (Zombies). */
	UPROPERTY(Replicated) int32 Points = 0;
	UPROPERTY(Replicated) bool bEliminated = false;
	UPROPERTY(Replicated) bool bExtracted = false;
};
