// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "FluidSurfaceDecoration.h"

class FSceneView;

/**
 * @brief Renders surface decoration effects (foam, emissive, flow) on top of the fluid.
 * 
 * @param GraphBuilder RDG builder.
 * @param View Current scene view.
 * @param Params Surface decoration parameters.
 * @param DepthTexture Smoothed fluid depth.
 * @param NormalTexture Reconstructed fluid normals.
 * @param ThicknessTexture Smoothed fluid thickness.
 * @param SceneColorTexture Background scene color.
 * @param VelocityMapTexture Screen-space velocity buffer.
 * @param AccumulatedFlowTexture Temporal flow offset texture.
 * @param OcclusionMaskTexture Mask for culling decoration in occluded areas.
 * @param OutputViewRect Screen rectangle for rendering.
 * @param OutDecoratedTexture Output: Final texture with all surface decorations.
 */
void RenderFluidSurfaceDecorationPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FSurfaceDecorationParams& Params,
	FRDGTextureRef DepthTexture,
	FRDGTextureRef NormalTexture,
	FRDGTextureRef ThicknessTexture,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef VelocityMapTexture,
	FRDGTextureRef AccumulatedFlowTexture,
	FRDGTextureRef OcclusionMaskTexture,
	const FIntRect& OutputViewRect,
	FRDGTextureRef& OutDecoratedTexture);
