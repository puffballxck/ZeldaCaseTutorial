// 版权所有 Epic Games, Inc，保留所有权利

using UnrealBuildTool;
using System.Collections.Generic;

// 定义游戏构建目标并加载运行时模块
public class ZCaseTarget : TargetRules
{
	// 使用 UE 5.8 包含顺序与 V7 构建设置创建游戏目标
	public ZCaseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ZCase");
	}
}
