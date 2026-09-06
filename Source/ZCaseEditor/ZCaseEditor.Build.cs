// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZCaseEditor : ModuleRules
{
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
