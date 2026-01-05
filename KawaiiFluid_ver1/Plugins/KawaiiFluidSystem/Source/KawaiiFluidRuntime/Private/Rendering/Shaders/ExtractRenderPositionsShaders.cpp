// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Shaders/ExtractRenderPositionsShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

IMPLEMENT_GLOBAL_SHADER(FExtractRenderPositionsCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidExtractRenderPositions.usf",
	"ExtractRenderPositionsCS",
	SF_Compute);

void FExtractRenderPositionsPassBuilder::AddExtractPositionsPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef RenderParticlesSRV,
	FRDGBufferUAVRef PositionsUAV,
	int32 ParticleCount)
{
	if (ParticleCount <= 0 || !RenderParticlesSRV || !PositionsUAV)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddExtractPositionsPass: Invalid parameters"));
		return;
	}

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	if (!ShaderMap)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddExtractPositionsPass: ShaderMap is null"));
		return;
	}

	TShaderMapRef<FExtractRenderPositionsCS> ComputeShader(ShaderMap);
	if (!ComputeShader.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddExtractPositionsPass: FExtractRenderPositionsCS shader not valid"));
		return;
	}

	FExtractRenderPositionsCS::FParameters* PassParameters =
		GraphBuilder.AllocParameters<FExtractRenderPositionsCS::FParameters>();

	PassParameters->RenderParticles = RenderParticlesSRV;
	PassParameters->Positions = PositionsUAV;
	PassParameters->ParticleCount = ParticleCount;

	uint32 NumGroups = FMath::DivideAndRoundUp(
		(uint32)ParticleCount,
		FExtractRenderPositionsCS::ThreadGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ExtractRenderPositions(%d particles)", ParticleCount),
		ComputeShader,
		PassParameters,
		FIntVector(NumGroups, 1, 1));
}
