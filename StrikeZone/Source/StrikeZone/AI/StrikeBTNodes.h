// STRIKE ZONE - nœuds de Behavior Tree pour composer l'IA de soldat dans l'éditeur
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "StrikeBTNodes.generated.h"

/**
 * Service : met à jour la cible et les clés du Blackboard
 * (TargetActor, HasLineOfSight, LastKnownLocation, HealthFraction).
 */
UCLASS(meta = (DisplayName = "Strike : Mise à jour combat"))
class STRIKEZONE_API UBTService_StrikeCombat : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_StrikeCombat();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

/** Tâche : vise et tire en rafales sur la cible pendant Duration secondes. */
UCLASS(meta = (DisplayName = "Strike : Tirer sur la cible"))
class STRIKEZONE_API UBTTask_StrikeShoot : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_StrikeShoot();

	UPROPERTY(EditAnywhere, Category = "Strike")
	float Duration = 2.f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(float); }
};

/** Tâche : cherche une couverture et écrit sa position dans la clé choisie. */
UCLASS(meta = (DisplayName = "Strike : Trouver une couverture"))
class STRIKEZONE_API UBTTask_StrikeFindCover : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_StrikeFindCover();

	UPROPERTY(EditAnywhere, Category = "Strike")
	FBlackboardKeySelector CoverLocationKey;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
