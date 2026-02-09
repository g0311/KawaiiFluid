// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "Rendering/Pipeline/IKawaiiFluidRenderingPipeline.h"

/**
 * @class FKawaiiFluidScreenSpacePipeline
 * @brief Standard screen-space rendering pipeline for fluid surfaces.
 * 
 * Computes fluid surfaces using a series of screen-space passes:
 * 1. Depth Pass - Particle rasterization.
 * 2. Smoothing Pass - Narrow-range bilateral filtering.
 * 3. Normal Pass - Normal reconstruction from depth.
 * 4. Thickness Pass - View-space thickness accumulation.
 * 5. Shading - Blinn-Phong lighting with Beer's law absorption.
 */
class FKawaiiFluidScreenSpacePipeline : public IKawaiiMetaballRenderingPipeline
{
public:
	FKawaiiFluidScreenSpacePipeline() = default;
	virtual ~FKawaiiFluidScreenSpacePipeline() = default;

	virtual void PrepareRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef& GlobalDepthTexture,
		FRDGTextureRef GlobalVelocityTexture = nullptr,
		FRDGTextureRef GlobalOcclusionMask = nullptr) override;

	virtual void ExecuteRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef GlobalDepthTexture,
		FRDGTextureRef SceneColorTexture,
		FScreenPassRenderTarget Output) override;

	virtual const FMetaballIntermediateTextures* GetCachedIntermediateTextures() const override
	{
		return CachedIntermediateTextures.IsValid() ? &CachedIntermediateTextures : nullptr;
	}

private:
	FMetaballIntermediateTextures CachedIntermediateTextures;

	TRefCountPtr<IPooledRenderTarget> PrevAccumulatedFlowRT;

	FMatrix PrevViewProjectionMatrix = FMatrix::Identity;

	bool bHasPrevFrameData = false;
};