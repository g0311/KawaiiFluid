// Copyright KawaiiFluid Team. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class KawaiiFluidNiagara : ModuleRules
{
	public KawaiiFluidNiagara(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string ModulePath = ModuleDirectory;

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModulePath, "Public"),
				Path.Combine(ModulePath, "Public/NiagaraDI")
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModulePath, "Private"),
				Path.Combine(ModulePath, "Private/NiagaraDI")
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Niagara",
				"NiagaraCore",
				"NiagaraShader",
				"RenderCore",
				"RHI",
				"KawaiiFluidRuntime" // 기존 Runtime 모듈 참조
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"Renderer"
			}
		);
	}
}
