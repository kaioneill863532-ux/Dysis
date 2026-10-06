// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Dysis : ModuleRules
{
	public Dysis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// 日落回廊：Source/Dysis 下的子文件夹（Sky/、Player/）按 "Sky/xxx.h" 引用
		PublicIncludePaths.Add(ModuleDirectory);

		// 材质参数集合（DysisMPCComponent/MPCFactory）、LevelSequence（PrismActor 众神对话/PrologueDirector）、Niagara 瀑布水雾（规划中）
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Materials",
			"LevelSequence",
			"MovieScene",
			"UMG",
			"Slate",
			"SlateCore",
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
