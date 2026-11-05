// STRIKE ZONE - nœuds de Behavior Tree pour composer l'IA de soldat dans l'éditeur
#include "AI/StrikeBTNodes.h"
#include "AI/StrikeAIController.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

UBTService_StrikeCombat::UBTService_StrikeCombat()
{
	NodeName = TEXT("Strike Combat Update");
	Interval = 0.2f;
	RandomDeviation = 0.05f;
}

void UBTService_StrikeCombat::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	if (AStrikeAIController* AI = Cast<AStrikeAIController>(OwnerComp.GetAIOwner()))
	{
		AI->UpdateTargetSelection();
		if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
		{
			BB->SetValueAsObject(TEXT("TargetActor"), AI->GetCurrentTarget());
			BB->SetValueAsBool(TEXT("HasLineOfSight"), AI->HasLineOfSightToTarget());
			BB->SetValueAsVector(TEXT("LastKnownLocation"), AI->GetLastKnownTargetLocation());
		}
	}
}

UBTTask_StrikeShoot::UBTTask_StrikeShoot()
{
	NodeName = TEXT("Strike Shoot");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_StrikeShoot::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AStrikeAIController* AI = Cast<AStrikeAIController>(OwnerComp.GetAIOwner());
	if (!AI || !AI->GetCurrentTarget())
	{
		return EBTNodeResult::Failed;
	}
	*reinterpret_cast<float*>(NodeMemory) = 0.f;
	return EBTNodeResult::InProgress;
}

void UBTTask_StrikeShoot::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AStrikeAIController* AI = Cast<AStrikeAIController>(OwnerComp.GetAIOwner());
	float& Elapsed = *reinterpret_cast<float*>(NodeMemory);
	Elapsed += DeltaSeconds;

	if (!AI || !AI->GetCurrentTarget())
	{
		if (AI)
		{
			AI->StopShooting();
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	AI->AimAndShoot(DeltaSeconds);
	if (Elapsed >= Duration)
	{
		AI->StopShooting();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_StrikeShoot::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AStrikeAIController* AI = Cast<AStrikeAIController>(OwnerComp.GetAIOwner()))
	{
		AI->StopShooting();
	}
	return EBTNodeResult::Aborted;
}

UBTTask_StrikeFindCover::UBTTask_StrikeFindCover()
{
	NodeName = TEXT("Strike Find Cover");
	CoverLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_StrikeFindCover, CoverLocationKey));
}

EBTNodeResult::Type UBTTask_StrikeFindCover::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AStrikeAIController* AI = Cast<AStrikeAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	FVector Cover;
	if (AI && BB && AI->FindCoverLocation(Cover))
	{
		BB->SetValueAsVector(CoverLocationKey.SelectedKeyName, Cover);
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}
