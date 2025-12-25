// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KawaiiFluidEditor : ModuleRules
{
	public KawaiiFluidEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"KawaiiFluidRuntime",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"EditorStyle",
				"EditorFramework",
				"ToolMenus",
				"AssetTools",
				"PropertyEditor",
				"AdvancedPreviewScene",
				"EditorWidgets",
				"WorkspaceMenuStructure",
				"LevelEditor",
				"RenderCore",
				"Projects",
				"ApplicationCore",
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
