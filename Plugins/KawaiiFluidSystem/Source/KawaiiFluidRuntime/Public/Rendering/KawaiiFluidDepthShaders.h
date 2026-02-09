// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"

//=============================================================================
// Anisotropy Permutation
//=============================================================================

class FUseAnisotropyDim : SHADER_PERMUTATION_BOOL("USE_ANISOTROPY");

/**
 * @struct FFluidDepthParameters
 * @brief Shared parameter structure for fluid depth rasterization passes.
 * 
 * @param ParticlePositions Buffer containing world-space particle positions.
 * @param ParticleVelocities Buffer containing particle velocities for screen-space flow.
 * @param RenderOffset Buffer for particle rendering offsets.
 * @param ParticleRadius The base physical radius of the fluid particles.
 * @param ViewMatrix Camera view matrix.
 * @param ProjectionMatrix Camera projection matrix.
 * @param ViewProjectionMatrix Combined camera view-projection matrix.
 * @param SceneDepthTexture The current hardware scene depth texture.
 * @param SceneDepthSampler Sampler for the scene depth texture.
 * @param SceneViewRect Dimensions of the current view rectangle.
 * @param SceneTextureSize Dimensions of the source scene texture.
 * @param AnisotropyAxis1 Major axis vector and scale for anisotropic ellipsoids.
 * @param AnisotropyAxis2 Intermediate axis vector and scale.
 * @param AnisotropyAxis3 Minor axis vector and scale.
 * @param IndirectArgsBuffer RDG buffer containing arguments for DrawPrimitiveIndirect.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FFluidDepthParameters, )
	//=============================================================================
	// Buffers
	//=============================================================================
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float3>, ParticlePositions)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float3>, ParticleVelocities)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float3>, RenderOffset)
	SHADER_PARAMETER(float, ParticleRadius)

	//=============================================================================
	// Matrices
	//=============================================================================
	SHADER_PARAMETER(FMatrix44f, ViewMatrix)
	SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
	SHADER_PARAMETER(FMatrix44f, ViewProjectionMatrix)

	//=============================================================================
	// Scene
	//=============================================================================
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)
	SHADER_PARAMETER(FVector2f, SceneViewRect)
	SHADER_PARAMETER(FVector2f, SceneTextureSize)

	//=============================================================================
	// Anisotropy
	//=============================================================================
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, AnisotropyAxis1)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, AnisotropyAxis2)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, AnisotropyAxis3)

	//=============================================================================
	// Indirect Arguments
	//=============================================================================
	RDG_BUFFER_ACCESS(IndirectArgsBuffer, ERHIAccess::IndirectArgs)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * @class FKawaiiFluidDepthVS
 * @brief Vertex shader for fluid particle depth rasterization (billboards or ellipsoids).
 */
class FKawaiiFluidDepthVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FKawaiiFluidDepthVS);
	SHADER_USE_PARAMETER_STRUCT(FKawaiiFluidDepthVS, FGlobalShader);

	using FParameters = FFluidDepthParameters;
	using FPermutationDomain = TShaderPermutationDomain<FUseAnisotropyDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

/**
 * @class FKawaiiFluidDepthPS
 * @brief Pixel shader for fluid depth, outputting linear depth, velocity, and occlusion.
 */
class FKawaiiFluidDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FKawaiiFluidDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FKawaiiFluidDepthPS, FGlobalShader);

	using FParameters = FFluidDepthParameters;
	using FPermutationDomain = TShaderPermutationDomain<FUseAnisotropyDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
