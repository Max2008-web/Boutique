// STRIKE ZONE - construction procédurale des cartes (aucun asset requis)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/StrikeTypes.h"
#include "StrikeArenaBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Construit une carte jouable à partir de cubes instanciés (moteur uniquement) :
 * sol, murs, couvertures symétriques, rampes, bâtiments, stand de tir, éclairage.
 *
 * - Répliqué : chaque client reconstruit la même géométrie à partir de Layout + Seed.
 * - Serveur : place aussi les marqueurs de gameplay (apparitions, achats, butin, objectifs).
 * - Les niveaux faits main restent prioritaires : il suffit d'y placer des AStrikeSpawnPoint.
 */
UCLASS()
class STRIKEZONE_API AStrikeArenaBuilder : public AActor
{
	GENERATED_BODY()

public:
	AStrikeArenaBuilder();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Idempotent : construit la géométrie (tous) et les marqueurs (serveur). */
	void BuildArena();

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Arena")
	EStrikeArenaLayout Layout = EStrikeArenaLayout::Arena;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Arena")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, Category = "Arena")
	bool bSpawnLightingIfMissing = true;

	// --- Marqueurs de gameplay (serveur) ---
	UPROPERTY(Transient) TArray<FVector> TeamSpawns0;
	UPROPERTY(Transient) TArray<FVector> TeamSpawns1;
	UPROPERTY(Transient) TArray<FVector> FreeSpawns;
	UPROPERTY(Transient) TArray<FVector> ZombieSpawns;
	UPROPERTY(Transient) TArray<FVector> EnemySpawns;
	UPROPERTY(Transient) TArray<FTransform> BuyLocations;
	UPROPERTY(Transient) TArray<FVector> ObjectiveLocations;
	UPROPERTY(Transient) TArray<FVector> LootLocations;
	UPROPERTY(Transient) TArray<FVector> ExtractionLocations;
	UPROPERTY(Transient) TArray<FVector> TargetLocations;
	UPROPERTY(Transient) TArray<FVector> MovingTargetLocations;
	UPROPERTY(Transient) TArray<FVector> RackLocations;
	UPROPERTY(Transient) TArray<FVector> BotLocations;

	/** Demi-taille de la zone jouable (cm). */
	float GetHalfSize() const;

	/** Point aléatoire au sol, hors des obstacles connus. */
	FVector GetRandomOpenPoint(FRandomStream& Stream, float Margin = 300.f) const;

protected:
	void AddBlock(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator, bool bIsObstacle = true);
	bool IsPointBlocked(const FVector& Point, float Radius) const;

	void BuildArenaLayout(FRandomStream& Stream);
	void BuildLargeLayout(FRandomStream& Stream);
	void BuildRangeLayout(FRandomStream& Stream);
	void BuildBuilding(FRandomStream& Stream, const FVector& Center, const FVector2D& Size, float Height);
	void SpawnLightingIfNeeded();
	void SpawnServerMarkers();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> FloorBlocks;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> WallBlocks;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> CoverBlocks;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> AccentBlocks;

	TArray<FBox> Obstacles;
	bool bBuilt = false;
};
