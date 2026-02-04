// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/Pipeline/KawaiiFluidScreenSpacePipeline.h"
#include "Rendering/KawaiiFluidMetaballRenderer.h"
#include "Rendering/FluidDepthPass.h"
#include "Rendering/FluidSmoothingPass.h"
#include "Rendering/FluidNormalPass.h"
#include "Rendering/FluidThicknessPass.h"

// Separated shading implementation
#include "Rendering/Shading/FluidCompositeShading.h"
#include "Rendering/FluidSurfaceDecorationPass.h"
#include "Rendering/FluidFlowAccumulationPass.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "SceneTextures.h"

// Helper function to generate intermediate textures (depth, normal, thickness)
static bool GenerateIntermediateTextures(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef& GlobalDepthTexture, // Reference to allow updating the accumulated hardware depth
	FMetaballIntermediateTextures& OutIntermediateTextures)
{
	// Calculate average particle radius for this batch
	float AverageParticleRadius = 10.0f;
	float TotalRadius = 0.0f;
	int ValidCount = 0;

	for (UKawaiiFluidMetaballRenderer* Renderer : Renderers)
	{
		TotalRadius += Renderer->GetCachedParticleRadius();
		ValidCount++;
	}

	if (ValidCount > 0)
	{
		AverageParticleRadius = TotalRadius / ValidCount;
	}

	// Calculate DepthFalloff considering anisotropy
	float DepthFalloff = AverageParticleRadius * 0.7f;
	if (RenderParams.AnisotropyParams.bEnabled)
	{
		float AnisotropyMultiplier = FMath::Max(1.0f, RenderParams.AnisotropyParams.MaxStretch);
		DepthFalloff *= AnisotropyMultiplier * 2.0f;
	}
	const int32 NumIterations = RenderParams.SmoothingIterations;

	// 1. Depth Pass (outputs linear depth + screen-space velocity + occlusion mask for THIS BATCH)
	// IMPORTANT: Use GlobalDepthTexture (Hardware Depth: Scene + Previous Fluids) as the depth reference.
	// We use bIncremental=true to ensure RenderFluidDepthPass starts with this existing depth.
	FRDGTextureRef DepthTexture = nullptr;
	FRDGTextureRef VelocityTexture = nullptr;
	FRDGTextureRef OcclusionMaskTexture = nullptr;
	FRDGTextureRef HardwareDepthTexture = nullptr;

	// Cache the depth BEFORE it is updated by the current batch.
	// This will be used as the background (refraction/transmittance reference) in the shading pass.
	OutIntermediateTextures.BackgroundDepthTexture = GlobalDepthTexture;
	
	RenderFluidDepthPass(GraphBuilder, View, Renderers, GlobalDepthTexture, 
		DepthTexture, VelocityTexture, OcclusionMaskTexture, HardwareDepthTexture, true);

	if (!DepthTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballScreenSpacePipeline: Depth pass failed"));
		return false;
	}

	// [IMPORTANT] Update GlobalDepthTexture with the NEW hardware depth.
	// This ensures the next batch knows about this batch's surface.
	GlobalDepthTexture = HardwareDepthTexture;

	// 2. Smoothing Pass - Narrow-Range Filter (Truong & Yuksel 2018)
	FRDGTextureRef SmoothedDepthTexture = nullptr;

	// Calculate adjusted particle radius for anisotropy
	float AdjustedParticleRadius = AverageParticleRadius;
	if (RenderParams.AnisotropyParams.bEnabled)
	{
		float AnisotropyMultiplier = FMath::Max(1.0f, RenderParams.AnisotropyParams.MaxStretch);
		AdjustedParticleRadius *= AnisotropyMultiplier;
	}

	// Distance-based dynamic smoothing parameters
	FDistanceBasedSmoothingParams DistanceBasedParams;
	DistanceBasedParams.WorldScale = RenderParams.SmoothingWorldScale;
	DistanceBasedParams.MinRadius = RenderParams.SmoothingMinRadius;
	DistanceBasedParams.MaxRadius = RenderParams.SmoothingMaxRadius;
	
	RenderFluidNarrowRangeSmoothingPass(GraphBuilder, View, DepthTexture, SmoothedDepthTexture,
	                                    AdjustedParticleRadius,
	                                    RenderParams.NarrowRangeThresholdRatio,
	                                    RenderParams.NarrowRangeClampRatio,
	                                    NumIterations,
	                                    RenderParams.NarrowRangeGrazingBoost,
	                                    DistanceBasedParams);

	if (!SmoothedDepthTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballScreenSpacePipeline: Smoothing pass failed"));
		return false;
	}

	// 3. Normal Pass
	FRDGTextureRef NormalTexture = nullptr;
	RenderFluidNormalPass(GraphBuilder, View, SmoothedDepthTexture, NormalTexture);

	if (!NormalTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballScreenSpacePipeline: Normal pass failed"));
		return false;
	}

	// 4. Thickness Pass
	FRDGTextureRef ThicknessTexture = nullptr;
	RenderFluidThicknessPass(GraphBuilder, View, Renderers, SceneDepthTexture, ThicknessTexture);

	if (!ThicknessTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballScreenSpacePipeline: Thickness pass failed"));
		return false;
	}

	// 5. Thickness Smoothing Pass (Separable Gaussian Blur)
	FRDGTextureRef SmoothedThicknessTexture = nullptr;
	const float ThicknessBlurRadius = static_cast<float>(RenderParams.SmoothingMaxRadius);
	RenderFluidThicknessSmoothingPass(GraphBuilder, View, ThicknessTexture, SmoothedThicknessTexture,
	                                  ThicknessBlurRadius, 2);  // 2 iterations for thickness

	if (!SmoothedThicknessTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballScreenSpacePipeline: Thickness smoothing pass failed"));
		SmoothedThicknessTexture = ThicknessTexture;  // Fallback to unsmoothed
	}

	// 6. Velocity Smoothing Pass (Separable Gaussian Blur)
	FRDGTextureRef FinalVelocityTexture = VelocityTexture;
	const bool bShouldSmoothVelocity = RenderParams.SurfaceDecoration.bEnabled &&
		RenderParams.SurfaceDecoration.Foam.bEnabled &&
		RenderParams.SurfaceDecoration.Foam.bVelocitySmoothing &&
		VelocityTexture != nullptr;

	if (bShouldSmoothVelocity)
	{
		FRDGTextureRef SmoothedVelocityTexture = nullptr;
		RenderFluidVelocitySmoothingPass(
			GraphBuilder, View, VelocityTexture, SmoothedVelocityTexture,
			RenderParams.SurfaceDecoration.Foam.VelocitySmoothingRadius,
			RenderParams.SurfaceDecoration.Foam.VelocitySmoothingIterations);

		if (SmoothedVelocityTexture)
		{
			FinalVelocityTexture = SmoothedVelocityTexture;
		}
	}

	// Build output
	OutIntermediateTextures.SmoothedDepthTexture = SmoothedDepthTexture;
	OutIntermediateTextures.NormalTexture = NormalTexture;
	OutIntermediateTextures.ThicknessTexture = SmoothedThicknessTexture;
	OutIntermediateTextures.VelocityTexture = FinalVelocityTexture;
	OutIntermediateTextures.OcclusionMaskTexture = OcclusionMaskTexture;

	return true;
}


