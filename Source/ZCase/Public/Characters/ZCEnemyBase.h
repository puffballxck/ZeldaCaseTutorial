// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/ZCTargetable.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZCEnemyBase.generated.h"

class UAnimMontage;
class UZCAttributeComponent;

/** 负责敌人受伤、受击表现和单向死亡生命周期的轻量角色基类。 */
UCLASS()
class ZCASE_API AZCEnemyBase : public ACharacter, public IZCTargetable
{
	GENERATED_BODY()

public:
	AZCEnemyBase();

	/** 敌人的生命值和死亡状态来源。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Enemy|Attributes")
	TObjectPtr<UZCAttributeComponent> Attributes;

	/** 统一接收引擎伤害，并根据伤害结果分流到受击或死亡表现。 */
	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	/** 存活敌人才允许成为锁定目标。 */
	virtual bool CanBeTargetLocked() const override;

	/** 第一版以胶囊体上半身作为锁定位置。 */
	virtual FVector GetTargetLockLocation() const override;

protected:
	/** 非致命伤害播放的全身受击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Enemy|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** 首次致命伤害播放的全身死亡蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Enemy|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

private:
	void PlayHitReact();
	void HandleDeath();

	/** 防止任何调用路径重复执行死亡表现和碰撞切换。 */
	bool bDeathStarted = false;

	/** 缺少动画配置时每类诊断最多输出一次，避免连续受击刷屏。 */
	bool bHitReactDiagnosticIssued = false;
	bool bDeathDiagnosticIssued = false;
};
