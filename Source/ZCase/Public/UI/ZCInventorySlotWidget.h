// Fill out your copyright notice in the Description page of Project Settings.

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

/** One native fallback inventory slot. A Blueprint subclass may replace its visual tree. */
UCLASS()
class ZCASE_API UZCInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSlot(UZCInventoryWidget* InOwner, int32 InSlotIndex, const FZCInventorySlot& InSlot,
		const FZCItemData* InItemData);

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
	void BuildNativeWidgetTree();
	void RefreshVisual();

	TWeakObjectPtr<UZCInventoryWidget> OwnerWidget;
	int32 SlotIndex = INDEX_NONE;
	int32 ItemId = INDEX_NONE;
	int32 Amount = 0;
	TObjectPtr<UTexture2D> ItemIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmountText;
};
