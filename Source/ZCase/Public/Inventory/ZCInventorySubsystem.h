// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Inventory/ZCInventoryTypes.h"
#include "ZCInventorySubsystem.generated.h"

class UDataTable;

/**
 * 单人背包状态，固定格数组是唯一可变的
 * 背包表示，UI 只读取它，不维护第二份副本
 */
UCLASS()
class ZCASE_API UZCInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 建立固定格数组并清理旧数据表引用 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** 释放数据表、定义缓存和背包格数组 */
	virtual void Deinitialize() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FZCInventoryChangedSignature);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FZCInventoryChangedSignature OnInventoryChanged;

	/** 原子重建物品定义并重置固定格数组 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool InitializeFromDataTable(UDataTable* InItemDataTable, int32 InCapacity = 6);

	/** 添加数量，先填充已有堆叠再使用空格 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EZCInventoryResult TryAddItem(int32 ItemId, int32 Amount = 1);

	/** 从一个格子中移除指定数量 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EZCInventoryResult TryRemoveItem(int32 SlotIndex, int32 Amount = 1);

	/** 将源格移动到空格，或交换两个已占用格子 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EZCInventoryResult MoveOrSwap(int32 FromSlot, int32 ToSlot);

	/** 按物品编号读取静态定义，失败时不修改输出数据 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool TryGetItemData(int32 ItemId, FZCItemData& OutItemData) const;

	/** 当前背包是否已完成数据表初始化 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInitialized() const { return bInitialized; }

	/** 固定格数量 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCapacity() const { return Capacity; }

	/** 原生只读视图，蓝图可以读取公开的 Slots 属性 */
	const TArray<FZCInventorySlot>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsValidSlot(int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex); }

	/** 在子系统生命周期内保留的只读数据表引用 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FZCInventorySlot> Slots;

	/** 当前背包容量，和 Slots 数组长度保持一致 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Capacity = 6;

private:
	UPROPERTY(Transient)
	TMap<int32, FZCItemData> ItemDefinitions;

	bool bInitialized = false;

	/** 从 ItemDefinitions 中查找物品定义 */
	const FZCItemData* FindItemData(int32 ItemId) const;
};
