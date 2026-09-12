// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "Animation/AnimInstance.h"
#include "ZCBokoblinAnimInstance.generated.h"

/** 供 Bokoblin AnimBP BlendSpace 使用的最小移动数据 */
UCLASS()
class ZCASE_API UZCBokoblinAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 同步 Bokoblin 移动与倒地姿态给 AnimBP */
	virtual void NativeUpdateAnimation(float DeltaTime) override;

	/** 当前水平移动速度，单位为厘米每秒 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Bokoblin|Animation")
	float Speed = 0.0f;

	/** 在全身 Slot 前选择循环 Down_Wait；起身淡出前切回移动基础姿态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Bokoblin|Animation")
	bool bParryDownPose = false;
};
