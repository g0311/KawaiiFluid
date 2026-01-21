// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "KawaiiFluidPresetThumbnailRenderer.generated.h"

class UMaterialInstanceDynamic;
class FKawaiiFluidPresetThumbnailScene;

/**
 * Renders a thumbnail for UKawaiiFluidPresetDataAsset as a sphere with fluid material properties.
 */
UCLASS()
class KAWAIIFLUIDEDITOR_API UKawaiiFluidPresetThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	UKawaiiFluidPresetThumbnailRenderer();

	virtual void Draw(
		UObject* Object,
		int32 X,
		int32 Y,
		uint32 Width,
		uint32 Height,
		FRenderTarget* RenderTarget,
		FCanvas* Canvas,
		bool bAdditionalContext) override;

	virtual void BeginDestroy() override;

private:
	/** Preview scene for 3D rendering */
	FKawaiiFluidPresetThumbnailScene* ThumbnailScene;
};
