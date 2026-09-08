// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZCInventoryWidget.generated.h"

class UUniformGridPanel;
class UZCInventorySlotWidget;
class UZCInventorySubsystem;

/** Native fallback inventory panel; a Blueprint subclass may provide final art/layout. */
UCLASS()
class ZCASE_API UZCInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** Refreshes all slots from the single authoritative inventory subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshInventory();

	void SetInventorySubsystem(UZCInventorySubsystem* InSubsystem);

	FVector2D GetDesiredInventorySize() const;

	int32 GetColumnCount() const { return FMath::Max(1, ColumnCount); }

	/** Called by the slot widgets after a drag is dropped on a slot. */
	void HandleDrop(int32 FromSlot, int32 ToSlot);
	void HandleDragCancelled(UZCInventorySlotWidget* DraggedSlot);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 ColumnCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "48.0"))
	float SlotSize = 96.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0.0"))
	float SlotPadding = 4.0f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> Grid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZCInventorySlotWidget>> SlotWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UZCInventorySubsystem> InventorySubsystem;

private:
	void BuildNativeWidgetTree();
};
