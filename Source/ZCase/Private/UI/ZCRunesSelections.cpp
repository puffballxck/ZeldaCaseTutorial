// 请在项目设置的说明页面填写版权声明


#include "UI/ZCRunesSelections.h"

#include "Characters/ZCCharBase.h"

void UZCRunesSelections::SelectRuneType(TEnumAsByte<ERunes> RuneType)
{
	// 旧控件只负责转发，角色或 Runtime 组件负责校验符文并广播状态
	if (PlayerRef)
	{
		PlayerRef->SelectRune(static_cast<ERunes>(RuneType.GetValue()));
	}
}
