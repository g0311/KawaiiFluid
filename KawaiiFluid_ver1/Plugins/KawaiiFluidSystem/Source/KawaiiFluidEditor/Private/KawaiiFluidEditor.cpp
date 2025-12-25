// Copyright Epic Games, Inc. All Rights Reserved.

#include "KawaiiFluidEditor.h"
#include "Style/FluidEditorStyle.h"
#include "AssetTypeActions/AssetTypeActions_FluidPreset.h"

#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FKawaiiFluidEditorModule"

void FKawaiiFluidEditorModule::StartupModule()
{
	// Initialize editor style
	FFluidEditorStyle::Initialize();

	// Register custom asset category
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FluidAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName(TEXT("KawaiiFluid")),
		LOCTEXT("KawaiiFluidAssetCategory", "Kawaii Fluid"));

	// Register asset type actions
	RegisterAssetTypeActions();

	// Register property customizations
	RegisterPropertyCustomizations();
}

void FKawaiiFluidEditorModule::ShutdownModule()
{
	// Unregister property customizations
	UnregisterPropertyCustomizations();

	// Unregister asset type actions
	UnregisterAssetTypeActions();

	// Shutdown editor style
	FFluidEditorStyle::Shutdown();
}

FKawaiiFluidEditorModule& FKawaiiFluidEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FKawaiiFluidEditorModule>("KawaiiFluidEditor");
}

void FKawaiiFluidEditorModule::RegisterAssetTypeActions()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Register Fluid Preset asset type
	TSharedPtr<FAssetTypeActions_FluidPreset> FluidPresetActions = MakeShared<FAssetTypeActions_FluidPreset>();
	AssetTools.RegisterAssetTypeActions(FluidPresetActions.ToSharedRef());
	RegisteredAssetTypeActions.Add(FluidPresetActions);
}

void FKawaiiFluidEditorModule::UnregisterAssetTypeActions()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			if (Action.IsValid())
			{
				AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
			}
		}
	}

	RegisteredAssetTypeActions.Empty();
}

void FKawaiiFluidEditorModule::RegisterPropertyCustomizations()
{
	// Register custom property editors here if needed
	// FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	// PropertyModule.RegisterCustomPropertyTypeLayout(...);
}

void FKawaiiFluidEditorModule::UnregisterPropertyCustomizations()
{
	// Unregister custom property editors here if needed
	// if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	// {
	//     FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	//     PropertyModule.UnregisterCustomPropertyTypeLayout(...);
	// }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKawaiiFluidEditorModule, KawaiiFluidEditor)
