// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_KawaiiFluidPreset.h"
#include "KawaiiFluidEditor.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Editor/KawaiiFluidPresetAssetEditor.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_FluidPreset"

/**
 * @brief Returns the name of the asset type.
 * @return Localized asset type name
 */
FText FAssetTypeActions_KawaiiFluidPreset::GetName() const
{
	return LOCTEXT("AssetName", "Kawaii Fluid Preset");
}

/**
 * @brief Returns the class supported by these actions.
 * @return Static class pointer
 */
UClass* FAssetTypeActions_KawaiiFluidPreset::GetSupportedClass() const
{
	return UKawaiiFluidPresetDataAsset::StaticClass();
}

/**
 * @brief Returns the color associated with this asset type in the Content Browser.
 * @return FColor for the asset type
 */
FColor FAssetTypeActions_KawaiiFluidPreset::GetTypeColor() const
{
	return FColor(50, 100, 200);
}

/**
 * @brief Returns the asset categories under which this type is grouped.
 * @return Bitmask of categories
 */
uint32 FAssetTypeActions_KawaiiFluidPreset::GetCategories()
{
	return FKawaiiFluidEditorModule::Get().GetAssetCategory();
}

/**
 * @brief Opens the specialized fluid preset editor for the selected objects.
 * @param InObjects The objects to edit
 * @param EditWithinLevelEditor The host toolkit
 */
void FAssetTypeActions_KawaiiFluidPreset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (UKawaiiFluidPresetDataAsset* Preset = Cast<UKawaiiFluidPresetDataAsset>(Object))
		{
			TSharedRef<FKawaiiFluidPresetAssetEditor> NewEditor = MakeShared<FKawaiiFluidPresetAssetEditor>();
			NewEditor->InitFluidPresetEditor(Mode, EditWithinLevelEditor, Preset);
		}
	}
}

/**
 * @brief Returns the thumbnail info for the asset, creating it if it doesn't exist.
 * @param Asset The asset to get info for
 * @return Pointer to thumbnail info
 */
UThumbnailInfo* FAssetTypeActions_KawaiiFluidPreset::GetThumbnailInfo(UObject* Asset) const
{
	UKawaiiFluidPresetDataAsset* Preset = CastChecked<UKawaiiFluidPresetDataAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = Preset->ThumbnailInfo;
	if (ThumbnailInfo == nullptr)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(Preset, NAME_None, RF_Transactional);
		Preset->ThumbnailInfo = ThumbnailInfo;
	}
	return ThumbnailInfo;
}

#undef LOCTEXT_NAMESPACE
