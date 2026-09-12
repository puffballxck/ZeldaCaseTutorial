// 请在项目设置的说明页面填写版权声明

#pragma once

namespace Debug
{
	/** 在屏幕上显示青色调试信息，且仅在引擎实例可用时执行 */
	static void Print(const FString& DebugMessage)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMessage);
		}
	}
}
