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

		// 材质参数集合（DysisMPCComponent/MPCFactory）、LevelSequence（PrismActor 众神对话/PrologueDirector）
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"LevelSequence",
			"MovieScene",
			"Slate",       // HUD 的 FSlateFontInfo/FCanvasTextItem（思源宋体 FontFace 画字）
			"SlateCore",
		});
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
