// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "ScreenPass.h"
#include "Rendering/FluidRenderingParameters.h"
#include "Rendering/MetaballRenderingData.h"

class FRDGBuilder;
class FSceneView;
class UKawaiiFluidMetaballRenderer;

/**
 * @class IKawaiiMetaballRenderingPipeline
 * @brief Interface for metaball rendering pipelines responsible for surface computation and shading.
 * 
 * Pipelines manage the generation of intermediate surface data (depth, normals, thickness) 
 * and apply final lighting/shading in a post-process pass.
 */
class IKawaiiMetaballRenderingPipeline
{
public:
	virtual ~IKawaiiMetaballRenderingPipeline() = default;

	/**
	 * @brief Prepare intermediate textures and buffers needed for rendering.
	 * 
	 * @param GraphBuilder RDG builder for pass registration.
	 * @param View Current scene view.
	 * @param RenderParams Global fluid rendering parameters.
	 * @param Renderers Array of active metaball renderers to process.
	 * @param SceneDepthTexture The existing scene depth texture for occlusion.
	 * @param GlobalDepthTexture Output unified depth texture for all batches.
	 * @param GlobalVelocityTexture Optional unified velocity texture.
	 * @param GlobalOcclusionMask Optional unified occlusion mask.
	 */
	virtual void PrepareRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef& GlobalDepthTexture,
		FRDGTextureRef GlobalVelocityTexture = nullptr,
		FRDGTextureRef GlobalOcclusionMask = nullptr) = 0;

	/**
	 * @brief Execute the final shading pass and output to the target.
	 * 
	 * @param GraphBuilder RDG builder for pass registration.
	 * @param View Current scene view.
	 * @param RenderParams Global fluid rendering parameters.
	 * @param Renderers Array of active metaball renderers.
	 * @param SceneDepthTexture The existing scene depth texture.
	 * @param GlobalDepthTexture Unified depth texture from the prepare pass.
	 * @param SceneColorTexture Source scene color texture.
	 * @param Output Final render target for the fluid composite.
	 */
	virtual void ExecuteRender(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef GlobalDepthTexture,
		FRDGTextureRef SceneColorTexture,
		FScreenPassRenderTarget Output) = 0;

	/**
	 * @brief Retrieve cached intermediate textures if available.
	 * @return Pointer to internal textures or nullptr.
	 */
	virtual const FMetaballIntermediateTextures* GetCachedIntermediateTextures() const { return nullptr; }

protected:
	/**
	 * @brief Utility to calculate the combined bounding box of a set of particles.
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

		const float Expansion = ParticleRadius + Margin;
		OutMin -= FVector3f(Expansion);
		OutMax += FVector3f(Expansion);
	}
};
