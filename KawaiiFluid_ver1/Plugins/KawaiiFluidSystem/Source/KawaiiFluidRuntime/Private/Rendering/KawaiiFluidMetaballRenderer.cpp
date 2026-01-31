// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidMetaballRenderer.h"
#include "Interfaces/IKawaiiFluidDataProvider.h"
#include "Rendering/FluidRendererSubsystem.h"
#include "Rendering/KawaiiFluidRenderResource.h"
#include "Core/KawaiiFluidSimulationContext.h"
#include "Core/KawaiiRenderParticle.h"
#include "RenderGraphResources.h"
#include "RenderingThread.h"
#include "GPU/GPUFluidSimulator.h"
#include "Engine/World.h"

// Pipeline architecture
#include "Rendering/Pipeline/IKawaiiMetaballRenderingPipeline.h"
#include "Rendering/Pipeline/KawaiiMetaballScreenSpacePipeline.h"

UKawaiiFluidMetaballRenderer::UKawaiiFluidMetaballRenderer()
{
	// No component tick needed - UObject doesn't tick
}

void UKawaiiFluidMetaballRenderer::Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, UKawaiiFluidPresetDataAsset* InPreset)
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
		RendererSubsystem = CachedWorld->GetSubsystem<UFluidRendererSubsystem>();

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

void UKawaiiFluidMetaballRenderer::Cleanup()
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

void UKawaiiFluidMetaballRenderer::ApplySettings(const FKawaiiFluidMetaballRendererSettings& Settings)
{
	bEnabled = Settings.bEnabled;
	bUseSimulationRadius = Settings.bUseSimulationRadius;
	bRenderSurfaceOnly = Settings.bRenderSurfaceOnly;

	// Disable rendering immediately when disabled
	if (!bEnabled)
	{
		bIsRenderingActive = false;
	}

	// Map settings to LocalParameters
	LocalParameters.FluidColor = Settings.FluidColor;
	LocalParameters.FresnelStrength = Settings.FresnelStrength;
	LocalParameters.RefractiveIndex = Settings.RefractiveIndex;
	LocalParameters.AbsorptionStrength = Settings.AbsorptionStrength;
	LocalParameters.SpecularStrength = Settings.SpecularStrength;
	LocalParameters.SpecularRoughness = Settings.SpecularRoughness;
	LocalParameters.ParticleRenderRadius = Settings.ParticleRenderRadius;
	LocalParameters.SmoothingRadius = Settings.SmoothingRadius;
	LocalParameters.ThicknessScale = Settings.ThicknessScale;

	// Anisotropy parameters
	LocalParameters.AnisotropyParams = Settings.AnisotropyParams;

	// MaxRenderParticles stays as member variable (not in LocalParameters)
	MaxRenderParticles = Settings.MaxRenderParticles;

	// Update Pipeline (ShadingMode is handled internally by Pipeline)
	UpdatePipeline();

	UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: Applied settings (Enabled: %s, UseSimRadius: %s, Color: %s, MaxParticles: %d)"),
		bEnabled ? TEXT("true") : TEXT("false"),
		bUseSimulationRadius ? TEXT("true") : TEXT("false"),
		*LocalParameters.FluidColor.ToString(),
		MaxRenderParticles);
}

void UKawaiiFluidMetaballRenderer::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	if (!bEnabled)
	{
		// Clear rendering state when disabled
		bIsRenderingActive = false;
	}
}

void UKawaiiFluidMetaballRenderer::UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime)
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

	// Get particle count (atomic, thread-safe read)
	const int32 GPUParticleCount = Simulator->GetParticleCount();

	// Access GPU buffers through RenderResource on render thread
	if (FKawaiiFluidRenderResource* RR = GetFluidRenderResource())
	{
		RR->SetGPUSimulatorReference(Simulator, GPUParticleCount, RenderRadius);
	}

	// Update stats
	LastRenderedParticleCount = FMath::Min(GPUParticleCount, MaxRenderParticles);
	bIsRenderingActive = (GPUParticleCount > 0);

	// Cache radius for shader parameters
	CachedParticleRadius = RenderRadius;

	// Debug logging
	static int32 FrameCounter = 0;
	if (++FrameCounter % 60 == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: GPU mode - Set simulator reference in RenderResource (%d particles, radius: %.2f)"),
			GPUParticleCount, RenderRadius);
	}
}

FKawaiiFluidRenderResource* UKawaiiFluidMetaballRenderer::GetFluidRenderResource() const
{
	if (CachedSimulationContext.IsValid())
	{
		return CachedSimulationContext->GetRenderResource();
	}
	return nullptr;
}

void UKawaiiFluidMetaballRenderer::SetSimulationContext(UKawaiiFluidSimulationContext* InContext)
{
	CachedSimulationContext = InContext;

	UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: %s SimulationContext"),
		InContext ? TEXT("Set") : TEXT("Cleared"));
}

bool UKawaiiFluidMetaballRenderer::IsRenderingActive() const
{
	return bIsRenderingActive && CachedSimulationContext.IsValid() && CachedSimulationContext->HasValidRenderResource();
}

void UKawaiiFluidMetaballRenderer::UpdatePipeline()
{
	// Create Pipeline if needed
	if (!Pipeline)
	{
		Pipeline = MakeShared<FKawaiiMetaballScreenSpacePipeline>();
		UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: Created ScreenSpace Pipeline"));
	}
}

void UKawaiiFluidMetaballRenderer::SetPreset(UKawaiiFluidPresetDataAsset* InPreset)
{
	CachedPreset = InPreset;

	if (CachedPreset)
	{
		// Ensure Pipeline is created
		UpdatePipeline();
		UE_LOG(LogTemp, Log, TEXT("MetaballRenderer: SetPreset - Pipeline=ScreenSpace"));
	}
}
