// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "Rendering/FluidDensityGrid.h"

/**
 * @brief Vertex shader for fluid VSM depth rendering.
 *
 * Transforms bounding box vertices to light clip space and passes
 * world position for ray marching in pixel shader.
 */
class FFluidVSMDepthVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidVSMDepthVS);
	SHADER_USE_PARAMETER_STRUCT(FFluidVSMDepthVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLightClip)
		SHADER_PARAMETER(FVector3f, LightDirection)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

/**
 * @brief Pixel shader for fluid VSM depth rendering.
 *
 * Ray marches through the density grid to find fluid surface and
 * outputs the surface depth via SV_Depth for shadow mapping.
 */
class FFluidVSMDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidVSMDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FFluidVSMDepthPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, GridBoundsMin)
		SHADER_PARAMETER(FVector3f, GridBoundsMax)
		SHADER_PARAMETER(FVector3f, GridResolution)
		SHADER_PARAMETER(FVector3f, InvGridSize)
		SHADER_PARAMETER(float, SurfaceDensityThreshold)
		SHADER_PARAMETER(int32, MaxRayMarchSteps)
		SHADER_PARAMETER(FMatrix44f, WorldToLightClip)
		SHADER_PARAMETER(FVector3f, LightDirection)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D<float>, DensityGridTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DensityGridSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

/**
 * @brief Parameters for fluid VSM depth rendering.
 * @param DensityGrid 3D density grid texture.
 * @param GridConfig Grid configuration.
 * @param LightViewProjection Light's view-projection matrix.
 * @param LightDirection World-space light direction.
 */
struct FFluidVSMDepthRenderParams
{
	/** Density grid texture (registered with RDG). */
	FRDGTextureRef DensityGridTexture = nullptr;

	/** Grid configuration. */
	FFluidDensityGridConfig GridConfig;

	/** Light view-projection matrix. */
	FMatrix44f LightViewProjectionMatrix;

	/** Light direction in world space. */
	FVector3f LightDirection;

	/** Local to world transform for bounding box. */
	FMatrix44f LocalToWorld;

	/** Surface density threshold for ray marching. */
	float SurfaceDensityThreshold = 0.5f;

	/** Maximum ray march steps. */
	int32 MaxRayMarchSteps = 64;
};
