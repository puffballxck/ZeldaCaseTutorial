// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ZCAnimNotifyState_WeaponTrace.generated.h"

/**
 * 由攻击动画定义命中窗口；Begin/End 只负责把动画时序转交给战斗组件
 * Sweep、伤害和去重均由 UZCCombatComponent 负责，避免 NotifyState 持有战斗状态
 */
UCLASS(meta = (DisplayName = "ZC Weapon Trace"))
class ZCASE_API UZCAnimNotifyState_WeaponTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 打开本次攻击的命中窗口并建立 Trace 基线 */
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	/** 关闭命中窗口，覆盖动画结束和被打断两条路径 */
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 返回编辑器中的 NotifyState 名称 */
	virtual FString GetNotifyName_Implementation() const override;
};
