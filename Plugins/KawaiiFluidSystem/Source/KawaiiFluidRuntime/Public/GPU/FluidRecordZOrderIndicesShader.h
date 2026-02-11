// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// C++ Shader Binding for FluidRecordZOrderIndices.usf

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "GPU/GPUFluidParticle.h"

//=============================================================================
// Record Z-Order Indices Compute Shader
// Records each particle's array index after Z-Order sort (for debug visualization)
// Input: Particles (Z-Order sorted)
// Output: DebugZOrderIndices[ParticleID] → ZOrderArrayIndex
//=============================================================================

/**
 * @class FRecordZOrderIndicesCS
 * @brief Records each particle's array index after Z-Order sort.
 * 
 * Used for debug visualization to track particle IDs across different spatial layouts.
 * 
 * @param Particles Input Z-Order sorted particles.
 * @param DebugZOrderIndices Output buffer mapping ParticleID to its array index.
 * @param ParticleCount Number of particles to process.
 */
class FRecordZOrderIndicesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRecordZOrderIndicesCS);
	SHADER_USE_PARAMETER_STRUCT(FRecordZOrderIndicesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, DebugZOrderIndices)
		SHADER_PARAMETER(int32, ParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};
