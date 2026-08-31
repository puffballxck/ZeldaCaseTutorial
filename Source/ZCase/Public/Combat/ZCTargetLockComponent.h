// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ZCTargetLockComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCTargetChangedSignature,
	AActor*, PreviousTarget,
	AActor*, CurrentTarget);

/** 持有当前锁定目标，并保证目标在整个锁定生命周期内仍可被锁定。 */
UCLASS(ClassGroup = (ZCase), meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCTargetLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCTargetLockComponent();

	/** 校验候选对象并替换当前目标；同一目标重复设置视为成功但不会重复广播。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	bool SetTarget(AActor* Candidate);

	/**
	 * 从视野中心获取最合适的可锁定目标，并替换当前目标。
	 *
	 * 候选对象必须实现 IZCTargetable、当前允许锁定，并且位于获取半径和
	 * 屏幕中心角度内且没有被 ECC_Visibility 几何体遮挡。评分以视线角度
	 * 为主、距离为辅；找不到候选时保持当前目标不变，由生命周期校验负责
	 * 清除死亡、超距或持续遮挡的目标。
	 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	bool AcquireBestTarget(const FVector& ViewLocation, const FVector& ViewForward);

	/** 清除当前目标并关闭仅用于有效锁定目标的 Tick。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	void ClearTarget();

	/** 返回当前锁定目标；目标失效时返回空指针。 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Target Lock")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** 当前是否持有有效目标。 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Target Lock")
	bool HasTarget() const { return CurrentTarget.IsValid(); }

	/** 目标替换、清除或失效时广播，参数依次为旧目标和新目标。 */
	UPROPERTY(BlueprintAssignable, Category = "ZCase|Target Lock")
	FZCTargetChangedSignature OnTargetChanged;

	/** 获取目标的最大距离（厘米）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float AcquisitionRadius = 2500.0f;

	/** 获取目标允许偏离屏幕中心的半角（角度）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AcquisitionHalfAngle = 50.0f;

	/** 当前目标超过该距离（厘米）时解除锁定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float LockLostDistance = 3000.0f;

	/** 目标被遮挡后允许持续的宽限时间（秒）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float OcclusionGracePeriod = 0.75f;

	/** 获取评分中角度项的权重。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float AngleWeight = 0.8f;

	/** 获取评分中距离项的权重。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float DistanceWeight = 0.2f;

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FZCTargetLockLossConditionsTest;
#endif

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	bool IsValidTarget(const AActor* Candidate) const;
	bool IsTargetVisible(const AActor* Candidate, const FVector& ViewLocation) const;
	void ReplaceTarget(AActor* NewTarget);

	TWeakObjectPtr<AActor> CurrentTarget;
	float OccludedDuration = 0.0f;
};
