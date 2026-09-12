// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZCInventoryTypes.generated.h"

class UTexture2D;

/** 背包变更结果，非成功结果会保持背包不变 */
UENUM(BlueprintType)
enum class EZCInventoryResult : uint8
{
	Success,
	NotInitialized,
	InvalidItem,
	InvalidAmount,
	InventoryFull,
	InvalidSlot,
	EmptySlot,
	InsufficientAmount,
	NoChange
};

/** 供背包数据表使用的静态物品数据 */
USTRUCT(BlueprintType)
struct FZCItemData : public FTableRowBase
{
	GENERATED_BODY()

	/** 数据表中的稳定物品编号 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 ItemId = INDEX_NONE;

	/** UI 展示的物品名称 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** UI 展示的物品图标 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 单格允许堆叠的最大数量 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 99;
};

/** 一个固定背包格的运行时状态 */
USTRUCT(BlueprintType)
struct FZCInventorySlot
{
	GENERATED_BODY()

	/** 当前格子的物品编号，INDEX_NONE 表示空格 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 ItemId = INDEX_NONE;

	/** 当前格子的堆叠数量 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Amount = 0;

	/** 判断编号或数量是否表示空格 */
	bool IsEmpty() const
	{
		return ItemId == INDEX_NONE || Amount <= 0;
	}

	/** 清空编号和堆叠数量 */
	void Reset()
	{
		ItemId = INDEX_NONE;
		Amount = 0;
	}
};
