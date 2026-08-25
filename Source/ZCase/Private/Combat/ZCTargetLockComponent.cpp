// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCTargetLockComponent.h"

#include "Combat/ZCTargetable.h"

UZCTargetLockComponent::UZCTargetLockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UZCTargetLockComponent::SetTarget(AActor* Candidate)
{
	// 只有实现目标接口且明确允许锁定的对象才能成为目标。
	if (!IsValidTarget(Candidate))
	{
		return false;
	}

	if (CurrentTarget.Get() == Candidate)
	{
		// 重复锁定同一对象保持幂等，不重复触发目标变更事件。
		return true;
	}

	ReplaceTarget(Candidate);
	return true;
}

void UZCTargetLockComponent::ClearTarget()
{
	ReplaceTarget(nullptr);
}

void UZCTargetLockComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValidTarget(CurrentTarget.Get()))
	{
		// Tick 负责兜底清理被销毁或不再可锁定的目标。
		ClearTarget();
	}
}

void UZCTargetLockComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
	if (CurrentTarget.Get() == DestroyedActor)
	{
		// OnDestroyed 回调比下一帧 Tick 更早清除已销毁目标。
		ClearTarget();
	}
}

bool UZCTargetLockComponent::IsValidTarget(const AActor* Candidate) const
{
	if (!IsValid(Candidate) || !Candidate->GetClass()->ImplementsInterface(UZCTargetable::StaticClass()))
	{
		return false;
	}

	const IZCTargetable* Targetable = Cast<IZCTargetable>(Candidate);
	// 接口实现仍需通过自身的可锁定策略，例如死亡状态检查。
	return Targetable && Targetable->CanBeTargetLocked();
}

void UZCTargetLockComponent::ReplaceTarget(AActor* NewTarget)
{
	AActor* PreviousTarget = CurrentTarget.Get();
	if (PreviousTarget == NewTarget && !(NewTarget == nullptr && CurrentTarget.IsStale()))
	{
		return;
	}

	if (PreviousTarget)
	{
		// 替换前解绑旧目标，避免旧对象销毁时回调到当前锁定组件。
		PreviousTarget->OnDestroyed.RemoveDynamic(this, &UZCTargetLockComponent::HandleTargetDestroyed);
	}

	CurrentTarget = NewTarget;
	if (NewTarget)
	{
		// 有目标时监听销毁事件并开启 Tick，持续验证目标可用性。
		NewTarget->OnDestroyed.AddUniqueDynamic(this, &UZCTargetLockComponent::HandleTargetDestroyed);
		SetComponentTickEnabled(true);
	}
	else
	{
		// 没有目标时无需每帧验证，关闭 Tick 以结束锁定生命周期。
		SetComponentTickEnabled(false);
	}

	OnTargetChanged.Broadcast(PreviousTarget, NewTarget);
}