void FKawaiiFluidScreenSpacePipeline::PrepareRender(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef& GlobalDepthTexture,
	FRDGTextureRef GlobalVelocityTexture,
	FRDGTextureRef GlobalOcclusionMask)
{
	if (Renderers.Num() == 0)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MetaballPipeline_ScreenSpace_PrepareForTonemap");

	// Generate and cache intermediate textures using hardware depth updates
	if (!GenerateIntermediateTextures(GraphBuilder, View, RenderParams, Renderers, SceneDepthTexture, 
		GlobalDepthTexture, CachedIntermediateTextures))
	{
		return;
	}

	// IMPORTANT: Clear AccumulatedFlowTexture to prevent stale pointer from previous frame
	// RDG textures are only valid within the same frame's RDG graph
	CachedIntermediateTextures.AccumulatedFlowTexture = nullptr;

	// Flow Accumulation Pass (if enabled)
	const bool bShouldAccumulateFlow = RenderParams.SurfaceDecoration.bEnabled &&
		RenderParams.SurfaceDecoration.FlowMap.bEnabled &&
		CachedIntermediateTextures.VelocityTexture != nullptr;

	// Clear history when flow accumulation is disabled to prevent stale data and memory leak
	if (!bShouldAccumulateFlow)
	{
		if (PrevAccumulatedFlowRT.IsValid())
		{
			PrevAccumulatedFlowRT = nullptr;
			bHasPrevFrameData = false;
			UE_LOG(LogTemp, Verbose, TEXT("FKawaiiMetaballScreenSpacePipeline: Flow accumulation disabled, cleared history buffer"));
		}
	}

	if (bShouldAccumulateFlow)
	{
		// Get previous accumulated flow from history
		FRDGTextureRef PrevAccumulatedFlowTexture = nullptr;
		if (PrevAccumulatedFlowRT.IsValid())
		{
			PrevAccumulatedFlowTexture = GraphBuilder.RegisterExternalTexture(
				PrevAccumulatedFlowRT,
				TEXT("FluidPrevAccumulatedFlow"));
		}

		// Setup flow accumulation parameters from surface decoration settings
		FFlowAccumulationParams FlowParams;
		FlowParams.VelocityScale = RenderParams.SurfaceDecoration.FlowMap.VelocityScale;
		FlowParams.FlowDecay = RenderParams.SurfaceDecoration.FlowMap.FlowDecay;
		FlowParams.MaxFlowOffset = RenderParams.SurfaceDecoration.FlowMap.MaxFlowOffset;

		// Current frame's matrices for world position reconstruction
		FlowParams.InvViewProjectionMatrix = View.ViewMatrices.GetInvViewProjectionMatrix();
		FlowParams.InvViewMatrix = View.ViewMatrices.GetInvViewMatrix();
		FlowParams.InvProjectionMatrix = View.ViewMatrices.GetInvProjectionMatrix();

		// Previous frame's view-projection for temporal reprojection
		// Use current frame's matrix if no previous data (first frame)
		FlowParams.PrevViewProjectionMatrix = bHasPrevFrameData
			? PrevViewProjectionMatrix
			: View.ViewMatrices.GetViewProjectionMatrix();

		// Run flow accumulation pass
		FRDGTextureRef AccumulatedFlowTexture = nullptr;
		RenderFluidFlowAccumulationPass(
			GraphBuilder,
			View,
			FlowParams,
			CachedIntermediateTextures.VelocityTexture,
			CachedIntermediateTextures.SmoothedDepthTexture,
			PrevAccumulatedFlowTexture,
			AccumulatedFlowTexture);

		// Store in intermediate textures
		CachedIntermediateTextures.AccumulatedFlowTexture = AccumulatedFlowTexture;

		// Extract to history for next frame
		if (AccumulatedFlowTexture)
		{
			GraphBuilder.QueueTextureExtraction(AccumulatedFlowTexture, &PrevAccumulatedFlowRT);
		}

		// Save current frame's ViewProjectionMatrix for next frame's reprojection
		PrevViewProjectionMatrix = View.ViewMatrices.GetViewProjectionMatrix();
		bHasPrevFrameData = true;
	}

	UE_LOG(LogTemp, Verbose, TEXT("FKawaiiMetaballScreenSpacePipeline: PrepareForTonemap completed - intermediate textures cached"));
}


