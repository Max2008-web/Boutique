// STRIKE ZONE - point d'apparition (joueurs par équipe, zombies, ennemis)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/StrikeTypes.h"
#include "StrikeSpawnPoint.generated.h"

class UCapsuleComponent;
class UArrowComponent;

UENUM(BlueprintType)
enum class EStrikeSpawnType : uint8
{
	Player,
	Zombie,
	Enemy
};

/**
 * Point d'apparition plaçable dans un niveau ou créé par AStrikeArenaBuilder.
 * TeamId = 255 : utilisable par toutes les équipes.
 */
UCLASS()
class STRIKEZONE_API AStrikeSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AStrikeSpawnPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	EStrikeSpawnType SpawnType = EStrikeSpawnType::Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	uint8 TeamId = StrikeTeams::NoTeam;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCapsuleComponent> Capsule;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UArrowComponent> Arrow;
#endif
};
