// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "ScreenPass.h"

/**
 * @struct FMetaballIntermediateTextures
 * @brief Container for intermediate surface textures produced by the ScreenSpace rendering pipeline.
 * 
 * @param SmoothedDepthTexture Bilateral filtered depth buffer.
 * @param NormalTexture Reconstructed world-space normals.
 * @param ThicknessTexture Accumulated view-space thickness.
 * @param VelocityTexture Screen-space velocity buffer (RG16F).
 * @param OcclusionMaskTexture Binary mask for visible fluid (R8).
 * @param AccumulatedFlowTexture Temporal UV offset for flow map animation.
 * @param BackgroundDepthTexture Hardware depth buffer including scene and previous fluids.
 * @param GBufferATexture Optional GBuffer A component.
 * @param GBufferBTexture Optional GBuffer B component.
 * @param GBufferCTexture Optional GBuffer C component.
 * @param GBufferDTexture Optional GBuffer D component.
 */
struct FMetaballIntermediateTextures
{
	FRDGTextureRef SmoothedDepthTexture = nullptr;

	FRDGTextureRef NormalTexture = nullptr;

	FRDGTextureRef ThicknessTexture = nullptr;

	FRDGTextureRef VelocityTexture = nullptr;

	FRDGTextureRef OcclusionMaskTexture = nullptr;

	FRDGTextureRef AccumulatedFlowTexture = nullptr;

	FRDGTextureRef BackgroundDepthTexture = nullptr;

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
		OcclusionMaskTexture = nullptr;
		AccumulatedFlowTexture = nullptr;
		BackgroundDepthTexture = nullptr;
		GBufferATexture = nullptr;
		GBufferBTexture = nullptr;
		GBufferCTexture = nullptr;
		GBufferDTexture = nullptr;
	}
};

/**
 * @struct FAnisotropyData
 * @brief Container for precomputed anisotropic ellipsoid axis data.
 * 
 * @param bUseAnisotropy Whether anisotropic rendering is active.
 * @param AnisotropyAxis1SRV Major axis buffer SRV (float4).
 * @param AnisotropyAxis2SRV Intermediate axis buffer SRV (float4).
 * @param AnisotropyAxis3SRV Minor axis buffer SRV (float4).
 */
struct FAnisotropyData
{
	bool bUseAnisotropy = false;

	FRDGBufferSRVRef AnisotropyAxis1SRV = nullptr;

	FRDGBufferSRVRef AnisotropyAxis2SRV = nullptr;

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
