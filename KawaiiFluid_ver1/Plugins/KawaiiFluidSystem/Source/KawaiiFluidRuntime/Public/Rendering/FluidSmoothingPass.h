// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"

class FSceneView;
class UFluidRendererSubsystem;

/**
 * Bilateral Gaussian Blur for Fluid Depth Smoothing
 *
 * Applies separable bilateral filter (horizontal + vertical passes)
 * to smooth the depth buffer while preserving sharp edges.
 */
void RenderFluidSmoothingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputDepthTexture,
	FRDGTextureRef& OutSmoothedDepthTexture,
	float BlurRadius = 5.0f,
	float DepthFalloff = 0.05f,
	int32 NumIterations = 3);

/**
 * Narrow-Range Filter for Fluid Depth Smoothing (Truong & Yuksel, i3D 2018)
 *
 * Uses hard threshold with dynamic range expansion instead of continuous
 * Gaussian range weighting. Better edge preservation than bilateral filter.
 *
 * @param FilterRadius  Spatial filter radius in pixels
 * @param ParticleRadius  Particle radius for threshold calculation
 */
void RenderFluidNarrowRangeSmoothingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputDepthTexture,
	FRDGTextureRef& OutSmoothedDepthTexture,
	float FilterRadius = 5.0f,
	float ParticleRadius = 10.0f,
	int32 NumIterations = 3);

/**
 * Simple Gaussian Blur for Fluid Thickness Smoothing
 *
 * Applies a simple Gaussian blur to the thickness buffer to smooth out
 * individual particle profiles. Unlike depth smoothing, this does not
 * use bilateral filtering since thickness values are additive.
 *
 * @param BlurRadius  Spatial blur radius in pixels
 * @param NumIterations  Number of blur iterations
 */
void RenderFluidThicknessSmoothingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputThicknessTexture,
	FRDGTextureRef& OutSmoothedThicknessTexture,
	float BlurRadius = 5.0f,
	int32 NumIterations = 2);
