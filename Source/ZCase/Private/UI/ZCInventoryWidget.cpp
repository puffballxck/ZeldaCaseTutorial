// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ZCInventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/UniformGridPanel.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/ZCInventorySlotWidget.h"
#include "Inventory/ZCInventorySubsystem.h"

TSharedRef<SWidget> UZCInventoryWidget::RebuildWidget()
{
	// Slate 获取根控件前建立原生布局，NativeConstruct 此时已经太晚。
	if (!WidgetTree)
	{
		Initialize();
	}
	BuildNativeWidgetTree();
	return Super::RebuildWidget();
}

void UZCInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(InventorySubsystem) && GetGameInstance())
	{
		InventorySubsystem = GetGameInstance()->GetSubsystem<UZCInventorySubsystem>();
	}

	RefreshInventory();
}

void UZCInventoryWidget::SetInventorySubsystem(UZCInventorySubsystem* InSubsystem)
{
	InventorySubsystem = InSubsystem;
	RefreshInventory();
}

void UZCInventoryWidget::BuildNativeWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();

	Background->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.03f, 0.90f));
	Background->SetPadding(FMargin(8.0f));
	Grid->SetSlotPadding(FMargin(SlotPadding));
	Background->SetContent(Grid);
	SizeBox->SetContent(Background);
	WidgetTree->RootWidget = SizeBox;
}

void UZCInventoryWidget::RefreshInventory()
{
	if (!IsValid(Grid))
	{
		return;
	}

	Grid->ClearChildren();
	SlotWidgets.Reset();

	if (!IsValid(InventorySubsystem) || !InventorySubsystem->IsInitialized())
	{
		return;
	}

	const TArray<FZCInventorySlot>& Slots = InventorySubsystem->GetSlots();
	SlotWidgets.Reserve(Slots.Num());

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const FZCInventorySlot& InventorySlot = Slots[SlotIndex];
		FZCItemData ItemData;
		const FZCItemData* ItemDataPtr = nullptr;
		if (!InventorySlot.IsEmpty() && InventorySubsystem->TryGetItemData(InventorySlot.ItemId, ItemData))
		{
			ItemDataPtr = &ItemData;
		}

		UZCInventorySlotWidget* SlotWidget = nullptr;
		if (APlayerController* OwningPlayer = GetOwningPlayer())
		{
			SlotWidget = CreateWidget<UZCInventorySlotWidget>(
				OwningPlayer, UZCInventorySlotWidget::StaticClass());
		}
		else if (UWorld* World = GetWorld())
		{
			SlotWidget = CreateWidget<UZCInventorySlotWidget>(
				World, UZCInventorySlotWidget::StaticClass());
		}
		if (!IsValid(SlotWidget))
		{
			continue;
		}

		SlotWidget->InitializeSlot(this, SlotIndex, InventorySlot, ItemDataPtr);
		SlotWidgets.Add(SlotWidget);
		const int32 Row = SlotIndex / GetColumnCount();
		const int32 Column = SlotIndex % GetColumnCount();
		Grid->AddChildToUniformGrid(SlotWidget, Row, Column);
	}
}

FVector2D UZCInventoryWidget::GetDesiredInventorySize() const
{
	const int32 RowCount = IsValid(InventorySubsystem) && InventorySubsystem->IsInitialized()
		? FMath::DivideAndRoundUp(InventorySubsystem->GetCapacity(), GetColumnCount())
		: 2;
	return FVector2D(
		GetColumnCount() * SlotSize + (GetColumnCount() + 1) * SlotPadding + 16.0f,
		RowCount * SlotSize + (RowCount + 1) * SlotPadding + 16.0f);
}

void UZCInventoryWidget::HandleDrop(const int32 FromSlot, const int32 ToSlot)
{
	if (IsValid(InventorySubsystem))
	{
		InventorySubsystem->MoveOrSwap(FromSlot, ToSlot);
	}

	RefreshInventory();
}

void UZCInventoryWidget::HandleDragCancelled(UZCInventorySlotWidget* DraggedSlot)
{
	if (IsValid(DraggedSlot))
	{
		DraggedSlot->SetVisibility(ESlateVisibility::Visible);
	}

	RefreshInventory();
}
