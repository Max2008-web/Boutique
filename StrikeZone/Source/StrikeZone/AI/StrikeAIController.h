// STRIKE ZONE - IA de soldat (bots des modes compétitifs, ennemis d'Extraction)
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Core/StrikeTypes.h"
#include "StrikeAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UBehaviorTree;
class UEnvQuery;
class AStrikeCharacter;

UENUM(BlueprintType)
enum class EStrikeBotState : uint8
{
	Patrol,
	Engage,
	Chase,
	TakeCover,
	Retreat
};

/**
 * IA de soldat.
 * - Perception visuelle + auditive (tirs).
 * - Cerveau natif C++ (machine à états) : patrouille, engagement, poursuite, couverture, repli.
 * - Optionnel : Behavior Tree (BehaviorTreeAsset) utilisant les nœuds UBT*Strike*, et requête EQS de couverture.
 * - Navigation : NavMesh si présent, sinon pilotage direct avec évitement simple.
 * - Difficulté : temps de réaction, erreur de visée, vitesse de visée, rafales, usage des couvertures.
 */
UCLASS()
class STRIKEZONE_API AStrikeAIController : public AAIController
{
	GENERATED_BODY()

public:
	AStrikeAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Identité ---
	uint8 GetTeamId() const { return TeamId; }
	void SetTeamId(uint8 NewTeam) { TeamId = NewTeam; }
	void SetDifficulty(EStrikeAIDifficulty NewDifficulty);
	EStrikeAIDifficulty GetDifficulty() const { return Difficulty; }
	void SetDisplayLabel(const FString& Label) { DisplayLabel = Label; }
	const FString& GetDisplayLabel() const { return DisplayLabel; }

	/** Objectif imposé par le mode (site à attaquer/défendre, centre de zone...). */
	void SetObjective(const FVector& Location, bool bInteractAtObjective);
	void ClearObjective();

	/** Zone de patrouille (ennemis d'Extraction, bots d'Entraînement). */
	void SetHomeLocation(const FVector& Location, float Radius) { HomeLocation = Location; PatrolRadius = Radius; bHasHome = true; }

	// --- API utilisée par le cerveau natif et par les nœuds de Behavior Tree ---
	void UpdateTargetSelection();
	AActor* GetCurrentTarget() const { return Target.Get(); }
	bool HasLineOfSightToTarget() const { return bTargetVisible; }
	FVector GetLastKnownTargetLocation() const { return LastKnownLocation; }
	void AimAndShoot(float DeltaSeconds);
	void StopShooting();
	bool FindCoverLocation(FVector& OutLocation) const;
	void MoveToPoint(const FVector& Destination, float AcceptanceRadius = 80.f);
	void MoveToTargetActor(AActor* Goal, float AcceptanceRadius);
	bool IsMoveFinished() const;
	EStrikeBotState GetBotState() const { return State; }

	/** Behavior Tree optionnel : s'il est défini, il remplace le cerveau natif. */
	UPROPERTY(EditAnywhere, Category = "Strike|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** Requête EQS optionnelle pour trouver une couverture (sinon recherche native). */
	UPROPERTY(EditAnywhere, Category = "Strike|AI")
	TObjectPtr<UEnvQuery> CoverQuery;

	/** Bot passif (Entraînement) : se déplace mais ne tire pas. */
	UPROPERTY(EditAnywhere, Category = "Strike|AI")
	bool bPassive = false;

	/** Se dirige vers les ennemis connus quand il n'a pas de cible (modes compétitifs). */
	UPROPERTY(EditAnywhere, Category = "Strike|AI")
	bool bRoamTowardEnemies = true;

protected:
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnPawnDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	/** Cerveau natif : appelé à chaque tick si aucun Behavior Tree n'est utilisé. */
	virtual void UpdateBrain(float DeltaSeconds);
	void UpdateMovementFallback(float DeltaSeconds);
	void FaceMovementDirection(float DeltaSeconds);
	void ChooseRoamDestination();
	void SetState(EStrikeBotState NewState);
	void UpdateBlackboard();
	bool IsEnemy(const AActor* Other) const;
	bool IsMatchOver() const;
	AStrikeCharacter* GetStrikePawn() const;

	UPROPERTY(VisibleAnywhere, Category = "Strike|AI")
	TObjectPtr<UAIPerceptionComponent> StrikePerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	// Identité
	uint8 TeamId = StrikeTeams::NoTeam;
	FString DisplayLabel = TEXT("Bot");
	EStrikeAIDifficulty Difficulty = EStrikeAIDifficulty::Normal;

	// Paramètres de difficulté
	float ReactionTime = 0.55f;
	float AimErrorDegrees = 3.5f;
	float AimInterpSpeed = 7.f;
	FVector2D BurstDuration = FVector2D(0.4f, 0.8f);
	FVector2D BurstPause = FVector2D(0.5f, 1.0f);
	bool bUsesCover = true;
	float StrafeChance = 0.5f;

	// Ciblage
	TWeakObjectPtr<AActor> Target;
	FVector LastKnownLocation = FVector::ZeroVector;
	bool bHasLastKnown = false;
	bool bTargetVisible = false;
	float LastSeenTime = -100.f;
	float TargetAcquiredTime = -100.f;
	FRotator AimError = FRotator::ZeroRotator;
	float NextAimErrorTime = 0.f;

	// Tir
	bool bFiring = false;
	float BurstEndTime = 0.f;
	float NextBurstTime = 0.f;

	// États / déplacement
	EStrikeBotState State = EStrikeBotState::Patrol;
	float StateStartTime = 0.f;
	float NextStrafeTime = 0.f;
	float LastCoverTime = -100.f;
	FVector CoverLocation = FVector::ZeroVector;
	bool bHasCover = false;
	bool bWaitingForEQS = false;
	FVector HomeLocation = FVector::ZeroVector;
	float PatrolRadius = 2500.f;
	bool bHasHome = false;
	bool bUseBehaviorTree = false;

	// Objectif de mode
	bool bHasObjective = false;
	bool bInteractAtObjective = false;
	FVector ObjectiveLocation = FVector::ZeroVector;

	// Repli sans NavMesh
	bool bDirectMove = false;
	FVector DirectDestination = FVector::ZeroVector;
	float DirectAcceptance = 100.f;
	float AvoidSign = 1.f;
	float StuckTime = 0.f;
	FVector LastPawnLocation = FVector::ZeroVector;
};

/** Ennemi IA du mode Extraction : pas de PlayerState, patrouille autour d'un point. */
UCLASS()
class STRIKEZONE_API AStrikeEnemyAIController : public AStrikeAIController
{
	GENERATED_BODY()

public:
	AStrikeEnemyAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
