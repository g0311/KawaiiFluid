// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidRenderer.h"
#include "Core/IKawaiiFluidDataProvider.h"
#include "Rendering/KawaiiFluidRendererSubsystem.h"
#include "Rendering/Resources/KawaiiFluidRenderResource.h"
#include "Core/KawaiiFluidSimulationContext.h"
#include "Core/KawaiiFluidRenderParticle.h"
#include "RenderGraphResources.h"
#include "RenderingThread.h"
#include "Simulation/GPUFluidSimulator.h"
#include "Engine/World.h"

// Pipeline architecture
#include "Rendering/Pipeline/IKawaiiFluidRenderingPipeline.h"
#include "Rendering/Pipeline/KawaiiFluidScreenSpacePipeline.h"

UKawaiiFluidRenderer::UKawaiiFluidRenderer()
{
	// No component tick needed - UObject doesn't tick
}

/**
 * @brief Initialize the metaball renderer with context and rendering parameters.
 * 
 * @param InWorld World context.
 * @param InOwnerComponent Parent component reference.
 * @param InPreset Rendering property asset.
 */
void UKawaiiFluidRenderer::Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, UKawaiiFluidPresetDataAsset* InPreset)
{
	CachedWorld = InWorld;
	CachedOwnerComponent = InOwnerComponent;
	CachedPreset = InPreset;

	if (!CachedWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidMetaballRenderer::Initialize - No world context provided"));
	}

	if (!CachedOwnerComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidMetaballRenderer::Initialize - No owner component provided"));
	}

	// Cache renderer subsystem for ViewExtension access
	if (CachedWorld)
	{
		RendererSubsystem = CachedWorld->GetSubsystem<UKawaiiFluidRendererSubsystem>();
 
		if (!RendererSubsystem.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidMetaballRenderer: Failed to get FluidRendererSubsystem"));
		}
	}


	// Create Pipeline based on Preset
	if (CachedPreset)
	{
		UpdatePipeline();
	}

	UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: Initialized GPU resources (MaxParticles: %d)"),
		MaxRenderParticles);
}

void UKawaiiFluidRenderer::Cleanup()
{
	// Clear context reference - Context owns the RenderResource
	CachedSimulationContext.Reset();

	// Clear cached data
	CachedParticlePositions.Empty();
	RendererSubsystem = nullptr;
	bIsRenderingActive = false;

	// Clear cached references
	CachedWorld = nullptr;
	CachedOwnerComponent = nullptr;
	bEnabled = false;

	UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: Cleanup completed"));
}

void UKawaiiFluidRenderer::ApplySettings(const FKawaiiFluidMetaballRendererSettings& Settings)
{
	bEnabled = Settings.bEnabled;
	bUseSimulationRadius = Settings.bUseSimulationRadius;
	bRenderSurfaceOnly = Settings.bRenderSurfaceOnly;

	// Disable rendering immediately when disabled
	if (!bEnabled)
	{
		bIsRenderingActive = false;
	}

	// MaxRenderParticles stays as member variable (not in LocalParameters)
	MaxRenderParticles = Settings.MaxRenderParticles;

	// Update Pipeline (ShadingMode is handled internally by Pipeline)
	UpdatePipeline();

	UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: Applied settings (Enabled: %s, UseSimRadius: %s, MaxParticles: %d)"),
		bEnabled ? TEXT("true") : TEXT("false"),
		bUseSimulationRadius ? TEXT("true") : TEXT("false"),
		MaxRenderParticles);
}

void UKawaiiFluidRenderer::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	if (!bEnabled)
	{
		// Clear rendering state when disabled
		bIsRenderingActive = false;
	}
}

/**
 * @brief Updates GPU-side render resources with latest simulation data.
 * 
 * Sets the simulator reference and anisotropy parameters needed for the screen-space pipeline.
 * 
 * @param DataProvider Source of simulation particle data.
 * @param DeltaTime Frame time step.
 */
void UKawaiiFluidRenderer::UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime)
{
	
	
	if (!bEnabled || !DataProvider)
	{
		bIsRenderingActive = false;
		return;
	}

	// Get particle radius from simulation
	float ParticleRadius = DataProvider->GetParticleRadius();

	// Determine which radius to use for rendering
	float RenderRadius;
	if (bUseSimulationRadius)
	{
		RenderRadius = ParticleRadius;
	}
	else
	{
		RenderRadius = GetLocalParameters().ParticleRenderRadius;
	}

	// =====================================================
	// GPU path: Set simulator reference in RenderResource
	// Render thread accesses GPU buffers directly through RenderResource
	// =====================================================
	FGPUFluidSimulator* Simulator = DataProvider->GetGPUSimulator();

	if (!Simulator)
	{
		bIsRenderingActive = false;
		LastRenderedParticleCount = 0;
		return;
	}

	// Update anisotropy parameters to GPU simulator
	Simulator->SetAnisotropyParams(GetLocalParameters().AnisotropyParams);

	// Use MaxParticleCount for buffer sizing (immune to CPU/GPU count desync)
	const int32 MaxParticleCount = Simulator->GetMaxParticleCount();

	// Access GPU buffers through RenderResource on render thread
	if (FKawaiiFluidRenderResource* RR = GetFluidRenderResource())
	{
		RR->SetGPUSimulatorReference(Simulator, MaxParticleCount, RenderRadius);
	}

	// Update stats (stale CPU count is fine for UI display)
	LastRenderedParticleCount = FMath::Min(Simulator->GetParticleCount(), MaxRenderParticles);
	// Skip full-screen post-process passes when no particles remain
	// Despawn is CPU-initiated, so GetParticleCount() is already 0 by this point
	bIsRenderingActive = Simulator->HasEverHadParticles() && Simulator->GetParticleCount() > 0;

	// Cache radius for shader parameters
	CachedParticleRadius = RenderRadius;

	// // Debug logging
	// static int32 FrameCounter = 0;
	// if (++FrameCounter % 60 == 0)
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: GPU mode - Set simulator reference in RenderResource (%d particles, radius: %.2f)"),
	// 		GPUParticleCount, RenderRadius);
	// }
}

FKawaiiFluidRenderResource* UKawaiiFluidRenderer::GetFluidRenderResource() const
{
	if (CachedSimulationContext.IsValid())
	{
		return CachedSimulationContext->GetRenderResource();
	}
	return nullptr;
}

void UKawaiiFluidRenderer::SetSimulationContext(UKawaiiFluidSimulationContext* InContext)
{
	CachedSimulationContext = InContext;

	UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: %s SimulationContext"),
		InContext ? TEXT("Set") : TEXT("Cleared"));
}

bool UKawaiiFluidRenderer::IsRenderingActive() const
{
	return bIsRenderingActive && CachedSimulationContext.IsValid() && CachedSimulationContext->HasValidRenderResource();
}

void UKawaiiFluidRenderer::UpdatePipeline()
{
	// Create Pipeline if needed
	if (!Pipeline)
	{
		Pipeline = MakeShared<FKawaiiFluidScreenSpacePipeline>();
		UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: Created ScreenSpace Pipeline"));
	}
}

void UKawaiiFluidRenderer::SetPreset(UKawaiiFluidPresetDataAsset* InPreset)
{
	CachedPreset = InPreset;

	if (CachedPreset)
	{
		// Ensure Pipeline is created
		UpdatePipeline();
		UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: SetPreset - Pipeline=ScreenSpace"));
	}
}
