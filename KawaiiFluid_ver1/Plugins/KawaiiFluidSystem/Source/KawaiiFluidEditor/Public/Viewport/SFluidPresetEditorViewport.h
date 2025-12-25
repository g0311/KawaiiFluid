// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class FFluidPreviewScene;
class FKawaiiFluidPresetAssetEditor;
class FFluidPresetEditorViewportClient;

/**
 * Viewport widget for fluid preset editor
 * Displays 3D preview of fluid simulation
 */
class KAWAIIFLUIDEDITOR_API SFluidPresetEditorViewport : public SEditorViewport,
                                                          public FGCObject,
                                                          public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SFluidPresetEditorViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs,
	               TSharedPtr<FFluidPreviewScene> InPreviewScene,
	               TSharedPtr<FKawaiiFluidPresetAssetEditor> InAssetEditor);

	virtual ~SFluidPresetEditorViewport() override;

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SFluidPresetEditorViewport"); }
	//~ End FGCObject Interface

	//~ Begin ICommonEditorViewportToolbarInfoProvider Interface
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	//~ End ICommonEditorViewportToolbarInfoProvider Interface

	/** Refresh the viewport */
	void RefreshViewport();

	/** Focus camera on particles */
	void FocusOnParticles();

	/** Reset camera to default position */
	void ResetCamera();

	/** Get viewport client */
	TSharedPtr<FFluidPresetEditorViewportClient> GetViewportClient() const { return ViewportClient; }

	/** Get preview scene */
	TSharedPtr<FFluidPreviewScene> GetPreviewScene() const { return PreviewScene; }

protected:
	//~ Begin SEditorViewport Interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual void BindCommands() override;
	//~ End SEditorViewport Interface

private:
	/** Viewport client */
	TSharedPtr<FFluidPresetEditorViewportClient> ViewportClient;

	/** Preview scene reference */
	TSharedPtr<FFluidPreviewScene> PreviewScene;

	/** Asset editor reference */
	TWeakPtr<FKawaiiFluidPresetAssetEditor> AssetEditorPtr;
};
