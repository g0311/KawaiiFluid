// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"

/**
 * @struct FFluidThicknessParameters
 * @brief Shared parameter structure for fluid thickness accumulation passes.
 * 
 * @param ParticlePositions Buffer containing world-space particle positions.
 * @param ParticleRadius The physical radius of fluid particles.
 * @param ViewMatrix Camera view matrix.
 * @param ProjectionMatrix Camera projection matrix.
 * @param ThicknessScale Multiplier for depth-based thickness accumulation.
 * @param SceneDepthTexture Current hardware scene depth for occlusion.
 * @param SceneDepthSampler Sampler for the scene depth texture.
 * @param SceneViewRect Dimensions of the current view rectangle.
 * @param SceneTextureSize Dimensions of the source scene texture.
 * @param IndirectArgsBuffer RDG buffer containing arguments for DrawPrimitiveIndirect.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FFluidThicknessParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float3>, ParticlePositions)
	SHADER_PARAMETER(float, ParticleRadius)
	SHADER_PARAMETER(FMatrix44f, ViewMatrix)
	SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
	SHADER_PARAMETER(float, ThicknessScale)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)
	SHADER_PARAMETER(FVector2f, SceneViewRect)
	SHADER_PARAMETER(FVector2f, SceneTextureSize)
	RDG_BUFFER_ACCESS(IndirectArgsBuffer, ERHIAccess::IndirectArgs)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * @class FKawaiiFluidThicknessVS
 * @brief Vertex shader for accumulating fluid thickness in view-space.
 */
class FKawaiiFluidThicknessVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FKawaiiFluidThicknessVS);
	SHADER_USE_PARAMETER_STRUCT(FKawaiiFluidThicknessVS, FGlobalShader);

	using FParameters = FFluidThicknessParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

/**
 * @class FKawaiiFluidThicknessPS
 * @brief Pixel shader for fluid thickness, outputting additive view-space depth coverage.
 */
class FKawaiiFluidThicknessPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FKawaiiFluidThicknessPS);
	SHADER_USE_PARAMETER_STRUCT(FKawaiiFluidThicknessPS, FGlobalShader);

	using FParameters = FFluidThicknessParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