void FKawaiiFluidScreenSpacePipeline::ExecuteRender(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef GlobalDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output)
{
	if (Renderers.Num() == 0)
	{
		return;
	}


	// Validate cached intermediate textures
	if (!CachedIntermediateTextures.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballScreenSpacePipeline: Missing cached intermediate textures for Tonemap"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MetaballPipeline_ScreenSpace_Tonemap");

	// Check if Surface Decoration is enabled
	const bool bUseSurfaceDecoration = RenderParams.SurfaceDecoration.bEnabled;

	if (bUseSurfaceDecoration)
	{
		// Surface Decoration enabled: render to intermediate texture first
		// Use same size as SceneDepthTexture to ensure consistent alignment with other passes
		FIntPoint TextureExtent = SceneDepthTexture->Desc.Extent;

		// Create intermediate composite result texture
		FRDGTextureDesc CompositeDesc = FRDGTextureDesc::Create2D(
			TextureExtent,
			PF_FloatRGBA,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV);
		FRDGTextureRef CompositeResultTexture = GraphBuilder.CreateTexture(
			CompositeDesc, TEXT("FluidCompositeResult"));

		// Create intermediate render target for composite pass (use Output's ViewRect)
		FScreenPassRenderTarget IntermediateOutput;
		IntermediateOutput.Texture = CompositeResultTexture;
		IntermediateOutput.ViewRect = Output.ViewRect;  // Same ViewRect as final output
		IntermediateOutput.LoadAction = ERenderTargetLoadAction::EClear;

		// Copy scene color to intermediate (same size, safe to use CopyTexture)
		AddCopyTexturePass(GraphBuilder, SceneColorTexture, CompositeResultTexture);

		// Render fluid composite to intermediate texture
		// Use BackgroundDepthTexture (Hardware) which contains background + previous fluid info (excluding current fluid)
		KawaiiScreenSpaceShading::RenderPostProcessShading(
			GraphBuilder,
			View,
			RenderParams,
			CachedIntermediateTextures,
			CachedIntermediateTextures.BackgroundDepthTexture,
			SceneColorTexture,
			IntermediateOutput);

		// Apply Surface Decoration and output to final target
		FRDGTextureRef DecoratedTexture = nullptr;
		RenderFluidSurfaceDecorationPass(
			GraphBuilder,
			View,
			RenderParams.SurfaceDecoration,
			CachedIntermediateTextures.SmoothedDepthTexture,
			CachedIntermediateTextures.NormalTexture,
			CachedIntermediateTextures.ThicknessTexture,
			CompositeResultTexture,
			CachedIntermediateTextures.VelocityTexture,         // Screen-space velocity
			CachedIntermediateTextures.AccumulatedFlowTexture,  // Accumulated flow UV offset
			CachedIntermediateTextures.OcclusionMaskTexture,    // Occlusion mask for depth culling
			Output.ViewRect,  // Pass the actual ViewRect where fluid was rendered
			DecoratedTexture);

		// Copy decorated result to final output (same size now)
		AddCopyTexturePass(GraphBuilder, DecoratedTexture, Output.Texture);
	}
	else
	{
		// Surface Decoration disabled: render directly to output (no overhead)
		// Use BackgroundDepthTexture (Hardware) which contains background + previous fluid info (excluding current fluid)
		KawaiiScreenSpaceShading::RenderPostProcessShading(
			GraphBuilder,
			View,
			RenderParams,
			CachedIntermediateTextures,
			CachedIntermediateTextures.BackgroundDepthTexture,
			SceneColorTexture,
			Output);
	}
}
