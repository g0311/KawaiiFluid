// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "IFluidCompositePass.h"

/**
 * G-Buffer rendering pass with Ray Marching SDF support
 *
 * Two rendering modes:
 * 1. SSFR-based: Uses SmoothedDepth/Normal/Thickness from SSFR pipeline (legacy)
 * 2. Ray Marching: Uses SDF Volume for smooth metaball surfaces (recommended)
 *
 * After G-Buffer write, Lumen/VSM will light the surface automatically.
 * Then FluidTransparencyComposite applies refraction/transparency in Tonemap pass.
 */
class FFluidGBufferComposite : public IFluidCompositePass
{
public:
	virtual ~FFluidGBufferComposite() = default;

	/**
	 * Legacy SSFR-based G-Buffer write
	 * Uses SmoothedDepthTexture, NormalTexture, ThicknessTexture from SSFR pipeline
	 */
	virtual void RenderComposite(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		const FFluidIntermediateTextures& IntermediateTextures,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef SceneColorTexture,
		FScreenPassRenderTarget Output) override;

	/**
	 * Ray Marching SDF → G-Buffer write (recommended for slime)
	 * Uses SDF Volume texture for smooth metaball surfaces
	 *
	 * @param GraphBuilder RDG builder
	 * @param View Scene view
	 * @param RenderParams Rendering parameters
	 * @param ParticleBufferSRV Particle positions (for fallback if no volume)
	 * @param ParticleCount Number of particles
	 * @param ParticleRadius Particle radius
	 * @param SDFVolumeSRV Pre-baked SDF 3D texture
	 * @param VolumeMin SDF volume world min bounds
	 * @param VolumeMax SDF volume world max bounds
	 * @param VolumeResolution SDF volume resolution
	 * @param SceneDepthTexture Scene depth for occlusion
	 * @param GBufferTextures G-Buffer textures to write to
	 */
	void RenderRayMarchToGBuffer(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		FRDGBufferSRVRef ParticleBufferSRV,
		int32 ParticleCount,
		float ParticleRadius,
		FRDGTextureSRVRef SDFVolumeSRV,
		const FVector3f& VolumeMin,
		const FVector3f& VolumeMax,
		const FIntVector& VolumeResolution,
		FRDGTextureRef SceneDepthTexture,
		const FFluidIntermediateTextures& GBufferTextures);

	virtual ESSFRRenderingMode GetRenderingMode() const override
	{
		return ESSFRRenderingMode::GBuffer;
	}

	/** Stencil reference value for slime regions */
	static constexpr uint8 SlimeStencilRef = 0x01;
};
