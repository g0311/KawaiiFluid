// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"

/**
 * Fluid Depth 렌더링 Vertex Shader
 */
class FFluidDepthVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidDepthVS);
	SHADER_USE_PARAMETER_STRUCT(FFluidDepthVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<float3>, ParticlePositions)
		SHADER_PARAMETER_SRV(StructuredBuffer<float3>, ParticleVelocities)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER(FMatrix44f, ViewMatrix)
		SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
		SHADER_PARAMETER(FMatrix44f, ViewProjectionMatrix)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

/**
 * Fluid Depth 렌더링 Pixel Shader
 */
class FFluidDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FFluidDepthPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<float3>, ParticlePositions)
		SHADER_PARAMETER_SRV(StructuredBuffer<float3>, ParticleVelocities)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER(FMatrix44f, ViewMatrix)
		SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
		SHADER_PARAMETER(FMatrix44f, ViewProjectionMatrix)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
