// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking",
			"Json",
			"JsonUtilities",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"EditorSubsystem",
			"BlueprintGraph",
			"Kismet",
			"KismetCompiler",
			"LevelEditor",
			"AssetTools",
			"AssetRegistry",
			"EditorFramework",
			"Projects",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"EditorStyle",
			"ComponentVisualizers",
			"InputCore",
		});

		// Allow access to low-level engine internals for deep editor integration
		bEnableExceptions = false;
		bUseUnity = false;
	}
}
