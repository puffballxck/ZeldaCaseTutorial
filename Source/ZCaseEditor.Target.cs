// 版权所有 Epic Games, Inc，保留所有权利

using UnrealBuildTool;
using System.Collections.Generic;

// 定义编辑器构建目标并同时加载游戏与编辑器模块
public class ZCaseEditorTarget : TargetRules
{
	// 使用 UE 5.8 构建设置并注册编辑器需要的两个模块
	public ZCaseEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "ZCase", "ZCaseEditor" });
	}
}
