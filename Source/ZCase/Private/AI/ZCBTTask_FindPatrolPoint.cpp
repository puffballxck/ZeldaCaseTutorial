// 版权所有 Epic Games, Inc，保留所有权利

#include "AI/ZCBTTask_FindPatrolPoint.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UZCBTTask_FindPatrolPoint::UZCBTTask_FindPatrolPoint()
{
	NodeName = TEXT("Find Bokoblin Patrol Point");
	PatrolRadius = 800.0f;
	PatrolLocationKey.SelectedKeyName = TEXT("PatrolLocation");
}

EBTNodeResult::Type UZCBTTask_FindPatrolPoint::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UWorld* World = OwnerComp.GetWorld();
	// 缺少控制器、黑板、Pawn、世界或黑板键时不能写入半成品巡逻点
	if (!AIController || !Blackboard || !Pawn || !World || PatrolLocationKey.SelectedKeyName.IsNone())
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation)
	{
		return EBTNodeResult::Failed;
	}

	const float SafeRadius = FMath::IsFinite(PatrolRadius) ? FMath::Max(PatrolRadius, 0.0f) : 0.0f;
	FNavLocation PatrolLocation;
	// 导航系统只接受可达点，失败时让行为树稍后重试而不是写入不可用位置
	if (!Navigation->GetRandomReachablePointInRadius(
		Pawn->GetActorLocation(),
		SafeRadius,
		PatrolLocation))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(PatrolLocationKey.SelectedKeyName, PatrolLocation.Location);
	return EBTNodeResult::Succeeded;
}
