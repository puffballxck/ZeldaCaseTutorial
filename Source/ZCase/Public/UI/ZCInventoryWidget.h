// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZCInventoryWidget.generated.h"

class UUniformGridPanel;
class UZCInventorySlotWidget;
class UZCInventorySubsystem;

/** 原生备用背包面板，蓝图子类可以提供最终美术与布局 */
UCLASS()
class ZCASE_API UZCInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 建立原生布局并绑定背包子系统 */
	virtual void NativeConstruct() override;

	/** 从唯一权威的背包子系统刷新所有格子 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshInventory();

	/** 设置唯一权威的背包数据源 */
	void SetInventorySubsystem(UZCInventorySubsystem* InSubsystem);

	/** 返回面板按列数和格子尺寸计算出的期望大小 */
	FVector2D GetDesiredInventorySize() const;

	int32 GetColumnCount() const { return FMath::Max(1, ColumnCount); }

	/** 格子控件完成拖放后调用，用于处理目标格子变更 */
	/** 处理格子拖放后的移动或交换请求 */
	void HandleDrop(int32 FromSlot, int32 ToSlot);
	/** 拖放被取消时恢复源格的视觉 */
	void HandleDragCancelled(UZCInventorySlotWidget* DraggedSlot);

protected:
	/** 提供蓝图未配置 Grid 时的原生面板构建入口 */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** 每行使用的格子数量 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 ColumnCount = 3;

	/** 单个格子的正方形尺寸，单位为 Slate 像素 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "48.0"))
	float SlotSize = 96.0f;

	/** 相邻格子之间的间距，单位为 Slate 像素 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0.0"))
	float SlotPadding = 4.0f;

	/** 蓝图可选的网格容器 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> Grid;

	/** 当前已创建的格子控件集合 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UZCInventorySlotWidget>> SlotWidgets;

	/** 唯一权威的背包运行时子系统 */
	UPROPERTY(Transient)
	TObjectPtr<UZCInventorySubsystem> InventorySubsystem;

private:
	void BuildNativeWidgetTree();
};
