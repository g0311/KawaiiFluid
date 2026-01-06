// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidShadowProjection.h"
#include "Rendering/FluidShadowHistoryManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCore.h"

//=============================================================================
// Clear Atomic Buffer Compute Shader
//=============================================================================

class FFluidClearAtomicBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidClearAtomicBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidClearAtomicBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector2f, VSMTextureSize)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, DepthAtomicBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFluidClearAtomicBufferCS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidShadowProjection.usf",
                        "ClearAtomicBufferCS",
                        SF_Compute);

//=============================================================================
// Project Fluid Shadow Compute Shader
//=============================================================================

class FFluidProjectShadowCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidProjectShadowCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidProjectShadowCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, HistoryDepthTexture)
		SHADER_PARAMETER(FMatrix44f, HistoryInvViewProjectionMatrix)
		SHADER_PARAMETER(FMatrix44f, LightViewProjectionMatrix)
		SHADER_PARAMETER(FVector2f, HistoryTextureSize)
		SHADER_PARAMETER(FVector2f, VSMTextureSize)
		SHADER_PARAMETER(float, NearPlane)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, DepthAtomicBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFluidProjectShadowCS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidShadowProjection.usf",
                        "ProjectFluidShadowCS",
                        SF_Compute);

//=============================================================================
// Finalize VSM Compute Shader
//=============================================================================

class FFluidFinalizeVSMCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidFinalizeVSMCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidFinalizeVSMCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector2f, VSMTextureSize)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, DepthAtomicBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, VSMTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFluidFinalizeVSMCS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidShadowProjection.usf",
                        "FinalizeVSMCS",
                        SF_Compute);

//=============================================================================
// Render Function Implementation
//=============================================================================

/**
 * @brief Project fluid depth from previous frame into light space for VSM shadow generation.
 * @param GraphBuilder RDG builder for pass registration.
 * @param View Current frame's scene view.
 * @param HistoryBuffer Previous frame's depth and matrix data.
 * @param Params Shadow projection parameters.
 * @param OutProjection Output shadow projection data.
 */
void RenderFluidShadowProjection(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidShadowHistoryBuffer& HistoryBuffer,
	const FFluidShadowProjectionParams& Params,
	FFluidShadowProjectionOutput& OutProjection)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FluidShadowProjection");

	// Validate input
	if (!HistoryBuffer.bIsValid || !HistoryBuffer.DepthTexture.IsValid())
	{
		OutProjection.bIsValid = false;
		return;
	}

	// Register history depth texture
	FRDGTextureRef HistoryDepthTexture = GraphBuilder.RegisterExternalTexture(
		HistoryBuffer.DepthTexture,
		TEXT("FluidShadowHistoryDepth"));

	if (!HistoryDepthTexture)
	{
		OutProjection.bIsValid = false;
		return;
	}

	FIntPoint HistorySize = HistoryDepthTexture->Desc.Extent;
	FIntPoint VSMSize = Params.VSMResolution;

	// Create atomic depth buffer (R32_UINT for InterlockedMin)
	FRDGTextureDesc AtomicBufferDesc = FRDGTextureDesc::Create2D(
		VSMSize,
		PF_R32_UINT,
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureRef DepthAtomicBuffer = GraphBuilder.CreateTexture(
		AtomicBufferDesc,
		TEXT("FluidShadowAtomicDepth"));

	// Create VSM output texture (RG32F: depth, depth²)
	FRDGTextureDesc VSMDesc = FRDGTextureDesc::Create2D(
		VSMSize,
		PF_G32R32F,
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);

	FRDGTextureRef VSMTexture = GraphBuilder.CreateTexture(
		VSMDesc,
		TEXT("FluidVSMShadow"));

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	//=========================================================================
	// Pass 1: Clear Atomic Buffer
	//=========================================================================
	{
		TShaderMapRef<FFluidClearAtomicBufferCS> ComputeShader(GlobalShaderMap);

		auto* PassParameters = GraphBuilder.AllocParameters<FFluidClearAtomicBufferCS::FParameters>();
		PassParameters->VSMTextureSize = FVector2f(VSMSize.X, VSMSize.Y);
		PassParameters->DepthAtomicBuffer = GraphBuilder.CreateUAV(DepthAtomicBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearAtomicBuffer"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(VSMSize, 8));
	}

	//=========================================================================
	// Pass 2: Project Fluid Shadow
	//=========================================================================
	{
		TShaderMapRef<FFluidProjectShadowCS> ComputeShader(GlobalShaderMap);

		const float NearPlaneValue = View.NearClippingDistance;

		auto* PassParameters = GraphBuilder.AllocParameters<FFluidProjectShadowCS::FParameters>();
		PassParameters->HistoryDepthTexture = HistoryDepthTexture;
		PassParameters->HistoryInvViewProjectionMatrix = HistoryBuffer.InvViewProjectionMatrix;
		PassParameters->LightViewProjectionMatrix = Params.LightViewProjectionMatrix;
		PassParameters->HistoryTextureSize = FVector2f(HistorySize.X, HistorySize.Y);
		PassParameters->VSMTextureSize = FVector2f(VSMSize.X, VSMSize.Y);
		PassParameters->NearPlane = NearPlaneValue;
		PassParameters->DepthAtomicBuffer = GraphBuilder.CreateUAV(DepthAtomicBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ProjectFluidShadow"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(HistorySize, 8));
	}

	//=========================================================================
	// Pass 3: Finalize VSM
	//=========================================================================
	{
		TShaderMapRef<FFluidFinalizeVSMCS> ComputeShader(GlobalShaderMap);

		auto* PassParameters = GraphBuilder.AllocParameters<FFluidFinalizeVSMCS::FParameters>();
		PassParameters->VSMTextureSize = FVector2f(VSMSize.X, VSMSize.Y);
		PassParameters->DepthAtomicBuffer = GraphBuilder.CreateUAV(DepthAtomicBuffer);
		PassParameters->VSMTexture = GraphBuilder.CreateUAV(VSMTexture);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FinalizeVSM"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(VSMSize, 8));
	}

	// Set output
	OutProjection.VSMTexture = VSMTexture;
	OutProjection.bIsValid = true;
}
