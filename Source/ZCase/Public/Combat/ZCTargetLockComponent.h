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

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	bool IsValidTarget(const AActor* Candidate) const;
	void ReplaceTarget(AActor* NewTarget);

	TWeakObjectPtr<AActor> CurrentTarget;
};
