// 版权所有 Epic Games, Inc，保留所有权利

using UnrealBuildTool;

// 配置编辑器资产工具依赖，仅供编辑器目标使用
public class ZCaseEditor : ModuleRules
{
	// 声明资产、动画图表与行为树编辑工具所需的私有依赖
	public ZCaseEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"ContentBrowser",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd",
			"ZCase", "AIModule", "NavigationSystem", "BehaviorTreeEditor", "AIGraph",
			"AnimGraph", "AnimGraphRuntime", "BlueprintGraph", "BlueprintEditorLibrary", "AnimationBlueprintLibrary"
		});
	}
}
