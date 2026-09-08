// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Inventory/ZCInventoryTypes.h"
#include "ZCInventorySubsystem.generated.h"

class UDataTable;

/**
 * Single-player inventory state. The fixed slot array is the only mutable
 * inventory representation; UI reads it and never owns a second copy.
 */
UCLASS()
class ZCASE_API UZCInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FZCInventoryChangedSignature);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FZCInventoryChangedSignature OnInventoryChanged;

	/** Rebuilds item definitions and resets the fixed slot array atomically. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool InitializeFromDataTable(UDataTable* InItemDataTable, int32 InCapacity = 6);

	/** Adds an amount, filling existing stacks before empty slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EZCInventoryResult TryAddItem(int32 ItemId, int32 Amount = 1);

	/** Removes an amount from one slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EZCInventoryResult TryRemoveItem(int32 SlotIndex, int32 Amount = 1);

	/** Moves a source slot to an empty slot or swaps two occupied slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EZCInventoryResult MoveOrSwap(int32 FromSlot, int32 ToSlot);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool TryGetItemData(int32 ItemId, FZCItemData& OutItemData) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCapacity() const { return Capacity; }

	/** Native read-only view; Blueprints can read the public Slots property. */
	const TArray<FZCInventorySlot>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsValidSlot(int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex); }

	/** Read-only data table reference retained for the subsystem lifetime. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FZCInventorySlot> Slots;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Capacity = 6;

private:
	UPROPERTY(Transient)
	TMap<int32, FZCItemData> ItemDefinitions;

	bool bInitialized = false;

	const FZCItemData* FindItemData(int32 ItemId) const;
};
