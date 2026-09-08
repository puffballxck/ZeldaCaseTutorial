// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZCInventoryTypes.generated.h"

class UTexture2D;

/** Result of an inventory mutation. A non-success result leaves the inventory unchanged. */
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

/** Static item data used by the inventory data table. */
USTRUCT(BlueprintType)
struct FZCItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 ItemId = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 99;
};

/** Runtime state for one fixed inventory slot. */
USTRUCT(BlueprintType)
struct FZCInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 ItemId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Amount = 0;

	bool IsEmpty() const
	{
		return ItemId == INDEX_NONE || Amount <= 0;
	}

	void Reset()
	{
		ItemId = INDEX_NONE;
		Amount = 0;
	}
};
