// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRDGBuilder;
class FSceneView;
class FRDGTexture;
class UKawaiiFluidMetaballRenderer;
typedef FRDGTexture* FRDGTextureRef;

/**
 * @brief Fluid Thickness rendering pass (Batched path).
 * 
 * Accumulates the view-space thickness of fluid particles into a single-channel texture.
 * 
 * @param GraphBuilder RDG builder for pass registration.
 * @param View The current scene view.
 * @param Renderers List of metaball renderers to process.
 * @param SceneDepthTexture Background scene depth for depth-testing.
 * @param OutThicknessTexture Output: Accumulated thickness texture (R32F).
 */
void RenderKawaiiFluidThicknessPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef& OutThicknessTexture);
