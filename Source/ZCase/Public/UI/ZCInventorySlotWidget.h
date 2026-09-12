// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/ZCInventoryTypes.h"
#include "ZCInventorySlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UZCInventoryWidget;
class UDragDropOperation;

/** 一个原生备用背包格，蓝图子类可以替换其可视控件树 */
UCLASS()
class ZCASE_API UZCInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 写入格子索引和物品快照，随后刷新原生或蓝图视觉 */
	void InitializeSlot(UZCInventoryWidget* InOwner, int32 InSlotIndex, const FZCInventorySlot& InSlot,
		const FZCItemData* InItemData);

	/** 返回拖放操作对应的背包格索引 */
	int32 GetSlotIndex() const { return SlotIndex; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

private:
	/** 在 RebuildWidget 阶段创建没有蓝图控件树时的备用布局 */
	void BuildNativeWidgetTree();
	/** 根据当前物品快照更新图标、数量和边框 */
	void RefreshVisual();

	/** 接收拖放回调的背包面板 */
	TWeakObjectPtr<UZCInventoryWidget> OwnerWidget;
	/** 该控件对应的固定格索引 */
	int32 SlotIndex = INDEX_NONE;
	/** 当前显示的物品编号 */
	int32 ItemId = INDEX_NONE;
	/** 当前显示的物品数量 */
	int32 Amount = 0;
	/** 当前物品图标，空格时为空 */
	TObjectPtr<UTexture2D> ItemIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmountText;
};
