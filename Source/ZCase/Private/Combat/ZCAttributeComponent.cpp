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
	// 最大生命值至少为 1，当前生命值始终限制在合法区间内。
	MaxHealth = FMath::Max(1.0f, InMaxHealth);
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);
	// 以初始化结果同步死亡广播闸门，避免已死亡对象再次触发死亡事件。
	bDeathBroadcast = IsDead();
}

FZCDamageResult UZCAttributeComponent::ApplyDamage(const float DamageAmount)
{
	FZCDamageResult Result;
	Result.HealthBefore = CurrentHealth;
	Result.HealthAfter = CurrentHealth;

	// 非正伤害和已经死亡的对象都不产生生命值变化或事件。
	if (DamageAmount <= 0.0f || IsDead())
	{
		return Result;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	Result.HealthAfter = CurrentHealth;
	Result.AppliedDamage = Result.HealthBefore - Result.HealthAfter;
	// 只有实际扣血才通知生命值变化。
	OnHealthChanged.Broadcast(Result.HealthBefore, Result.HealthAfter);

	if (IsDead() && !bDeathBroadcast)
	{
		// bDeathBroadcast 是一次性闸门，保证死亡转移和事件只发生一次。
		bDeathBroadcast = true;
		Result.bBecameDead = true;
		OnDeath.Broadcast(GetOwner());
	}

	return Result;
}
