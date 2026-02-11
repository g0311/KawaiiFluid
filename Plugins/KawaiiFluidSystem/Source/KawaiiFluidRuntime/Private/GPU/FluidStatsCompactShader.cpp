// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "GPU/FluidStatsCompactShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"

//=============================================================================
// Shader Implementation
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FCompactStatsCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidStatsCompact.usf",
	"CompactStatsCS", SF_Compute);

/**
 * @brief Check if basic stats compact shader permutation should be compiled.
 */
bool FCompactStatsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify basic stats compact shader compilation environment.
 */
void FCompactStatsCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FCompactStatsExCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidStatsCompact.usf",
	"CompactStatsExCS", SF_Compute);

/**
 * @brief Check if extended stats compact shader permutation should be compiled.
 */
bool FCompactStatsExCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify extended stats compact shader compilation environment.
 */
void FCompactStatsExCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}
