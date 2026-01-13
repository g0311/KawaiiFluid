// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "FluidSurfaceDecoration.h"

class FSceneView;

/**
 * Renders surface decoration effects on top of the fluid.
 * This pass applies foam, emissive, texture overlays, and flow-based effects.
 *
 * @param GraphBuilder  RDG builder
 * @param View  Scene view
 * @param Params  Surface decoration parameters
 * @param DepthTexture  Smoothed fluid depth texture (from SSFR)
 * @param NormalTexture  Fluid normal texture (from SSFR)
 * @param ThicknessTexture  Fluid thickness texture (from SSFR)
 * @param SceneColorTexture  Current scene color (fluid already composited)
 * @param VelocityMapTexture  Optional screen-space velocity texture (for flow/foam, from Depth pass)
 * @param AccumulatedFlowTexture  Optional accumulated flow UV offset (for velocity-based flow)
 * @param OutputViewRect  ViewRect where fluid was rendered in SceneColorTexture
 * @param OutDecoratedTexture  Output with decorations applied
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
	const FIntRect& OutputViewRect,
	FRDGTextureRef& OutDecoratedTexture);
