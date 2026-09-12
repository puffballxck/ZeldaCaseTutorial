// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZCLayout.generated.h"

class AZCCharBase;
class UWidgetSwitcher;

/**
 * 玩家界面的根布局，视觉组合保留在蓝图中
 * 状态变化通过语义函数进入，而不是直接访问
 * 蓝图 WidgetSwitcher
 */
UCLASS()
class ZCASE_API UZCLayout : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 保留的蓝图初始化扩展点，用于兼容已有资产 */
	UFUNCTION(BlueprintImplementableEvent)
	void ConstructDeferred(AZCCharBase* PlayerRef);

	/** 保留的蓝图体力动画扩展点，用于兼容已有资产 */
	UFUNCTION(BlueprintImplementableEvent)
	void ShowGaugeAnim(bool bShow);

	/**
	 * 在游戏界面（索引 0）和符文选择界面（索引 1）之间切换
	 * 蓝图可以覆盖此函数，原生备用实现会使用
	 * WidgetTree 中找到的第一个 WidgetSwitcher
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ZCase|Player Presentation")
	void SetRuneMenuOpen(bool bOpen);
	virtual void SetRuneMenuOpen_Implementation(bool bOpen);

protected:
	/** 现有 UI_Layout 切换器的首选显式绑定 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher;
	
};
