// STRIKE ZONE - zombies (personnage + IA de horde distincte de celle des soldats)
#pragma once

#include "CoreMinimal.h"
#include "Character/StrikeCharacter.h"
#include "AI/StrikeAIController.h"
#include "StrikeZombie.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EStrikeZombieType : uint8
{
	Walker,
	Runner,
	Brute,
	Boss
};

/** Zombie : corps à corps, plusieurs types, boss. Réutilise santé/dégâts localisés/mort du personnage. */
UCLASS()
class STRIKEZONE_API AStrikeZombie : public AStrikeCharacter
{
	GENERATED_BODY()

public:
	AStrikeZombie(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Serveur : à appeler juste après l'apparition. */
	void InitZombie(EStrikeZombieType InType, float HealthMultiplier);

	/** Serveur : tente une attaque au corps à corps. */
	bool TryAttack(AActor* Victim);

	EStrikeZombieType GetZombieType() const { return ZombieType; }
	float GetAttackRange() const { return AttackRange; }

	UPROPERTY(EditDefaultsOnly, Category = "Zombie")
	TObjectPtr<UAnimMontage> AttackMontage;

protected:
	UFUNCTION()
	void OnRep_ZombieType();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAttack();

	void ApplyTypeVisuals();

	UPROPERTY(ReplicatedUsing = OnRep_ZombieType)
	EStrikeZombieType ZombieType = EStrikeZombieType::Walker;

	float AttackRange = 150.f;
	float AttackDamage = 25.f;
	float AttackCooldown = 1.2f;
	float NextAttackTime = 0.f;
};

/** IA de horde : connaît la position des survivants, fonce, frappe ; aucune couverture ni arme. */
UCLASS()
class STRIKEZONE_API AStrikeZombieAIController : public AStrikeAIController
{
	GENERATED_BODY()

public:
	AStrikeZombieAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnPossess(APawn* InPawn) override;

protected:
	virtual void UpdateBrain(float DeltaSeconds) override;

	float NextRetargetTime = 0.f;
	float NextRepathTime = 0.f;
};
