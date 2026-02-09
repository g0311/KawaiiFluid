// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"

class FSceneView;

/**
 * @struct FKawaiiFluidAccumulationParams
 * @brief Parameters for the screen-space flow accumulation pass, used to track temporal motion of fluid surfaces.
 * 
 * @param VelocityScale Multiplier for screen-space velocity contribution to flow offset.
 * @param FlowDecay Rate at which accumulated flow returns to zero in static regions (0 = no decay).
 * @param MaxFlowOffset Cap for accumulated offset to ensure seamless dual-phase texture wrapping.
 * @param InvViewProjectionMatrix Current frame's inverse view-projection matrix.
 * @param InvViewMatrix Current frame's inverse view matrix.
 * @param InvProjectionMatrix Current frame's inverse projection matrix.
 * @param PrevViewProjectionMatrix Previous frame's view-projection matrix for temporal reprojection.
 */
struct FKawaiiFluidAccumulationParams
{
	float VelocityScale = 1.0f;

	float FlowDecay = 0.0f;

	float MaxFlowOffset = 10.0f;

	FMatrix InvViewProjectionMatrix = FMatrix::Identity;

	FMatrix InvViewMatrix = FMatrix::Identity;

	FMatrix InvProjectionMatrix = FMatrix::Identity;

	FMatrix PrevViewProjectionMatrix = FMatrix::Identity;
};

/**
 * @brief Accumulates screen-space velocity into a temporal UV offset texture for flowing effects.
 */
void RenderKawaiiFluidFlowAccumulationPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FKawaiiFluidAccumulationParams& Params,
	FRDGTextureRef VelocityTexture,
	FRDGTextureRef DepthTexture,
	FRDGTextureRef PrevAccumulatedFlowTexture,
	FRDGTextureRef& OutAccumulatedFlowTexture);
