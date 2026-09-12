// 版权所有 Epic Games, Inc，保留所有权利

using UnrealBuildTool;

// 配置游戏模块的输入、界面、人工智能与导航依赖
public class ZCase : ModuleRules
{
	// 使用显式或共享预编译头，并声明运行时所需依赖
	public ZCase(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore","EnhancedInput","UMG", "AIModule", "NavigationSystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "FieldSystemEngine", "Slate", "SlateCore" });

		// Slate 界面依赖已在上方启用，下面保留配置示例
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// 使用在线功能时启用以下依赖
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// 使用 OnlineSubsystemSteam 时还需在 uproject 的插件列表中启用该插件
	}
}
