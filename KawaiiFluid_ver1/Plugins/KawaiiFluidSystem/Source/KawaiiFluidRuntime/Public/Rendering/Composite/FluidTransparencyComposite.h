// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"
#include "ScreenPass.h"

class FSceneView;
struct FFluidRenderingParameters;
struct FFluidIntermediateTextures;

/**
 * Transparency composite pass for G-Buffer + Transparency hybrid rendering
 *
 * This pass runs AFTER the lighting pass (in Tonemap hook) to apply
 * refraction and transparency effects to slime regions that were
 * previously rendered to G-Buffer.
 *
 * Pipeline:
 * 1. G-Buffer Pass (PostRenderBasePassDeferred)
 *    - Writes slime surface to G-Buffer
 *    - Writes depth
 *    - Marks slime regions with Stencil = 0x01
 *
 * 2. Lighting Pass (Engine automatic)
 *    - Lumen GI, shadows, reflections applied to G-Buffer
 *
 * 3. Transparency Pass (This class, Tonemap hook)
 *    - Uses Stencil to identify slime regions
 *    - Reads lit SceneColor
 *    - Applies refraction offset
 *    - Applies Beer's Law absorption
 *    - Blends with Fresnel-based transparency
 */
class FFluidTransparencyComposite
{
public:
	FFluidTransparencyComposite() = default;
	~FFluidTransparencyComposite() = default;

	/**
	 * Render transparency pass
	 *
	 * @param GraphBuilder RDG builder
	 * @param View Scene view
	 * @param RenderParams Rendering parameters
	 * @param SceneDepthTexture Scene depth (with stencil)
	 * @param LitSceneColorTexture Lit scene color (after lighting pass)
	 * @param GBufferATexture G-Buffer A (normals) for refraction direction
	 * @param GBufferDTexture G-Buffer D (thickness in R channel from Ray Marching pass)
	 * @param Output Output render target
	 */
	void RenderTransparency(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FFluidRenderingParameters& RenderParams,
		FRDGTextureRef SceneDepthTexture,
		FRDGTextureRef LitSceneColorTexture,
		FRDGTextureRef GBufferATexture,
		FRDGTextureRef GBufferDTexture,
		FScreenPassRenderTarget Output);

	/** Stencil reference value for slime regions (must match G-Buffer pass) */
	static constexpr uint8 SlimeStencilRef = 0x01;
};
