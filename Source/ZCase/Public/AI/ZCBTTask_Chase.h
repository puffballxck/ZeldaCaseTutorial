// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "ZCBTTask_Chase.generated.h"

/** Move task whose acceptance radius is the possessed Bokoblin's attack range. */
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
