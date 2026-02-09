// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"

class FSceneView;
class UKawaiiFluidRendererSubsystem;

/**
 * @struct FDistanceBasedSmoothingParams
 * @brief Distance-based dynamic smoothing parameters for scaling blur radius by pixel depth.
 * 
 * @param WorldScale Blur radius multiplier relative to particle screen size (0.5 to 5.0).
 * @param MinRadius Minimum blur radius in pixels (1 to 64).
 * @param MaxRadius Maximum blur radius in pixels (4 to 64, limited by LDS).
 */
struct FDistanceBasedSmoothingParams
{
	float WorldScale = 2.0f;

	int32 MinRadius = 4;

	int32 MaxRadius = 32;
};

/**
 * @brief Narrow-Range Filter for Fluid Depth Smoothing (Truong & Yuksel, i3D 2018).
 */
void RenderKawaiiFluidNarrowRangeSmoothingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputDepthTexture,
	FRDGTextureRef& OutSmoothedDepthTexture,
	float ParticleRadius = 10.0f,
	float ThresholdRatio = 3.0f,
	float ClampRatio = 1.0f,
	int32 NumIterations = 3,
	float GrazingBoost = 1.0f,
	const FDistanceBasedSmoothingParams& DistanceBasedParams = FDistanceBasedSmoothingParams());

/**
 * @brief Separable Gaussian Blur for Fluid Thickness Smoothing.
 */
void RenderKawaiiFluidThicknessSmoothingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputThicknessTexture,
	FRDGTextureRef& OutSmoothedThicknessTexture,
	float BlurRadius = 5.0f,
	int32 NumIterations = 2,
	bool bUseHalfRes = true);

/**
 * @brief Separable Gaussian Blur for Fluid Velocity Smoothing.
 */
void RenderKawaiiFluidVelocitySmoothingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputVelocityTexture,
	FRDGTextureRef& OutSmoothedVelocityTexture,
	float BlurRadius = 8.0f,
	int32 NumIterations = 1);
