// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCCombatComponent.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UZCCombatComponent::UZCCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UZCCombatComponent::StartAttack()
{
	bAttackActive = true;
	bTraceActive = false;
	HitActors.Reset();
}

bool UZCCombatComponent::BeginTrace()
{
	if (!bAttackActive)
	{
		return false;
	}

	bTraceActive = true;
	return true;
}

void UZCCombatComponent::EndTrace()
{
	bTraceActive = false;
}

bool UZCCombatComponent::TryApplyHit(AActor* Target, const float DamageAmount)
{
	if (!bAttackActive || !bTraceActive || !IsValid(Target) || DamageAmount <= 0.0f)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (Owner && Target == Owner)
	{
		return false;
	}

	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (HitActors.Contains(TargetKey))
	{
		return false;
	}

	HitActors.Add(TargetKey);
	AController* InstigatorController = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	UGameplayStatics::ApplyDamage(Target, DamageAmount, InstigatorController, Owner, UDamageType::StaticClass());
	return true;
}
