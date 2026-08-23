// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCAttributeComponent.h"

UZCAttributeComponent::UZCAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaxHealth = DefaultMaxHealth;
	CurrentHealth = DefaultHealth;
}

void UZCAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeAttributes(DefaultMaxHealth, DefaultHealth);
}

void UZCAttributeComponent::InitializeAttributes(const float InMaxHealth, const float InCurrentHealth)
{
	MaxHealth = FMath::Max(1.0f, InMaxHealth);
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);
	bDeathBroadcast = IsDead();
}

FZCDamageResult UZCAttributeComponent::ApplyDamage(const float DamageAmount)
{
	FZCDamageResult Result;
	Result.HealthBefore = CurrentHealth;
	Result.HealthAfter = CurrentHealth;

	if (DamageAmount <= 0.0f || IsDead())
	{
		return Result;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	Result.HealthAfter = CurrentHealth;
	Result.AppliedDamage = Result.HealthBefore - Result.HealthAfter;
	OnHealthChanged.Broadcast(Result.HealthBefore, Result.HealthAfter);

	if (IsDead() && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		Result.bBecameDead = true;
		OnDeath.Broadcast(GetOwner());
	}

	return Result;
}
