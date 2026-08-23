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
	if (!IsValidTarget(Candidate))
	{
		return false;
	}

	if (CurrentTarget.Get() == Candidate)
	{
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
		ClearTarget();
	}
}

void UZCTargetLockComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
	if (CurrentTarget.Get() == DestroyedActor)
	{
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
		PreviousTarget->OnDestroyed.RemoveDynamic(this, &UZCTargetLockComponent::HandleTargetDestroyed);
	}

	CurrentTarget = NewTarget;
	if (NewTarget)
	{
		NewTarget->OnDestroyed.AddUniqueDynamic(this, &UZCTargetLockComponent::HandleTargetDestroyed);
		SetComponentTickEnabled(true);
	}
	else
	{
		SetComponentTickEnabled(false);
	}

	OnTargetChanged.Broadcast(PreviousTarget, NewTarget);
}
