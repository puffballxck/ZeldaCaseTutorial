// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "Characters/ZCEnemyBase.h"
#include "ZCBokoblinEnemy.generated.h"

class AActor;
class UAnimMontage;
class UBehaviorTree;
class UBrainComponent;

UENUM(BlueprintType)
/** 招架成功后驱动倒地、停留和起身表现的阶段 */
enum class EZCBokoblinParryStage : uint8
{
	None,
	Knockdown,
	DownWait,
	GettingUp
};

/**
 * Bokoblin 专属的战斗与 AI 数据
 *
 * 敌人基类负责伤害、受击、死亡和可复用的 Combat
 * 组件，本类只提供行为树所需的少量攻击策略
 * Bokoblin 行为树
 */
UCLASS()
class ZCASE_API AZCBokoblinEnemy : public AZCEnemyBase
{
	GENERATED_BODY()

public:
	AZCBokoblinEnemy();

	/** 敌人被 AZCBokoblinAIController Possess 后启动的行为树 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	/** 候选攻击 Montage，每次攻击选择一个有效条目 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Animation")
	TArray<UAnimMontage*> AttackMontages;

	/** Chase 成功并允许开始攻击的距离，单位为厘米 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 130.0f;

	/** 巡逻移动速度，单位为厘米每秒 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Movement", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 180.0f;

	/** 追击移动速度，单位为厘米每秒 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 400.0f;

	/** 与初始出生点的最大水平距离；超过后停止战斗并跑回 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI", meta = (ClampMin = "100.0", Units = "cm"))
	float MaxChaseDistance = 2000.0f;

	/** 返回出生点的水平到达容差，不包含胶囊半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI", meta = (ClampMin = "1.0", Units = "cm"))
	float ReturnAcceptanceRadius = 75.0f;

	/** 对范围内存活的玩家控制 Pawn 启动一次选中的攻击 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Bokoblin|Combat")
	bool TryAttack(AActor* Target);

	/** 仅由玩家成功弹反当前攻击时调用 */
	void HandleAttackParried();

	UFUNCTION(BlueprintPure, Category = "ZCase|Bokoblin|Combat")
	bool IsParryRecovering() const { return bParryRecovering; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Bokoblin|Combat")
	bool ShouldUseParryDownPose() const { return bParryRecovering && bParryDownPose; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Bokoblin|Combat")
	EZCBokoblinParryStage GetParryStage() const { return ParryStage; }

protected:
	/** 为 Bokoblin 配置徒手骨骼 Trace 与攻击伤害 */
	virtual void ConfigureCombat() override;
	/** 播放受击表现，正在招架恢复时由专用姿态接管 */
	virtual void PlayHitReact() override;
	/** 停止 AI、攻击和恢复流程，再进入死亡状态 */
	virtual void HandleDeath(AActor* DeadActor) override;
	/** 释放恢复 Timer 与脑组件引用，避免结束世界后回调 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 招架成功后播放的倒地 Montage */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Bokoblin|Combat|Parry")
	TObjectPtr<UAnimMontage> ParryKnockdownMontage;

	/** 招架恢复阶段播放的起身 Montage */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Bokoblin|Combat|Parry")
	TObjectPtr<UAnimMontage> ParryGetupMontage;

	/** 倒地后停留时间，期间保持攻击/移动锁定 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Bokoblin|Combat|Parry", meta = (ClampMin = "0.0", Units = "s"))
	float ParryDownWaitDuration = 0.6f;

	/** 该敌人所有攻击 Montage 共享的拳击伤害与 Trace 配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace", meta = (ClampMin = "0.0"))
	float AttackDamage = 15.0f;

	/** 徒手拳击 Trace 的球体半径，单位为厘米 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace", meta = (ClampMin = "0.0"))
	float AttackTraceRadius = 20.0f;

	/** 徒手右拳 Trace 使用的起点与终点骨骼或 Socket 名称 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace|Points")
	FName AttackTraceBasePoint = TEXT("Wrist_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace|Points")
	FName AttackTraceTipPoint = TEXT("Finger_B_2_R");

private:
	/** 启动倒地或起身 Montage，并保存恢复阶段的控制权 */
	void StartParryRecoveryStage(bool bGetup);
	/** 倒地 Montage 结束后进入固定停留阶段 */
	void EnterParryDownWait();
	/** 按阶段截止时间推进招架恢复生命周期 */
	void UpdateParryRecovery();
	/** 恢复移动与 AI 控制并清理招架状态 */
	void FinishParryRecovery(bool bRestoreControl);

	/** 当前招架恢复阶段正在播放的 Montage */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveParryMontage;
	/** 招架期间暂停的行为树脑组件 */
	TWeakObjectPtr<UBrainComponent> ParryPausedBrain;
	/** 驱动倒地停留和起身检查的 Timer */
	FTimerHandle ParryRecoveryTimer;
	/** 是否存在一条尚未完成的招架恢复生命周期 */
	bool bParryRecovering = false;
	/** 当前招架恢复阶段 */
	EZCBokoblinParryStage ParryStage = EZCBokoblinParryStage::None;
	/** AnimBP 是否应在全身 Slot 前保持倒地姿态 */
	bool bParryDownPose = false;
	/** 招架前的移动模式，用于恢复控制时还原 */
	TEnumAsByte<EMovementMode> PreParryMovementMode = MOVE_Walking;
	/** 招架前的自定义移动模式编号 */
	uint8 PreParryCustomMovementMode = 0;
	/** 当前阶段结束的世界时间，单位为秒 */
	double ParryStageDeadline = 0.0;
};
