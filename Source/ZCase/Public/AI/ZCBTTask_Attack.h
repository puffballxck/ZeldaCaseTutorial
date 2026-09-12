// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "ZCBTTask_Attack.generated.h"

class UZCCombatComponent;

/**
 * 启动一次 Bokoblin 攻击，并保持潜伏直到 Combat 报告
 * Montage 结束，节点按实例运行以便每个 AI 独立持有 Delegate 状态
 */
UCLASS()
class ZCASE_API UZCBTTask_Attack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UZCBTTask_Attack();

	/** 启动攻击并根据同步或异步的结束事件返回行为树结果 */
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
	/** 行为树黑板中当前攻击目标的键 */
	UPROPERTY(EditAnywhere, Category = "Attack")
	FBlackboardKeySelector TargetActorKey;

private:
	UFUNCTION()
	void HandleAttackEnded(bool bInterrupted);

	void FinishTask(EBTNodeResult::Type Result);
	void ClearActiveTask();

	TWeakObjectPtr<UBehaviorTreeComponent> ActiveOwnerComp;
	/** 当前攻击任务绑定的 Combat，结束回调只允许作用于它 */
	TWeakObjectPtr<UZCCombatComponent> ActiveCombat;
	/** 当前节点是否仍有一条尚未结束的攻击任务 */
	bool bExecuting = false;
	/** ExecuteTask 尚未返回时收到同步结束广播的延迟标记 */
	bool bAttackEndedPending = false;
	/** 延迟结束广播携带的中断状态 */
	bool bPendingInterrupted = false;
};
