// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "AIController.h"
#include "Engine/TimerHandle.h"
#include "Perception/AIPerceptionTypes.h"
#include "ZCBokoblinAIController.generated.h"

class AZCBokoblinEnemy;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UZCAttributeComponent;

/**
 * 负责 Bokoblin 的感知与行为树
 *
 * 黑板契约保持最小化
 * 运行时只写入 TargetActor，巡逻任务只写入 PatrolLocation
 */
UCLASS()
class ZCASE_API AZCBokoblinAIController : public AAIController
{
	GENERATED_BODY()

public:
	AZCBokoblinAIController();

	/** 接管敌人时记录出生点、启动感知并运行行为树 */
	virtual void OnPossess(APawn* InPawn) override;
	/** 解除控制时解绑目标生命周期并停止行为树 */
	virtual void OnUnPossess() override;
	/** 结束游戏或销毁控制器时清理感知、Timer 和目标引用 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "ZCase|Bokoblin|AI")
	bool IsReturningHome() const { return bReturningHome; }

	/** 感知回调，获取或释放存活的玩家控制 Pawn */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** 供控制器 UAIPerceptionComponent 使用的感知参数 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (ClampMin = "0.0"))
	float SightRadius = 1800.0f;

	/** 超出该半径后仍可保留目标的感知距离，单位为厘米 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 2200.0f;

	/** 感知组件的视野半角，单位为角度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PeripheralVisionAngleDegrees = 70.0f;

private:
	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor);

	UFUNCTION()
	void HandleTargetDeath(AActor* DeadActor);

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	bool IsLivingPlayerTarget(const AActor* Actor) const;
	void BindTargetLifecycle(AActor* Target);
	void UnbindTargetLifecycle();
	void HandleTargetAcquired(AActor* Target);
	void ClearTarget();
	void SetPatrolMovement();
	void SetChaseMovement();
	void StopAI();
	void UpdateHomeReturn();
	void BeginHomeReturn();
	void FinishHomeReturn();
	void AcquireVisiblePlayer();
	bool HasReachedHome() const;

	/** 每次接管时记录起点，返程和重新索敌不会更新它 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI", meta = (AllowPrivateAccess = "true"))
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI", meta = (AllowPrivateAccess = "true"))
	bool bReturningHome = false;

	FTimerHandle HomeReturnTimer;
	/** 下一次返程检查允许执行的绝对时间，避免 Timer 高频重入 */
	double NextReturnAttemptTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	TWeakObjectPtr<AZCBokoblinEnemy> ControlledEnemy;
	/** 当前感知到且仍通过生命与控制权校验的目标 */
	TWeakObjectPtr<AActor> CurrentTarget;
	/** 当前目标的属性组件，用于监听死亡并立即清除锁定 */
	TWeakObjectPtr<UZCAttributeComponent> CurrentTargetAttributes;

	/** 死亡或解除 Possess 停止控制器后，后续回调不得重新启动行为树 */
	bool bPermanentlyStopped = false;
	/** 当前是否已停止行为树但仍保留控制器对象 */
	bool bAIStopped = false;
};
