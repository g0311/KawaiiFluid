// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphDefinitions.h"

/**
 * @brief Parameter structure for Transparency pass shaders
 *
 * Applied after lighting to add refraction/transparency effects to slime regions.
 * Uses Stencil buffer to identify slime regions marked in G-Buffer pass.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FFluidTransparencyParameters, )
	// Lit scene color (after Lumen/lighting pass)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LitSceneColorTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)

	// Scene depth
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidSceneDepthTex)
	SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)

	// GBuffer A (Normal)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidGBufferATex)
	SHADER_PARAMETER_SAMPLER(SamplerState, GBufferSampler)

	// GBuffer D (Thickness stored in R channel from Ray Marching pass)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidGBufferDTex)

	// Transparency parameters
	SHADER_PARAMETER(float, RefractiveIndex)
	SHADER_PARAMETER(float, RefractionStrength)
	SHADER_PARAMETER(float, Opacity)
	SHADER_PARAMETER(float, FresnelStrength)
	SHADER_PARAMETER(FVector3f, TintColor)
	SHADER_PARAMETER(float, AbsorptionCoefficient)

	// Viewport info
	SHADER_PARAMETER(FVector2f, ViewportSize)
	SHADER_PARAMETER(FVector2f, InverseViewportSize)

	// UV mapping - Output coordinates (PostProcessing output resolution)
	SHADER_PARAMETER(FVector2f, OutputViewRect)      // Output 렌더링 영역 크기
	SHADER_PARAMETER(FVector2f, OutputViewRectMin)   // Output ViewRect 시작점
	SHADER_PARAMETER(FVector2f, OutputTextureSize)   // Output 텍스처 크기

	// UV mapping - GBuffer coordinates (may be different resolution due to Screen Percentage)
	SHADER_PARAMETER(FVector2f, GBufferViewRect)     // GBuffer 렌더링 영역 크기
	SHADER_PARAMETER(FVector2f, GBufferViewRectMin)  // GBuffer ViewRect 시작점
	SHADER_PARAMETER(FVector2f, GBufferTextureSize)  // GBuffer 텍스처 크기

	// UV mapping - SceneColor (same as Output in most cases)
	SHADER_PARAMETER(FVector2f, SceneTextureSize)

	// View uniforms
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

	// Output
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * @brief Vertex shader for Transparency pass (fullscreen triangle)
 */
class FFluidTransparencyVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFluidTransparencyVS);
	SHADER_USE_PARAMETER_STRUCT(FFluidTransparencyVS, FGlobalShader);

	using FParameters = FFluidTransparencyParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

/**
 * @brief Pixel shader for applying transparency/refraction to slime regions
 *
 * Runs after lighting pass, reads lit SceneColor, applies refraction offset,
 * Beer's Law absorption, and Fresnel-based blending.
 */
class FFluidTransparencyPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFluidTransparencyPS);
	SHADER_USE_PARAMETER_STRUCT(FFluidTransparencyPS, FGlobalShader);

	using FParameters = FFluidTransparencyParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
