// Copyright Epic Games, Inc. All Rights Reserved.

#include "KawaiiFluidRuntime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FKawaiiFluidRuntimeModule"

void FKawaiiFluidRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	/*
	* 쉐이더 경로 매핑
	* 
	* "/Plugin/KawaiiFluidSystem" 으로 경로 매핑
	* 다른 쉐이더 파일에서 #include "/Plugin/KawaiiFluidSystem/YourShaderFile.usf" 와 같이 사용 가능
	*/
	FString PluginShaderPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("KawaiiFluidSystem"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/KawaiiFluidSystem"), PluginShaderPath);
}

void FKawaiiFluidRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FKawaiiFluidRuntimeModule, KawaiiFluidRuntime)