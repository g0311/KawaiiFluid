// Copyright Epic Games, Inc. All Rights Reserved.

#include "Viewport/FluidPresetEditorViewportClient.h"
#include "Viewport/SFluidPresetEditorViewport.h"
#include "Preview/FluidPreviewScene.h"
#include "Preview/FluidPreviewSettings.h"
#include "Core/FluidParticle.h"
#include "Core/SpatialHash.h"
#include "AdvancedPreviewScene.h"

FFluidPresetEditorViewportClient::FFluidPresetEditorViewportClient(
	TSharedRef<FFluidPreviewScene> InPreviewScene,
	TSharedRef<SFluidPresetEditorViewport> InViewportWidget)
	: FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InViewportWidget))
	, PreviewScene(InPreviewScene)
	, ViewportWidgetPtr(InViewportWidget)
{
	// Setup viewport settings
	SetRealtime(true);

	// Camera settings
	SetViewLocation(FVector(-400.0f, 0.0f, 200.0f));
	SetViewRotation(FRotator(-20.0f, 0.0f, 0.0f));

	// Orbit camera around origin
	SetLookAtLocation(FVector(0.0f, 0.0f, 100.0f));

	// Enable grid
	DrawHelper.bDrawGrid = true;
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.GridColorAxis = FColor(80, 80, 80);
	DrawHelper.GridColorMajor = FColor(40, 40, 40);
	DrawHelper.GridColorMinor = FColor(20, 20, 20);

	// Visibility settings
	EngineShowFlags.SetGrid(true);
	EngineShowFlags.SetAntiAliasing(true);
}

FFluidPresetEditorViewportClient::~FFluidPresetEditorViewportClient()
{
}

void FFluidPresetEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Preview scene tick is handled by the asset editor
}

void FFluidPresetEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	// Draw debug info if enabled
	DrawDebugInfo(PDI);
}

bool FFluidPresetEditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	bool bHandled = false;

	// Handle keyboard shortcuts
	if (EventArgs.Event == IE_Pressed)
	{
		if (EventArgs.Key == EKeys::F)
		{
			// Focus on particles
			TSharedPtr<SFluidPresetEditorViewport> ViewportWidget = ViewportWidgetPtr.Pin();
			if (ViewportWidget.IsValid())
			{
				ViewportWidget->FocusOnParticles();
				bHandled = true;
			}
		}
		else if (EventArgs.Key == EKeys::H)
		{
			// Reset camera to home position
			SetInitialCameraPosition();
			bHandled = true;
		}
	}

	if (!bHandled)
	{
		bHandled = FEditorViewportClient::InputKey(EventArgs);
	}

	return bHandled;
}

void FFluidPresetEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
}

FLinearColor FFluidPresetEditorViewportClient::GetBackgroundColor() const
{
	return FLinearColor(0.1f, 0.1f, 0.12f, 1.0f);
}

void FFluidPresetEditorViewportClient::SetInitialCameraPosition()
{
	SetViewLocation(FVector(-400.0f, 0.0f, 250.0f));
	SetViewRotation(FRotator(-25.0f, 0.0f, 0.0f));
	SetLookAtLocation(FVector(0.0f, 0.0f, 100.0f));
}

void FFluidPresetEditorViewportClient::FocusOnBounds(const FBoxSphereBounds& Bounds)
{
	const float HalfFOVRadians = FMath::DegreesToRadians(ViewFOV / 2.0f);
	const float DistanceFromSphere = Bounds.SphereRadius / FMath::Tan(HalfFOVRadians);

	FVector Direction = GetViewRotation().Vector();
	FVector NewLocation = Bounds.Origin - Direction * DistanceFromSphere;

	SetViewLocation(NewLocation);
	SetLookAtLocation(Bounds.Origin);
}

void FFluidPresetEditorViewportClient::DrawDebugInfo(FPrimitiveDrawInterface* PDI)
{
	if (!PreviewScene.IsValid())
	{
		return;
	}

	const FFluidPreviewSettings& Settings = PreviewScene->GetPreviewSettings();

	if (Settings.bShowVelocityVectors)
	{
		DrawVelocityVectors(PDI);
	}

	if (Settings.bShowNeighborConnections)
	{
		DrawNeighborConnections(PDI);
	}

	if (Settings.bShowSpatialHashGrid)
	{
		DrawSpatialHashGrid(PDI);
	}
}

void FFluidPresetEditorViewportClient::DrawVelocityVectors(FPrimitiveDrawInterface* PDI)
{
	const TArray<FFluidParticle>& Particles = PreviewScene->GetParticles();

	for (const FFluidParticle& Particle : Particles)
	{
		if (Particle.Velocity.SizeSquared() > 1.0f)
		{
			FVector End = Particle.Position + Particle.Velocity * 0.1f;
			PDI->DrawLine(Particle.Position, End, FLinearColor::Yellow, SDPG_World, 1.0f);
		}
	}
}

void FFluidPresetEditorViewportClient::DrawNeighborConnections(FPrimitiveDrawInterface* PDI)
{
	const TArray<FFluidParticle>& Particles = PreviewScene->GetParticles();

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		const FFluidParticle& Particle = Particles[i];

		for (int32 NeighborIdx : Particle.NeighborIndices)
		{
			if (NeighborIdx > i && NeighborIdx < Particles.Num())
			{
				const FFluidParticle& Neighbor = Particles[NeighborIdx];
				PDI->DrawLine(Particle.Position, Neighbor.Position, FLinearColor(0.2f, 0.6f, 0.2f, 0.5f), SDPG_World, 0.5f);
			}
		}
	}
}

void FFluidPresetEditorViewportClient::DrawSpatialHashGrid(FPrimitiveDrawInterface* PDI)
{
	// Draw bounds of spatial hash
	const FFluidPreviewSettings& Settings = PreviewScene->GetPreviewSettings();
	const float HalfSize = Settings.FloorSize.X * 0.5f;
	const float Height = Settings.WallHeight;

	FVector Min(-HalfSize, -HalfSize, Settings.FloorHeight);
	FVector Max(HalfSize, HalfSize, Settings.FloorHeight + Height);

	// Draw box edges
	FLinearColor GridColor(0.3f, 0.3f, 0.5f, 0.5f);

	// Bottom
	PDI->DrawLine(FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Min.Y, Min.Z), GridColor, SDPG_World);

	// Top
	PDI->DrawLine(FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Max.X, Min.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Max.X, Max.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Min.X, Max.Y, Max.Z), FVector(Min.X, Min.Y, Max.Z), GridColor, SDPG_World);

	// Verticals
	PDI->DrawLine(FVector(Min.X, Min.Y, Min.Z), FVector(Min.X, Min.Y, Max.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Max.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Max.X, Max.Y, Min.Z), FVector(Max.X, Max.Y, Max.Z), GridColor, SDPG_World);
	PDI->DrawLine(FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Max.Z), GridColor, SDPG_World);
}
