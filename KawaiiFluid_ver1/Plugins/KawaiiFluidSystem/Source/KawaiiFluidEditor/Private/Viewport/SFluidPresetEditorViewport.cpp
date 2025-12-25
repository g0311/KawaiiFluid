// Copyright Epic Games, Inc. All Rights Reserved.

#include "Viewport/SFluidPresetEditorViewport.h"
#include "Viewport/FluidPresetEditorViewportClient.h"
#include "Preview/FluidPreviewScene.h"
#include "Editor/KawaiiFluidPresetAssetEditor.h"
#include "Widgets/SFluidPreviewStatsOverlay.h"
#include "Core/FluidParticle.h"
#include "SEditorViewportToolBarMenu.h"
#include "STransformViewportToolbar.h"

void SFluidPresetEditorViewport::Construct(const FArguments& InArgs,
                                            TSharedPtr<FFluidPreviewScene> InPreviewScene,
                                            TSharedPtr<FKawaiiFluidPresetAssetEditor> InAssetEditor)
{
	PreviewScene = InPreviewScene;
	AssetEditorPtr = InAssetEditor;

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SFluidPresetEditorViewport::~SFluidPresetEditorViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}
}

void SFluidPresetEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Add references to prevent garbage collection
}

TSharedRef<SEditorViewport> SFluidPresetEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SFluidPresetEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SFluidPresetEditorViewport::OnFloatingButtonClicked()
{
	// Handle floating button click if needed
}

void SFluidPresetEditorViewport::RefreshViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}

void SFluidPresetEditorViewport::FocusOnParticles()
{
	if (!ViewportClient.IsValid() || !PreviewScene.IsValid())
	{
		return;
	}

	const TArray<FFluidParticle>& Particles = PreviewScene->GetParticles();
	if (Particles.Num() == 0)
	{
		return;
	}

	// Calculate bounds of all particles
	FBox Bounds(ForceInit);
	for (const FFluidParticle& Particle : Particles)
	{
		Bounds += Particle.Position;
	}

	// Expand bounds slightly
	Bounds = Bounds.ExpandBy(50.0f);

	ViewportClient->FocusOnBounds(FBoxSphereBounds(Bounds));
}

void SFluidPresetEditorViewport::ResetCamera()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetInitialCameraPosition();
	}
}

TSharedRef<FEditorViewportClient> SFluidPresetEditorViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FFluidPresetEditorViewportClient(
		PreviewScene.ToSharedRef(),
		SharedThis(this)));

	ViewportClient->SetInitialCameraPosition();

	return ViewportClient.ToSharedRef();
}

void SFluidPresetEditorViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	// Add stats overlay
	Overlay->AddSlot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Left)
		.Padding(10.0f)
		[
			SNew(SFluidPreviewStatsOverlay, PreviewScene)
		];
}

void SFluidPresetEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// Bind additional commands if needed
}
