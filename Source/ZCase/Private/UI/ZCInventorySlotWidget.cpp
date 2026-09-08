// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ZCInventorySlotWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/ZCInventoryWidget.h"

void UZCInventorySlotWidget::InitializeSlot(UZCInventoryWidget* InOwner, const int32 InSlotIndex,
	const FZCInventorySlot& InSlot, const FZCItemData* InItemData)
{
	OwnerWidget = InOwner;
	SlotIndex = InSlotIndex;
	ItemId = InSlot.IsEmpty() ? INDEX_NONE : InSlot.ItemId;
	Amount = InSlot.IsEmpty() ? 0 : InSlot.Amount;
	ItemIcon = InItemData ? InItemData->Icon : nullptr;
	RefreshVisual();
}

TSharedRef<SWidget> UZCInventorySlotWidget::RebuildWidget()
{
	// 每个格子也必须在生成 Slate 根控件之前创建布局。
	if (!WidgetTree)
	{
		Initialize();
	}
	BuildNativeWidgetTree();
	return Super::RebuildWidget();
}

void UZCInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshVisual();
}

void UZCInventorySlotWidget::BuildNativeWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SlotBorder = WidgetTree->ConstructWidget<UBorder>();
	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
	IconImage = WidgetTree->ConstructWidget<UImage>();
	AmountText = WidgetTree->ConstructWidget<UTextBlock>();

	SizeBox->SetWidthOverride(96.0f);
	SizeBox->SetHeightOverride(96.0f);
	SlotBorder->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.92f));
	SlotBorder->SetPadding(FMargin(5.0f));
	AmountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AmountText->SetJustification(ETextJustify::Right);

	Overlay->AddChildToOverlay(IconImage);
	Overlay->AddChildToOverlay(AmountText);
	SlotBorder->SetContent(Overlay);
	SizeBox->SetContent(SlotBorder);
	WidgetTree->RootWidget = SizeBox;
}

void UZCInventorySlotWidget::RefreshVisual()
{
	if (!IsValid(SlotBorder) || !IsValid(IconImage) || !IsValid(AmountText))
	{
		return;
	}

	const bool bHasItem = ItemId != INDEX_NONE && Amount > 0;
	SlotBorder->SetBrushColor(bHasItem
		? FLinearColor(0.10f, 0.12f, 0.16f, 0.96f)
		: FLinearColor(0.04f, 0.05f, 0.07f, 0.72f));
	IconImage->SetVisibility(bHasItem && IsValid(ItemIcon) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bHasItem && IsValid(ItemIcon))
	{
		IconImage->SetBrushFromTexture(ItemIcon, true);
	}

	AmountText->SetVisibility(bHasItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bHasItem)
	{
		AmountText->SetText(FText::AsNumber(Amount));
	}
}

FReply UZCInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (ItemId == INDEX_NONE || Amount <= 0 || !OwnerWidget.IsValid())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
}

void UZCInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (ItemId == INDEX_NONE || Amount <= 0 || !OwnerWidget.IsValid())
	{
		return;
	}

	UDragDropOperation* DragOperation = NewObject<UDragDropOperation>(this);
	DragOperation->Payload = this;
	DragOperation->Pivot = EDragPivot::CenterCenter;

	if (IsValid(ItemIcon))
	{
		UImage* DragVisual = NewObject<UImage>(DragOperation);
		DragVisual->SetBrushFromTexture(ItemIcon, true);
		DragOperation->DefaultDragVisual = DragVisual;
	}

	OutOperation = DragOperation;
	SetVisibility(ESlateVisibility::Hidden);
}

void UZCInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

	if (OwnerWidget.IsValid())
	{
		OwnerWidget->HandleDragCancelled(this);
	}
	else
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

bool UZCInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (!InOperation || !OwnerWidget.IsValid())
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	UZCInventorySlotWidget* SourceSlot = Cast<UZCInventorySlotWidget>(InOperation->Payload);
	if (!SourceSlot || SourceSlot->OwnerWidget != OwnerWidget)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	OwnerWidget->HandleDrop(SourceSlot->GetSlotIndex(), SlotIndex);
	return true;
}
