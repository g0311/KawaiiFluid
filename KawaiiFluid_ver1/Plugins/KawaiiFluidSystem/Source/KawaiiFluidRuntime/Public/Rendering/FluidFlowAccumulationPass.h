// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"

class FSceneView;

/**
 * @brief Parameters for flow accumulation pass
 */
struct FFlowAccumulationParams
{
	/** Scale factor for velocity contribution to flow */
	float VelocityScale = 1.0f;

	/** How quickly flow decays when velocity is zero (0 = no decay) */
	float FlowDecay = 0.0f;

	/** Maximum accumulated offset before wrapping (for seamless dual-phase) */
	float MaxFlowOffset = 10.0f;

	/** Current frame's inverse view-projection matrix (for world position reconstruction) */
	FMatrix InvViewProjectionMatrix = FMatrix::Identity;

	/** Current frame's inverse view matrix (for view->world transform) */
	FMatrix InvViewMatrix = FMatrix::Identity;

	/** Current frame's inverse projection matrix (for clip->view transform) */
	FMatrix InvProjectionMatrix = FMatrix::Identity;

	/** Previous frame's view-projection matrix (for temporal reprojection) */
	FMatrix PrevViewProjectionMatrix = FMatrix::Identity;
};

/**
 * @brief Accumulates screen-space velocity into UV offset for flow texture effects.
 *
 * This pass implements the "Accumulated Screen-Space Flow" technique where:
 * - Still water: offset stays constant (no texture movement)
 * - Flowing water: offset accumulates based on velocity (texture moves)
 *
 * @param GraphBuilder RDG builder
 * @param View Scene view
 * @param Params Flow accumulation parameters
 * @param VelocityTexture Current frame's screen-space velocity (from Depth pass, RG16F)
 * @param DepthTexture Fluid depth texture (for masking non-fluid areas)
 * @param PrevAccumulatedFlowTexture Previous frame's accumulated flow (nullptr for first frame)
 * @param OutAccumulatedFlowTexture Output accumulated flow texture (RG16F)
 */
void RenderFluidFlowAccumulationPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFlowAccumulationParams& Params,
	FRDGTextureRef VelocityTexture,
	FRDGTextureRef DepthTexture,
	FRDGTextureRef PrevAccumulatedFlowTexture,
	FRDGTextureRef& OutAccumulatedFlowTexture);
