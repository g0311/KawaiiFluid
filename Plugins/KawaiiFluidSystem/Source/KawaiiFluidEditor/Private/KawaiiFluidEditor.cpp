// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "KawaiiFluidEditor.h"
#include "Style/KawaiiFluidEditorStyle.h"
#include "AssetTypeActions/AssetTypeActions_KawaiiFluidPreset.h"
#include "Brush/KawaiiFluidBrushEditorMode.h"
#include "Details/KawaiiFluidVolumeComponentDetails.h"
#include "Components/KawaiiFluidVolumeComponent.h"

#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"
#include "EditorModeRegistry.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Thumbnail/KawaiiFluidPresetThumbnailRenderer.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Editor.h"
#include "ObjectTools.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "FKawaiiFluidEditorModule"

/**
 * @brief Initializes the editor module, registers styles, asset tools, and editor modes.
 */
void FKawaiiFluidEditorModule::StartupModule()
{
	// Initialize editor style
	FKawaiiFluidEditorStyle::Initialize();

	// Register custom asset category
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FluidAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName(TEXT("KawaiiFluid")),
		LOCTEXT("KawaiiFluidAssetCategory", "Kawaii Fluid"));

	// Register asset type actions
	RegisterAssetTypeActions();

	// Register property customizations
	RegisterPropertyCustomizations();

	// Register Fluid Brush Editor Mode
	FEditorModeRegistry::Get().RegisterMode<FKawaiiFluidBrushEditorMode>(
		FKawaiiFluidBrushEditorMode::EM_FluidBrush,
		LOCTEXT("FluidBrushModeName", "Fluid Brush"),
		FSlateIcon(),
		false  // Do not show in toolbar
	);

	// Register custom thumbnail renderer
	UThumbnailManager::Get().RegisterCustomRenderer(
		UKawaiiFluidPresetDataAsset::StaticClass(),
		UKawaiiFluidPresetThumbnailRenderer::StaticClass());

	// Bind event for automatic thumbnail refresh on asset save
	UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FKawaiiFluidEditorModule::HandleAssetPreSave);
}

/**
 * @brief Cleans up registered tools, styles, and delegates on module shutdown.
 */
void FKawaiiFluidEditorModule::ShutdownModule()
{
	// Unbind event
	UPackage::PreSavePackageWithContextEvent.RemoveAll(this);

	if (!GExitPurge && !IsEngineExitRequested() && UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UKawaiiFluidPresetDataAsset::StaticClass());
	}
	
	// Unregister Fluid Brush Editor Mode
	FEditorModeRegistry::Get().UnregisterMode(FKawaiiFluidBrushEditorMode::EM_FluidBrush);

	// Unregister property customizations
	UnregisterPropertyCustomizations();

	// Unregister asset type actions
	UnregisterAssetTypeActions();

	// Shutdown editor style
	FKawaiiFluidEditorStyle::Shutdown();
}

/**
 * @brief Static getter for the module instance.
 * @return Reference to the editor module instance
 */
FKawaiiFluidEditorModule& FKawaiiFluidEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FKawaiiFluidEditorModule>("KawaiiFluidEditor");
}

/**
 * @brief Handles automatic thumbnail generation when a fluid preset asset is about to be saved.
 * @param InPackage The package being saved
 * @param InContext Pre-save context information
 */
void FKawaiiFluidEditorModule::HandleAssetPreSave(UPackage* InPackage, FObjectPreSaveContext InContext)
{
	if (!InPackage)
	{
		return;
	}

	// Check if our preset asset exists inside the package
	TArray<UObject*> ObjectsInPackage;
	GetObjectsWithOuter(InPackage, ObjectsInPackage);

	for (UObject* Obj : ObjectsInPackage)
	{
		if (UKawaiiFluidPresetDataAsset* Preset = Cast<UKawaiiFluidPresetDataAsset>(Obj))
		{
			// Calling ThumbnailTools at this point physically writes the latest Draw result
			// to the thumbnail section of the .uasset file.
			ThumbnailTools::GenerateThumbnailForObjectToSaveToDisk(Preset);
		}
	}
}

/**
 * @brief Registers asset type actions for fluid presets.
 */
void FKawaiiFluidEditorModule::RegisterAssetTypeActions()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Register Fluid Preset asset type
	TSharedPtr<FAssetTypeActions_KawaiiFluidPreset> FluidPresetActions = MakeShared<FAssetTypeActions_KawaiiFluidPreset>();
	AssetTools.RegisterAssetTypeActions(FluidPresetActions.ToSharedRef());
	RegisteredAssetTypeActions.Add(FluidPresetActions);
}

/**
 * @brief Unregisters all previously registered asset type actions.
 */
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

/**
 * @brief Registers custom detail layouts for fluid-related classes.
 */
void FKawaiiFluidEditorModule::RegisterPropertyCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Register KawaiiFluidVolumeComponent detail customization
	PropertyModule.RegisterCustomClassLayout(
		UKawaiiFluidVolumeComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FKawaiiFluidVolumeComponentDetails::MakeInstance)
	);
}

/**
 * @brief Unregisters all property customizations registered by this module.
 */
void FKawaiiFluidEditorModule::UnregisterPropertyCustomizations()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(UKawaiiFluidVolumeComponent::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKawaiiFluidEditorModule, KawaiiFluidEditor)
