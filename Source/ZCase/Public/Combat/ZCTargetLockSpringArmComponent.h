// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/SpringArmComponent.h"
#include "ZCTargetLockSpringArmComponent.generated.h"

/** 在实际碰撞缩臂之后约束锁定构图；安全区域内完全保留玩家视角。 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCTargetLockSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float ScreenMargin = 0.12f;

	/** 近身构图无法容纳目标时，允许额外抬高相机的最大高度（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.0"))
	float MaxFramingHeight = 300.0f;

	/** 为头顶指示箭头预留的世界空间高度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Lock", meta = (ClampMin = "0.0"))
	float IndicatorHeight = 80.0f;

protected:
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime) override;

private:
	bool bLockFramingActive = false;
	FVector UnlockedTargetOffset = FVector::ZeroVector;
};
