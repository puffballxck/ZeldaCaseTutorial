// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "ZCBTTask_FindPatrolPoint.generated.h"

/** Writes one reachable random point near the Bokoblin into PatrolLocation. */
UCLASS()
class ZCASE_API UZCBTTask_FindPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UZCBTTask_FindPatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float PatrolRadius = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolLocationKey;
};
