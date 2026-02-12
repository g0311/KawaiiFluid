// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "Simulation/Resources/GPUFluidParticle.h"

/**
 * @class FCompactStatsCS
 * @brief Basic mode stats compact compute shader (32 bytes output).
 * 
 * Extracts Position, ParticleID, SourceID, NeighborCount from full particle buffer.
 * 
 * @param InParticles Input particles buffer.
 * @param OutCompactStats Output buffer for basic particle stats.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FCompactStatsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCompactStatsCS);
	SHADER_USE_PARAMETER_STRUCT(FCompactStatsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, InParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCompactParticleStats>, OutCompactStats)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FCompactStatsExCS
 * @brief Extended mode stats compact compute shader (48 bytes output).
 * 
 * Extracts Position, ParticleID, Velocity, SourceID, NeighborCount.
 * 
 * @param InParticles Input particles buffer.
 * @param OutCompactStatsEx Output buffer for extended particle stats.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FCompactStatsExCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCompactStatsExCS);
	SHADER_USE_PARAMETER_STRUCT(FCompactStatsExCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, InParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCompactParticleStatsEx>, OutCompactStatsEx)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};
