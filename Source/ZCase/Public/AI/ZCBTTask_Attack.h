// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "ZCBTTask_Attack.generated.h"

class UZCCombatComponent;

/**
 * Starts a Bokoblin attack and remains latent until Combat reports its
 * montage end. The node is instanced so each AI owns its delegate state.
 */
UCLASS()
class ZCASE_API UZCBTTask_Attack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UZCBTTask_Attack();

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Attack")
	FBlackboardKeySelector TargetActorKey;

private:
	UFUNCTION()
	void HandleAttackEnded(bool bInterrupted);

	void FinishTask(EBTNodeResult::Type Result);
	void ClearActiveTask();

	TWeakObjectPtr<UBehaviorTreeComponent> ActiveOwnerComp;
	TWeakObjectPtr<UZCCombatComponent> ActiveCombat;
	bool bExecuting = false;
	bool bAttackEndedPending = false;
	bool bPendingInterrupted = false;
};
