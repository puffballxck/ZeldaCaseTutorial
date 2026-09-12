// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "ZCBTTask_FindPatrolPoint.generated.h"

/** 将 Bokoblin 附近一个可到达的随机点写入 PatrolLocation */
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
	/** 在出生点附近搜索随机可达点的半径，单位为厘米 */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float PatrolRadius = 800.0f;

	/** 写入行为树黑板的巡逻位置键 */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolLocationKey;
};
