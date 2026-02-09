// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Viewport/SKawaiiFluidPresetEditorViewport.h"
#include "Viewport/KawaiiFluidPresetEditorViewportClient.h"
#include "Preview/KawaiiFluidPreviewScene.h"
#include "Editor/KawaiiFluidPresetAssetEditor.h"
#include "Widgets/SKawaiiFluidPreviewStatsOverlay.h"
#include "Core/KawaiiFluidParticle.h"
#include "SEditorViewportToolBarMenu.h"
#include "STransformViewportToolbar.h"

/**
 * @brief Initializes the viewport widget and sets up the reference to the preview scene.
 * @param InArgs Slate arguments
 * @param InPreviewScene Shared pointer to the preview scene to display
 * @param InAssetEditor Shared pointer to the parent asset editor
 */
void SKawaiiFluidPresetEditorViewport::Construct(const FArguments& InArgs,
                                            TSharedPtr<FKawaiiFluidPreviewScene> InPreviewScene,
                                            TSharedPtr<FKawaiiFluidPresetAssetEditor> InAssetEditor)
{
	PreviewScene = InPreviewScene;
	AssetEditorPtr = InAssetEditor;

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SKawaiiFluidPresetEditorViewport::~SKawaiiFluidPresetEditorViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}
}

/**
 * @brief Empty implementation as there are no raw UObject pointers to track currently.
 * @param Collector The reference collector
 */
void SKawaiiFluidPresetEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Add references to prevent garbage collection
}

/**
 * @brief Returns a reference to this viewport widget.
 * @return Shared reference to the viewport widget
 */
TSharedRef<SEditorViewport> SKawaiiFluidPresetEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

/**
 * @brief Returns an extender for the viewport toolbar.
 * @return Shared pointer to the extender
 */
TSharedPtr<FExtender> SKawaiiFluidPresetEditorViewport::GetExtenders() const
{
	return MakeShared<FExtender>();
}

/**
 * @brief Handler for when the floating button is clicked.
 */
void SKawaiiFluidPresetEditorViewport::OnFloatingButtonClicked()
{
	// Handle floating button click if needed
}

/**
 * @brief Tells the viewport client to redraw.
 */
void SKawaiiFluidPresetEditorViewport::RefreshViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}

/**
 * @brief Calculates the bounding box of all particles in the preview scene and focuses the camera on them.
 */
void SKawaiiFluidPresetEditorViewport::FocusOnParticles()
{
	if (!ViewportClient.IsValid() || !PreviewScene.IsValid())
	{
		return;
	}

	const TArray<FKawaiiFluidParticle>& Particles = PreviewScene->GetParticles();
	if (Particles.Num() == 0)
	{
		return;
	}

	// Calculate bounds of all particles
	FBox Bounds(ForceInit);
	for (const FKawaiiFluidParticle& Particle : Particles)
	{
		Bounds += Particle.Position;
	}

	// Expand bounds slightly
	Bounds = Bounds.ExpandBy(50.0f);

	ViewportClient->FocusOnBounds(FBoxSphereBounds(Bounds));
}

/**
 * @brief Resets the camera to the default start position defined in the viewport client.
 */
void SKawaiiFluidPresetEditorViewport::ResetCamera()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetInitialCameraPosition();
	}
}

/**
 * @brief Factory method to create the specific viewport client for the fluid editor.
 * @return Shared reference to the created viewport client
 */
TSharedRef<FEditorViewportClient> SKawaiiFluidPresetEditorViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FKawaiiFluidPresetEditorViewportClient>(
		PreviewScene.ToSharedRef(),
		SharedThis(this));

	ViewportClient->SetInitialCameraPosition();

	return ViewportClient.ToSharedRef();
}

/**
 * @brief Adds the particle stats overlay (FPS, Count, etc.) to the viewport.
 * @param Overlay The overlay widget to populate
 */
void SKawaiiFluidPresetEditorViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	// Add stats overlay
	Overlay->AddSlot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Left)
		.Padding(10.0f)
		[
			SNew(SKawaiiFluidPreviewStatsOverlay, PreviewScene)
		];
}

/**
 * @brief Binds viewport-specific commands.
 */
void SKawaiiFluidPresetEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// Bind additional commands if needed
}
