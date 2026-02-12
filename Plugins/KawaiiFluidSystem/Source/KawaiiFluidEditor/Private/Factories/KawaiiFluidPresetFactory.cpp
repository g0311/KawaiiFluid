// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Factories/KawaiiFluidPresetFactory.h"
#include "KawaiiFluidEditor.h"
#include "Core/KawaiiFluidPresetDataAsset.h"

#define LOCTEXT_NAMESPACE "KawaiiFluidPresetFactory"

/**
 * @brief Default constructor setting up the supported class and creation flags.
 */
UKawaiiFluidPresetFactory::UKawaiiFluidPresetFactory()
{
	SupportedClass = UKawaiiFluidPresetDataAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

/**
 * @brief Creates a new instance of the fluid preset data asset.
 * @param InClass The class to create
 * @param InParent The parent object
 * @param InName The name of the new asset
 * @param Flags Object flags
 * @param Context Creation context
 * @param Warn Feedback context for errors/warnings
 * @return The newly created object
 */
UObject* UKawaiiFluidPresetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UKawaiiFluidPresetDataAsset>(InParent, InClass, InName, Flags);
}

/**
 * @brief Returns the menu categories where this asset should appear.
 * @return Bitmask of asset categories
 */
uint32 UKawaiiFluidPresetFactory::GetMenuCategories() const
{
	return FKawaiiFluidEditorModule::Get().GetAssetCategory();
}

/**
 * @brief Returns the display name shown in the Content Browser creation menu.
 * @return Localized display name
 */
FText UKawaiiFluidPresetFactory::GetDisplayName() const
{
	return LOCTEXT("FactoryDisplayName", "Fluid Preset");
}

#undef LOCTEXT_NAMESPACE
