// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/ZCLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"

void UZCLayout::SetRuneMenuOpen_Implementation(const bool bOpen)
{
	if (IsValid(WidgetSwitcher))
	{
		WidgetSwitcher->SetActiveWidgetIndex(bOpen ? 1 : 0);
		return;
	}

	if (!WidgetTree)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);

	for (UWidget* Widget : Widgets)
	{
		UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget);
		if (!Switcher)
		{
			continue;
		}

		Switcher->SetActiveWidgetIndex(bOpen ? 1 : 0);
		return;
	}
}
