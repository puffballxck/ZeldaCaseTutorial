// 请在项目设置的说明页面填写版权声明


#include "UI/ZCLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"

void UZCLayout::SetRuneMenuOpen_Implementation(const bool bOpen)
{
	// 优先使用显式 BindWidget，保留对旧 WidgetTree 的兼容回退
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

		// 找到第一个切换器后立即返回，避免误操作多个互不相关的菜单
		Switcher->SetActiveWidgetIndex(bOpen ? 1 : 0);
		return;
	}
}
