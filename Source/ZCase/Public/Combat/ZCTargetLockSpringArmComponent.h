// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "GameFramework/SpringArmComponent.h"
#include "ZCTargetLockSpringArmComponent.generated.h"

/** 碰撞缩臂后平滑约束双方战斗锚点，允许近身目标身体出框 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCTargetLockSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float ScreenMargin = 0.12f;

	/** 自动角度纠正的插值速度，不影响鼠标输入本身 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.1"))
	float FramingInterpSpeed = 6.0f;

	/** 自动纠正每轴的最大角速度（度/秒），防止目标越过镜头时瞬转 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "1.0"))
	float MaxFramingTurnRate = 90.0f;

	/** 额外构图高度的升降与解锁回落速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.1"))
	float HeightInterpSpeed = 4.0f;

	/** 无法同时容纳双方战斗锚点时，允许额外抬高的最大高度（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.0"))
	float MaxFramingHeight = 300.0f;

	/** 兼容已有资产的旧参数；柔和构图不再根据箭头高度纠正镜头 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.0"))
	float IndicatorHeight = 80.0f;

protected:
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime) override;

private:
	bool bLockFramingActive = false;
	FVector UnlockedTargetOffset = FVector::ZeroVector;
};
