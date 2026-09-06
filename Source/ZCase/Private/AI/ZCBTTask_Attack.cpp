// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZCBTTask_Attack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/ZCBokoblinEnemy.h"
#include "Combat/ZCCombatComponent.h"

UZCBTTask_Attack::UZCBTTask_Attack()
{
	NodeName = TEXT("Attack Bokoblin Target");
	bCreateNodeInstance = true;
	bNotifyTaskFinished = true;
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
}

EBTNodeResult::Type UZCBTTask_Attack::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AZCBokoblinEnemy* Enemy = AIController
		? Cast<AZCBokoblinEnemy>(AIController->GetPawn())
		: nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	UObject* TargetObject = Blackboard && !TargetActorKey.SelectedKeyName.IsNone()
		? Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)
		: nullptr;
	AActor* Target = Cast<AActor>(TargetObject);
	UZCCombatComponent* Combat = Enemy ? Enemy->Combat.Get() : nullptr;
	if (!Enemy || !Combat || !Target)
	{
		return EBTNodeResult::Failed;
	}

	ActiveOwnerComp = &OwnerComp;
	ActiveCombat = Combat;
	bAttackEndedPending = false;
	bPendingInterrupted = false;
	Combat->OnAttackEnded.AddUniqueDynamic(this, &UZCBTTask_Attack::HandleAttackEnded);

	// Combat can broadcast synchronously when Montage_Play fails. Keep the
	// callback in a pending slot until ExecuteTask has returned; calling
	// FinishLatentTask from this stack would re-enter the behavior tree.
	bExecuting = true;
	const bool bStarted = Enemy->TryAttack(Target);
	bExecuting = false;

	if (!bStarted)
	{
		ClearActiveTask();
		return EBTNodeResult::Failed;
	}

	if (bAttackEndedPending)
	{
		const EBTNodeResult::Type Result = bPendingInterrupted
			? EBTNodeResult::Failed
			: EBTNodeResult::Succeeded;
		ClearActiveTask();
		return Result;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UZCBTTask_Attack::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	UZCCombatComponent* Combat = ActiveCombat.Get();
	ClearActiveTask();
	if (Combat)
	{
		// Unbind first; CancelAttack may broadcast its end synchronously.
		Combat->CancelAttack();
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UZCBTTask_Attack::OnTaskFinished(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	const EBTNodeResult::Type TaskResult)
{
	ClearActiveTask();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UZCBTTask_Attack::HandleAttackEnded(const bool bInterrupted)
{
	if (!ActiveCombat.IsValid())
	{
		return;
	}

	if (bExecuting)
	{
		bAttackEndedPending = true;
		bPendingInterrupted = bInterrupted;
		return;
	}

	FinishTask(bInterrupted ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
}

void UZCBTTask_Attack::FinishTask(const EBTNodeResult::Type Result)
{
	UBehaviorTreeComponent* BehaviorTree = ActiveOwnerComp.Get();
	if (!BehaviorTree)
	{
		ClearActiveTask();
		return;
	}

	ClearActiveTask();
	FinishLatentTask(*BehaviorTree, Result);
}

void UZCBTTask_Attack::ClearActiveTask()
{
	if (UZCCombatComponent* Combat = ActiveCombat.Get())
	{
		Combat->OnAttackEnded.RemoveDynamic(this, &UZCBTTask_Attack::HandleAttackEnded);
	}
	ActiveCombat.Reset();
	ActiveOwnerComp.Reset();
	bExecuting = false;
	bAttackEndedPending = false;
	bPendingInterrupted = false;
}
