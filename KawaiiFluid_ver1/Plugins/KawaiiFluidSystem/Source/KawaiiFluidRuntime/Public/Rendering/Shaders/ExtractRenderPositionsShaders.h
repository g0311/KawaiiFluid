// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphDefinitions.h"
#include "Core/KawaiiRenderParticle.h"

/**
 * @brief Compute shader to extract float3 positions from FKawaiiRenderParticle buffer
 *
 * Used for Spatial Hash building in rendering pipeline.
 * Converts FKawaiiRenderParticle (32 bytes) to float3 (12 bytes) for hash compatibility.
 */
class FExtractRenderPositionsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FExtractRenderPositionsCS);
	SHADER_USE_PARAMETER_STRUCT(FExtractRenderPositionsCS, FGlobalShader);

	static constexpr uint32 ThreadGroupSize = 256;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FKawaiiRenderParticle>, RenderParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, Positions)
		SHADER_PARAMETER(int32, ParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

/**
 * @brief Helper class for adding extract positions pass to RDG
 */
class KAWAIIFLUIDRUNTIME_API FExtractRenderPositionsPassBuilder
{
public:
	/**
	 * Add compute pass to extract positions from render particle buffer
	 *
	 * @param GraphBuilder - RDG builder
	 * @param RenderParticlesSRV - Input: FKawaiiRenderParticle buffer
	 * @param PositionsUAV - Output: float3 positions buffer
	 * @param ParticleCount - Number of particles
	 */
	static void AddExtractPositionsPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef RenderParticlesSRV,
		FRDGBufferUAVRef PositionsUAV,
		int32 ParticleCount);
};
