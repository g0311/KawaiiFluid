// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "ScreenPass.h"
#include "Rendering/FluidRenderingParameters.h"
#include "Rendering/MetaballRenderingData.h"

// Forward declarations
class FRDGBuilder;
class FSceneView;
class UKawaiiFluidMetaballRenderer;

/**
 * Interface for Metaball Rendering Pipelines
 *
 * A Pipeline handles surface computation (how the fluid surface is determined):
 * - ScreenSpace: Depth -> Smoothing -> Normal -> Thickness passes
 *
 * Each Pipeline provides two execution points:
 * - PrepareRender(): Generate intermediate data (depth, normals, thickness, etc.)
 * - ExecuteRender(): Apply PostProcess shading (custom lighting)
 *
 * All pipelines use PostProcess shading mode.
 */
class IKawaiiMetaballRenderingPipeline
{
public:
	virtual ~IKawaiiMetaballRenderingPipeline() = default;

	/**
	 * Prepare data for rendering (called at PrePostProcess timing)
	 * Generates intermediate textures/buffers needed by ExecuteRender.
	 *
	 * - ScreenSpace: Depth, Normal, Thickness textures
	 *
	 * @param GraphBuilder     RDG builder for pass registration
	 * @param View             Scene view for rendering
	 * @param RenderParams     Fluid rendering parameters
	 * @param Renderers        Array of renderers to process
	 * @param SceneDepthTexture Scene depth texture
	 */
	virtual void PrepareRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture) = 0;

	/**
	 * Execute rendering (called at PrePostProcess timing)
	 * Applies PostProcess shading using intermediate data from PrepareRender.
	 *
	 * All pipelines use custom lighting (Blinn-Phong, Fresnel, Beer's Law).
	 *
	 * NOTE: PrepareRender must be called before this.
	 *
	 * @param GraphBuilder     RDG builder for pass registration
	 * @param View             Scene view for rendering
	 * @param RenderParams     Fluid rendering parameters
	 * @param Renderers        Array of renderers to process
	 * @param SceneDepthTexture Scene depth texture
	 * @param SceneColorTexture Scene color texture
	 * @param Output           Final render target
	 */
	virtual void ExecuteRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef SceneColorTexture,
		FScreenPassRenderTarget Output) = 0;

	/**
	 * Get cached intermediate textures for shadow history storage.
	 * Only valid after PrepareForTonemap has been called.
	 *
	 * @return Pointer to cached intermediate textures, or nullptr if not available.
	 */
	virtual const FMetaballIntermediateTextures* GetCachedIntermediateTextures() const { return nullptr; }

protected:
	/**
	 * Utility: Calculate particle bounding box
	 *
	 * @param Positions Particle positions
	 * @param ParticleRadius Average particle radius
	 * @param Margin Additional margin to add
	 * @param OutMin Output minimum bounds
	 * @param OutMax Output maximum bounds
	 */
	static void CalculateParticleBoundingBox(
		const TArray<FVector3f>& Positions,
		float ParticleRadius,
		float Margin,
		FVector3f& OutMin,
		FVector3f& OutMax)
	{
		if (Positions.Num() == 0)
		{
			OutMin = FVector3f::ZeroVector;
			OutMax = FVector3f::ZeroVector;
			return;
		}

		OutMin = Positions[0];
		OutMax = Positions[0];

		for (const FVector3f& Pos : Positions)
		{
			OutMin = FVector3f::Min(OutMin, Pos);
			OutMax = FVector3f::Max(OutMax, Pos);
		}

		// Expand by particle radius + margin
		const float Expansion = ParticleRadius + Margin;
		OutMin -= FVector3f(Expansion);
		OutMax += FVector3f(Expansion);
	}
};
