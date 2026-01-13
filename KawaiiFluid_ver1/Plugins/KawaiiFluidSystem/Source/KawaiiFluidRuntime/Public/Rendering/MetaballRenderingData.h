// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "ScreenPass.h"

/**
 * Intermediate textures produced by ScreenSpace pipeline
 * These are cached between PostBasePass and Tonemap passes
 */
struct FMetaballIntermediateTextures
{
	/** Smoothed depth texture (bilateral filtered) */
	FRDGTextureRef SmoothedDepthTexture = nullptr;

	/** View-space normal texture */
	FRDGTextureRef NormalTexture = nullptr;

	/** Accumulated thickness texture */
	FRDGTextureRef ThicknessTexture = nullptr;

	/** Screen-space velocity texture for flow effects (RG16F: velocity.xy) */
	FRDGTextureRef VelocityTexture = nullptr;

	/** Accumulated flow UV offset texture (RG16F: accumulated offset.xy) */
	FRDGTextureRef AccumulatedFlowTexture = nullptr;

	/** GBuffer textures (optional, for GBuffer shading mode) */
	FRDGTextureRef GBufferATexture = nullptr;
	FRDGTextureRef GBufferBTexture = nullptr;
	FRDGTextureRef GBufferCTexture = nullptr;
	FRDGTextureRef GBufferDTexture = nullptr;

	bool IsValid() const
	{
		return SmoothedDepthTexture != nullptr &&
		       NormalTexture != nullptr &&
		       ThicknessTexture != nullptr;
	}

	void Reset()
	{
		SmoothedDepthTexture = nullptr;
		NormalTexture = nullptr;
		ThicknessTexture = nullptr;
		VelocityTexture = nullptr;
		AccumulatedFlowTexture = nullptr;
		GBufferATexture = nullptr;
		GBufferBTexture = nullptr;
		GBufferCTexture = nullptr;
		GBufferDTexture = nullptr;
	}
};

/**
 * Anisotropy data for ellipsoid rendering
 * Precomputed by FluidAnisotropyCompute shader
 */
struct FAnisotropyData
{
	/** Whether to use anisotropic rendering */
	bool bUseAnisotropy = false;

	/** Anisotropy Axis1 buffer SRV: float4(axis.xyz, scale.w) */
	FRDGBufferSRVRef AnisotropyAxis1SRV = nullptr;

	/** Anisotropy Axis2 buffer SRV: float4(axis.xyz, scale.w) */
	FRDGBufferSRVRef AnisotropyAxis2SRV = nullptr;

	/** Anisotropy Axis3 buffer SRV: float4(axis.xyz, scale.w) */
	FRDGBufferSRVRef AnisotropyAxis3SRV = nullptr;

	bool IsValid() const
	{
		return bUseAnisotropy &&
			AnisotropyAxis1SRV != nullptr &&
			AnisotropyAxis2SRV != nullptr &&
			AnisotropyAxis3SRV != nullptr;
	}

	void Reset()
	{
		bUseAnisotropy = false;
		AnisotropyAxis1SRV = nullptr;
		AnisotropyAxis2SRV = nullptr;
		AnisotropyAxis3SRV = nullptr;
	}
};

