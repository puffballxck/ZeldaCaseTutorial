// 请在项目设置的说明页面填写版权声明

#include "Inventory/ZCInventorySubsystem.h"

#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogZCInventory, Log, All);

void UZCInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 子系统创建时只建立固定容量的空格，数据表必须经过显式校验后才生效
	Capacity = 6;
	Slots.Init(FZCInventorySlot{}, Capacity);
	bInitialized = false;
}

void UZCInventorySubsystem::Deinitialize()
{
	OnInventoryChanged.Clear();
	Slots.Reset();
	ItemDefinitions.Reset();
	ItemDataTable = nullptr;
	bInitialized = false;

	Super::Deinitialize();
}

bool UZCInventorySubsystem::InitializeFromDataTable(UDataTable* InItemDataTable, const int32 InCapacity)
{
	// 先在局部容器中完整校验，任何失败都不能破坏当前可用背包
	if (!IsValid(InItemDataTable))
	{
		UE_LOG(LogZCInventory, Error, TEXT("InitializeFromDataTable failed: item data table is null."));
		return false;
	}

	if (InCapacity <= 0)
	{
		UE_LOG(LogZCInventory, Error, TEXT("InitializeFromDataTable failed: capacity must be positive (got %d)."), InCapacity);
		return false;
	}

	if (InItemDataTable->GetRowStruct() != FZCItemData::StaticStruct())
	{
		UE_LOG(LogZCInventory, Error, TEXT("InitializeFromDataTable failed: table '%s' uses '%s', expected FZCItemData."),
			*InItemDataTable->GetPathName(),
			InItemDataTable->GetRowStruct() ? *InItemDataTable->GetRowStruct()->GetPathName() : TEXT("<null>"));
		return false;
	}

	TMap<int32, FZCItemData> NewDefinitions;
	TArray<FZCItemData*> Rows;
	InItemDataTable->GetAllRows<FZCItemData>(TEXT("ZCInventoryInitialization"), Rows);

	if (Rows.Num() == 0)
	{
		UE_LOG(LogZCInventory, Error, TEXT("InitializeFromDataTable failed: table '%s' has no FZCItemData rows."),
			*InItemDataTable->GetPathName());
		return false;
	}

	for (const FZCItemData* Row : Rows)
	{
		if (!Row || Row->ItemId == INDEX_NONE || Row->ItemId < 0)
		{
			UE_LOG(LogZCInventory, Error, TEXT("InitializeFromDataTable failed: row has an invalid ItemId."));
			return false;
		}

		if (Row->MaxStackSize <= 0)
		{
			UE_LOG(LogZCInventory, Error, TEXT("InitializeFromDataTable failed: item %d has invalid MaxStackSize %d."),
				Row->ItemId, Row->MaxStackSize);
			return false;
		}

		if (NewDefinitions.Contains(Row->ItemId))
		{
			UE_LOG(LogZCInventory, Error, TEXT("InitializeFromDataTable failed: duplicate ItemId %d."), Row->ItemId);
			return false;
		}

		NewDefinitions.Add(Row->ItemId, *Row);
	}

	// 所有行通过校验后才一次性替换定义、容量和固定格数组
	ItemDataTable = InItemDataTable;
	ItemDefinitions = MoveTemp(NewDefinitions);
	Capacity = InCapacity;
	Slots.Init(FZCInventorySlot{}, Capacity);
	bInitialized = true;
	OnInventoryChanged.Broadcast();

	return true;
}

