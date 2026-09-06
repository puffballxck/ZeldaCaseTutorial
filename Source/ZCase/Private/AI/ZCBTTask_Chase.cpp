// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZCBTTask_Chase.h"

#include "AIController.h"
#include "Characters/ZCBokoblinEnemy.h"

UZCBTTask_Chase::UZCBTTask_Chase()
{
	NodeName = TEXT("Chase Bokoblin Target");
	bCreateNodeInstance = true;
	BlackboardKey.SelectedKeyName = TEXT("TargetActor");
	bReachTestIncludesAgentRadius = false;
	bReachTestIncludesGoalRadius = false;
	bAllowPartialPath = false;
	bAllowStrafe = true;
	bTrackMovingGoal = true;
}

EBTNodeResult::Type UZCBTTask_Chase::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		if (AZCBokoblinEnemy* Enemy = Cast<AZCBokoblinEnemy>(AIController->GetPawn()))
		{
			AcceptableRadius = FMath::IsFinite(Enemy->AttackRange)
				? FMath::Max(Enemy->AttackRange, 0.0f)
				: 0.0f;
		}
	}

	return Super::ExecuteTask(OwnerComp, NodeMemory);
}
