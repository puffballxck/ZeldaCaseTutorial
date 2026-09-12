// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "Combat/ZCTargetable.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZCEnemyBase.generated.h"

class UAnimMontage;
class UZCAttributeComponent;
class UZCCombatComponent;

/** 负责敌人受伤、受击表现和单向死亡生命周期的轻量角色基类 */
UCLASS()
class ZCASE_API AZCEnemyBase : public ACharacter, public IZCTargetable
{
	GENERATED_BODY()

public:
	AZCEnemyBase();

	/** 敌人的生命值和死亡状态来源 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Enemy|Attributes")
	TObjectPtr<UZCAttributeComponent> Attributes;

	/** 敌人共享的攻击生命周期、命中窗口和受击/死亡打断状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Enemy|Combat")
	TObjectPtr<UZCCombatComponent> Combat;

	/** 统一接收引擎伤害，并根据伤害结果分流到受击或死亡表现 */
	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	/** 存活敌人才允许成为锁定目标 */
	virtual bool CanBeTargetLocked() const override;

	/** 返回敌人头部 Socket 的锁定锚点；缺少 Socket 时回退到胶囊体上半身 */
	virtual FVector GetTargetLockLocation() const override;

	/** 返回镜头用胸口锚点；缺少配置 Socket 时回退到稳定的胶囊体位置 */
	virtual FVector GetTargetLockCameraLocation() const override;

	/** 敌人镜头锁定优先读取的胸口 Socket；不同骨架可在蓝图中调整 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock|Camera")
	FName TargetLockCameraSocketName = TEXT("TargetLock");

protected:
	/** 初始化属性默认值、伤害回调和派生敌人的 Combat 配置 */
	virtual void BeginPlay() override;
	/** 结束时解除 Montage 回调并停止后续战斗处理 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 为派生敌人接入其攻击来源；基类只提供通用 Combat，不假设武器或骨骼端点 */
	virtual void ConfigureCombat();

	/** 非致命伤害播放的全身受击蒙太奇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Enemy|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** 首次致命伤害播放的全身死亡蒙太奇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Enemy|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	virtual void PlayHitReact();

	UFUNCTION()
	virtual void HandleDeath(AActor* DeadActor);

private:

	UFUNCTION()
	void HandleHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 防止任何调用路径重复执行死亡表现和碰撞切换 */
	bool bDeathStarted = false;

	/** 防止同步伤害回调在同一 TakeDamage 调用内递归扣血 */
	bool bDamageProcessing = false;

	/** 连续受击重置同一个蒙太奇，而不是并行叠加多个受击状态 */
	bool bHitReactActive = false;

	/** 缺少动画配置时每类诊断最多输出一次，避免连续受击刷屏 */
	bool bHitReactDiagnosticIssued = false;
	bool bDeathDiagnosticIssued = false;
};
