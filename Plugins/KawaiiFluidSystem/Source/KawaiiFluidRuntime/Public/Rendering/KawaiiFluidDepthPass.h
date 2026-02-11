// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRDGBuilder;
class FSceneView;
class FRDGTexture;
class UKawaiiFluidMetaballRenderer;
typedef FRDGTexture* FRDGTextureRef;

/**
 * @brief Fluid Depth rendering pass (Batched path).
 * 
 * Renders fluid particles from a list of renderers into linear depth, velocity, and occlusion mask textures.
 * 
 * @param GraphBuilder RDG builder for pass registration.
 * @param View Current scene view.
 * @param Renderers List of metaball renderers to process.
 * @param SceneDepthTexture Reference scene depth for Z-testing.
 * @param OutLinearDepthTexture Output: Normalized linear depth texture (R32F).
 * @param OutVelocityTexture Output: Screen-space velocity texture (RG16F).
 * @param OutOcclusionMaskTexture Output: Binary occlusion mask (R8).
 * @param OutHardwareDepth Output: Final hardware depth texture after fluid rendering.
 * @param bIncremental If true, the pass starts with existing scene depth instead of clearing.
 */
void RenderKawaiiFluidDepthPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef& OutLinearDepthTexture,
	FRDGTextureRef& OutVelocityTexture,
	FRDGTextureRef& OutOcclusionMaskTexture,
	FRDGTextureRef& OutHardwareDepth,
	bool bIncremental = false);
