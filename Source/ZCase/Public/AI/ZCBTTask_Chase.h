// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "ZCBTTask_Chase.generated.h"

/** 移动任务，接收半径取被 Possess 的 Bokoblin 攻击距离 */
UCLASS()
class ZCASE_API UZCBTTask_Chase : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UZCBTTask_Chase();

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
};
