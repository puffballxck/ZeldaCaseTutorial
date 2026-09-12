// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ZCAttributeComponent.generated.h"

USTRUCT(BlueprintType)
/** 一次伤害结算的前后生命值与死亡转移结果 */
struct ZCASE_API FZCDamageResult
{
	GENERATED_BODY()

	/** 施加伤害前的生命值 */
	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	float HealthBefore = 0.0f;

	/** 施加伤害并完成钳制后的生命值 */
	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	float HealthAfter = 0.0f;

	/** 本次实际扣除的生命值；无效伤害或死亡后的伤害为零 */
	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	float AppliedDamage = 0.0f;

	/** 本次调用是否首次完成从存活到死亡的单向转移 */
	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	bool bBecameDead = false;
};

/** 生命值发生实际变化时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCHealthChangedSignature,
	float, PreviousHealth,
	float, CurrentHealth);

/** 首次进入死亡状态时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FZCDeathSignature,
	AActor*, DeadActor);

/** 负责生命值钳制，以及只允许发生一次的死亡状态转移 */
UCLASS(ClassGroup = (ZCase), meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCAttributeComponent();

	/** 使用给定的最大生命值和当前生命值初始化属性；输入值会被钳制到合法范围 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Attributes")
	void InitializeAttributes(float InMaxHealth, float InCurrentHealth);

	/** 应用正数伤害并返回本次生命值变化；死亡后的重复伤害会被忽略 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Attributes")
	FZCDamageResult ApplyDamage(float DamageAmount);

	/** 返回当前生命值 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Attributes")
	float GetHealth() const { return CurrentHealth; }

	/** 返回经过下限保护的最大生命值 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Attributes")
	float GetMaxHealth() const { return MaxHealth; }

	/** 当前生命值为零时返回 true */
	UFUNCTION(BlueprintPure, Category = "ZCase|Attributes")
	bool IsDead() const { return CurrentHealth <= 0.0f; }

	/** 每次实际扣血后广播，参数依次为扣血前和扣血后生命值 */
	UPROPERTY(BlueprintAssignable, Category = "ZCase|Attributes")
	FZCHealthChangedSignature OnHealthChanged;

	/** 仅在首次进入死亡状态时广播 */
	UPROPERTY(BlueprintAssignable, Category = "ZCase|Attributes")
	FZCDeathSignature OnDeath;

protected:
	virtual void BeginPlay() override;

	/** 默认最大生命值，运行时仍会强制保持至少为 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZCase|Attributes", meta = (ClampMin = "1.0"))
	float DefaultMaxHealth = 100.0f;

	/** 默认当前生命值，运行时会被限制在 0 到最大生命值之间 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZCase|Attributes", meta = (ClampMin = "0.0"))
	float DefaultHealth = 100.0f;

private:
	/** 当前生效的最大生命值 */
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Attributes")
	float MaxHealth = 100.0f;

	/** 当前生效的生命值 */
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Attributes")
	float CurrentHealth = 100.0f;

	/** 防止死亡事件在后续重复调用中再次广播 */
	bool bDeathBroadcast = false;
};
