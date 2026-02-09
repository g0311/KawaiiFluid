// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidFlowAccumulationPass.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RHIStaticStates.h"

//=============================================================================
// Shader Parameters
//=============================================================================

BEGIN_SHADER_PARAMETER_STRUCT(FFlowAccumulationParameters, )
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, VelocityTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, PrevAccumulatedFlow)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, DepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, PointClampSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, BilinearClampSampler)
	SHADER_PARAMETER(FVector2f, TextureSize)
	SHADER_PARAMETER(float, DeltaTime)
	SHADER_PARAMETER(float, FlowDecay)
	SHADER_PARAMETER(float, MaxFlowOffset)
	SHADER_PARAMETER(float, VelocityScale)
	SHADER_PARAMETER(FMatrix44f, InvViewProjectionMatrix)
	SHADER_PARAMETER(FMatrix44f, InvViewMatrix)
	SHADER_PARAMETER(FMatrix44f, InvProjectionMatrix)
	SHADER_PARAMETER(FMatrix44f, PrevViewProjectionMatrix)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, OutputAccumulatedFlow)
END_SHADER_PARAMETER_STRUCT()

//=============================================================================
// Compute Shader
//=============================================================================

class FFluidFlowAccumulationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidFlowAccumulationCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidFlowAccumulationCS, FGlobalShader);

	using FParameters = FFlowAccumulationParameters;

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

IMPLEMENT_GLOBAL_SHADER(FFluidFlowAccumulationCS, "/Plugin/KawaiiFluidSystem/Private/FluidFlowAccumulation.usf", "FlowAccumulationCS", SF_Compute);

//=============================================================================
// Pass Implementation
//=============================================================================

/**
 * @brief Accumulates screen-space velocity into UV offset for flow texture effects.
 *
 * This pass implements the "Accumulated Screen-Space Flow" technique where:
 * - Still water: offset stays constant (no texture movement)
 * - Flowing water: offset accumulates based on velocity (texture moves)
 *
 * @param GraphBuilder RDG builder.
 * @param View Scene view.
 * @param Params Flow accumulation parameters.
 * @param VelocityTexture Current frame's screen-space velocity (from Depth pass, RG16F).
 * @param DepthTexture Fluid depth texture (for masking non-fluid areas).
 * @param PrevAccumulatedFlowTexture Previous frame's accumulated flow (nullptr for first frame).
 * @param OutAccumulatedFlowTexture Output accumulated flow texture (RG16F).
 */
void RenderKawaiiFluidFlowAccumulationPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FKawaiiFluidAccumulationParams& Params,
	FRDGTextureRef VelocityTexture,
	FRDGTextureRef DepthTexture,
	FRDGTextureRef PrevAccumulatedFlowTexture,
	FRDGTextureRef& OutAccumulatedFlowTexture)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FluidFlowAccumulation");

	if (!VelocityTexture || !DepthTexture)
	{
		return;
	}

	// Get texture size from velocity texture
	FIntPoint TextureSize = VelocityTexture->Desc.Extent;

	// Create output accumulated flow texture (RG16F for 2D offset)
	FRDGTextureDesc AccumulatedFlowDesc = FRDGTextureDesc::Create2D(
		TextureSize,
		PF_G16R16F,
		FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)),
		TexCreate_ShaderResource | TexCreate_UAV);

	OutAccumulatedFlowTexture = GraphBuilder.CreateTexture(AccumulatedFlowDesc, TEXT("FluidAccumulatedFlow"));

	// If no previous frame data, create a cleared texture
	FRDGTextureRef PrevFlowTexture = PrevAccumulatedFlowTexture;
	if (!PrevFlowTexture)
	{
		// Create a zeroed texture for first frame
		PrevFlowTexture = GraphBuilder.CreateTexture(AccumulatedFlowDesc, TEXT("FluidAccumulatedFlowPrev"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PrevFlowTexture), FVector4f(0.0f, 0.0f, 0.0f, 0.0f));
	}

	// Setup shader parameters
	FFlowAccumulationParameters* PassParameters = GraphBuilder.AllocParameters<FFlowAccumulationParameters>();
	PassParameters->VelocityTexture = GraphBuilder.CreateSRV(VelocityTexture);
	PassParameters->PrevAccumulatedFlow = GraphBuilder.CreateSRV(PrevFlowTexture);
	PassParameters->DepthTexture = GraphBuilder.CreateSRV(DepthTexture);
	PassParameters->PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->TextureSize = FVector2f(TextureSize.X, TextureSize.Y);
	PassParameters->DeltaTime = View.Family->Time.GetDeltaWorldTimeSeconds();
	PassParameters->FlowDecay = Params.FlowDecay;
	PassParameters->MaxFlowOffset = Params.MaxFlowOffset;
	PassParameters->VelocityScale = Params.VelocityScale;
	PassParameters->InvViewProjectionMatrix = FMatrix44f(Params.InvViewProjectionMatrix);
	PassParameters->InvViewMatrix = FMatrix44f(Params.InvViewMatrix);
	PassParameters->InvProjectionMatrix = FMatrix44f(Params.InvProjectionMatrix);
	PassParameters->PrevViewProjectionMatrix = FMatrix44f(Params.PrevViewProjectionMatrix);
	PassParameters->OutputAccumulatedFlow = GraphBuilder.CreateUAV(OutAccumulatedFlowTexture);

	// Get shader
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFluidFlowAccumulationCS> ComputeShader(GlobalShaderMap);

	// Calculate dispatch size (8x8 thread groups)
	FIntVector GroupCount = FIntVector(
		FMath::DivideAndRoundUp(TextureSize.X, 8),
		FMath::DivideAndRoundUp(TextureSize.Y, 8),
		1);

	// Add compute pass
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("FlowAccumulationCS"),
		ComputeShader,
		PassParameters,
		GroupCount);
}
