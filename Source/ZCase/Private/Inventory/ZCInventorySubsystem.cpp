// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/ZCInventorySubsystem.h"

#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogZCInventory, Log, All);

void UZCInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

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
