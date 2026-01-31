// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "Rendering/Pipeline/IKawaiiMetaballRenderingPipeline.h"

/**
 * ScreenSpace Pipeline for Metaball Rendering
 *
 * Surface computation method:
 * 1. Depth Pass - Render fluid particles to depth buffer
 * 2. Smoothing Pass - Bilateral filter on depth for smooth surface
 * 3. Normal Pass - Reconstruct normals from smoothed depth
 * 4. Thickness Pass - Accumulate particle thickness
 * 5. Shading - Apply PostProcess shading (Blinn-Phong, Fresnel, Beer's Law)
 *
 * All rendering happens at PrePostProcess timing.
 */
class FKawaiiMetaballScreenSpacePipeline : public IKawaiiMetaballRenderingPipeline
{
public:
	FKawaiiMetaballScreenSpacePipeline() = default;
	virtual ~FKawaiiMetaballScreenSpacePipeline() = default;

	//========================================
	// IKawaiiMetaballRenderingPipeline Interface
	//========================================



	/** Prepare intermediate textures for rendering (depth, normal, thickness) */
	virtual void PrepareRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture) override;

	/** Execute rendering - applies PostProcess shading */
	virtual void ExecuteRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef SceneColorTexture,
		FScreenPassRenderTarget Output) override;

	virtual const FMetaballIntermediateTextures* GetCachedIntermediateTextures() const override
	{
		return CachedIntermediateTextures.IsValid() ? &CachedIntermediateTextures : nullptr;
	}

private:
	/** Cached intermediate textures from PostBasePass for use in Tonemap */
	FMetaballIntermediateTextures CachedIntermediateTextures;

	/** Pooled texture for previous frame's accumulated flow (for temporal accumulation) */
	TRefCountPtr<IPooledRenderTarget> PrevAccumulatedFlowRT;

	/** Previous frame's ViewProjectionMatrix for temporal reprojection */
	FMatrix PrevViewProjectionMatrix = FMatrix::Identity;

	/** Flag to indicate if we have valid previous frame data */
	bool bHasPrevFrameData = false;

	// Note: Shading methods are in KawaiiScreenSpaceShadingImpl.h/cpp
	// This pipeline delegates to KawaiiScreenSpaceShading::* namespace functions
};
