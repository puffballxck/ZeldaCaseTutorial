// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "ZCTargetLockIndicatorWidget.generated.h"

/**
 * 显示本地玩家当前的目标锁定锚点，但不持有
 * 游戏玩法锁定状态
 */
UCLASS(meta = (DisableNativeTick))
class ZCASE_API UZCTargetLockIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 设置要显示目标锁定锚点的 Actor */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	void SetTarget(AActor* NewTarget);

	/** 隐藏指示器并释放弱目标引用 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	void ClearTarget();

protected:
	/** 建立控件布局并关闭无目标时的 Tick */
	virtual void NativeConstruct() override;
	/** 仅在目标有效时更新屏幕投影位置 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 箭头在目标锁定锚点上方的世界空间高度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float WorldHeightOffset = 60.0f;

private:
	/** 校验目标仍存在并实现锁定锚点接口 */
	bool IsValidIndicatorTarget(const AActor* Candidate) const;
	/** 投影目标锚点并更新箭头位置 */
	bool UpdateIndicatorPosition();
	/** 复用蓝图尺寸并建立可见控件的视口 Slot */
	void InitializeViewportLayout();
	/** 同步 UUserWidget 的 Tick 开关与目标生命周期 */
	void SetIndicatorTickEnabled(bool bEnabled);

	/** 当前展示的目标，仅用于 UI 投影，不拥有锁定状态 */
	TWeakObjectPtr<AActor> Target;
	/** 是否已开启目标存在期间的 Tick */
	bool bIndicatorTickEnabled = false;
};
