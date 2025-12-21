// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class KawaiiFluidRuntime : ModuleRules
{
	public KawaiiFluidRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string ModulePath = ModuleDirectory;

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModulePath, "Public"),
				Path.Combine(ModulePath, "Public/Core"),
				Path.Combine(ModulePath, "Public/Physics"),
				Path.Combine(ModulePath, "Public/Collision"),
				Path.Combine(ModulePath, "Public/Components")
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModulePath, "Private"),
				Path.Combine(ModulePath, "Private/Core"),
				Path.Combine(ModulePath, "Private/Physics"),
				Path.Combine(ModulePath, "Private/Collision"),
				Path.Combine(ModulePath, "Private/Components")
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
			}
		);
	}
}