EZCInventoryResult UZCInventorySubsystem::TryAddItem(const int32 ItemId, const int32 Amount)
{
	if (!bInitialized)
	{
		return EZCInventoryResult::NotInitialized;
	}

	if (Amount <= 0)
	{
		return EZCInventoryResult::InvalidAmount;
	}

	const FZCItemData* ItemData = FindItemData(ItemId);
	if (!ItemData)
	{
		return EZCInventoryResult::InvalidItem;
	}

	const int32 MaxStackSize = FMath::Max(1, ItemData->MaxStackSize);
	// 使用工作副本先完成堆叠与空格分配，容量不足时保持原数组不变
	TArray<FZCInventorySlot> WorkingSlots = Slots;
	int32 Remaining = Amount;

	for (FZCInventorySlot& Slot : WorkingSlots)
	{
		if (Slot.ItemId != ItemId || Slot.Amount >= MaxStackSize)
		{
			continue;
		}

		const int32 Added = FMath::Min(Remaining, MaxStackSize - Slot.Amount);
		Slot.Amount += Added;
		Remaining -= Added;
		if (Remaining == 0)
		{
			break;
		}
	}

	for (FZCInventorySlot& Slot : WorkingSlots)
	{
		if (!Slot.IsEmpty())
		{
			continue;
		}

		const int32 Added = FMath::Min(Remaining, MaxStackSize);
		Slot.ItemId = ItemId;
		Slot.Amount = Added;
		Remaining -= Added;
		if (Remaining == 0)
		{
			break;
		}
	}

	if (Remaining > 0)
	{
		return EZCInventoryResult::InventoryFull;
	}

	// 只有全部数量成功放入后才提交副本并广播一次变更
	Slots = MoveTemp(WorkingSlots);
	OnInventoryChanged.Broadcast();
	return EZCInventoryResult::Success;
}

EZCInventoryResult UZCInventorySubsystem::TryRemoveItem(const int32 SlotIndex, const int32 Amount)
{
	if (!bInitialized)
	{
		return EZCInventoryResult::NotInitialized;
	}

	if (Amount <= 0)
	{
		return EZCInventoryResult::InvalidAmount;
	}

	if (!Slots.IsValidIndex(SlotIndex))
	{
		return EZCInventoryResult::InvalidSlot;
	}

	const FZCInventorySlot& Slot = Slots[SlotIndex];
	if (Slot.IsEmpty())
	{
		return EZCInventoryResult::EmptySlot;
	}

	if (Amount > Slot.Amount)
	{
		return EZCInventoryResult::InsufficientAmount;
	}

	FZCInventorySlot& MutableSlot = Slots[SlotIndex];
	MutableSlot.Amount -= Amount;
	if (MutableSlot.Amount == 0)
	{
		// 数量归零时同时清除编号，恢复固定格的不变量
		MutableSlot.Reset();
	}

	OnInventoryChanged.Broadcast();
	return EZCInventoryResult::Success;
}

EZCInventoryResult UZCInventorySubsystem::MoveOrSwap(const int32 FromSlot, const int32 ToSlot)
{
	if (!bInitialized)
	{
		return EZCInventoryResult::NotInitialized;
	}

	if (!Slots.IsValidIndex(FromSlot) || !Slots.IsValidIndex(ToSlot))
	{
		return EZCInventoryResult::InvalidSlot;
	}

	if (FromSlot == ToSlot)
	{
		return EZCInventoryResult::NoChange;
	}

	if (Slots[FromSlot].IsEmpty())
	{
		return EZCInventoryResult::EmptySlot;
	}

	FZCInventorySlot& Source = Slots[FromSlot];
	FZCInventorySlot& Destination = Slots[ToSlot];
	// 目标格为空时表现为移动，目标格有物品时表现为交换
	Swap(Source, Destination);

	OnInventoryChanged.Broadcast();
	return EZCInventoryResult::Success;
}

bool UZCInventorySubsystem::TryGetItemData(const int32 ItemId, FZCItemData& OutItemData) const
{
	const FZCItemData* ItemData = FindItemData(ItemId);
	if (!ItemData)
	{
		return false;
	}

	OutItemData = *ItemData;
	return true;
}

const FZCItemData* UZCInventorySubsystem::FindItemData(const int32 ItemId) const
{
	return ItemDefinitions.Find(ItemId);
}
