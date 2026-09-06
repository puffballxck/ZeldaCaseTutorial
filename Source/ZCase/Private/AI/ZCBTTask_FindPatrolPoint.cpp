// Copyright Epic Games, Inc. All Rights Reserved.

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
