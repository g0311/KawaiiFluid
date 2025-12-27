// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidSmoothingPass.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"
#include "ShaderCore.h"
#include "PipelineStateCache.h"
#include "CommonRenderResources.h"

//=============================================================================
// Bilateral Blur Compute Shader (간단하게 Compute로 변경)
//=============================================================================

class FFluidBilateralBlurCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidBilateralBlurCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidBilateralBlurCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER(FVector2f, TextureSize)
		SHADER_PARAMETER(FIntPoint, BlurDirection)
		SHADER_PARAMETER(float, BlurRadius)
		SHADER_PARAMETER(float, BlurDepthFalloff)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFluidBilateralBlurCS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidSmoothing.usf", "BilateralBlurCS",
                        SF_Compute);

//=============================================================================
// Smoothing Pass Implementation
//=============================================================================

void RenderFluidSmoothingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputDepthTexture,
	FRDGTextureRef& OutSmoothedDepthTexture,
	float BlurRadius,
	float DepthFalloff,
	int32 NumIterations)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FluidSmoothingPass (Multi-Iteration)");
	check(InputDepthTexture);

	// Clamp iterations to reasonable range
	NumIterations = FMath::Clamp(NumIterations, 1, 5);

	FIntPoint TextureSize = InputDepthTexture->Desc.Extent;

	FRDGTextureDesc IntermediateDesc = FRDGTextureDesc::Create2D(
		TextureSize,
		PF_R32_FLOAT,
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFluidBilateralBlurCS> ComputeShader(GlobalShaderMap);

	// Current input/output for iteration loop
	FRDGTextureRef CurrentInput = InputDepthTexture;

	//=============================================================================
	// Multiple Iterations (NVIDIA Flex style)
	//=============================================================================
	for (int32 Iteration = 0; Iteration < NumIterations; ++Iteration)
	{
		// Create textures for this iteration
		FRDGTextureRef HorizontalOutput = GraphBuilder.CreateTexture(
			IntermediateDesc, TEXT("FluidDepthHorizontal"));

		FRDGTextureRef VerticalOutput = GraphBuilder.CreateTexture(
			IntermediateDesc, TEXT("FluidDepthVertical"));

		// NVIDIA Flex: Use consistent parameters across iterations
		// Multiple passes naturally fill gaps without scaling
		float IterationBlurRadius = BlurRadius;
		float IterationDepthFalloff = DepthFalloff;

		//=============================================================================
		// Horizontal Blur
		//=============================================================================
		{
			auto* PassParameters = GraphBuilder.AllocParameters<FFluidBilateralBlurCS::FParameters>();

			PassParameters->InputTexture = CurrentInput;
			PassParameters->TextureSize = FVector2f(TextureSize.X, TextureSize.Y);
			PassParameters->BlurDirection = FIntPoint(1, 0);
			PassParameters->BlurRadius = IterationBlurRadius;
			PassParameters->BlurDepthFalloff = IterationDepthFalloff;
			PassParameters->OutputTexture = GraphBuilder.CreateUAV(HorizontalOutput);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Iteration%d_HorizontalBlur", Iteration),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(TextureSize, 8));
		}

		//=============================================================================
		// Vertical Blur
		//=============================================================================
		{
			auto* PassParameters = GraphBuilder.AllocParameters<FFluidBilateralBlurCS::FParameters>();

			PassParameters->InputTexture = HorizontalOutput;
			PassParameters->TextureSize = FVector2f(TextureSize.X, TextureSize.Y);
			PassParameters->BlurDirection = FIntPoint(0, 1);
			PassParameters->BlurRadius = IterationBlurRadius;
			PassParameters->BlurDepthFalloff = IterationDepthFalloff;
			PassParameters->OutputTexture = GraphBuilder.CreateUAV(VerticalOutput);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Iteration%d_VerticalBlur", Iteration),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(TextureSize, 8));
		}

		// Use this iteration's output as next iteration's input
		CurrentInput = VerticalOutput;
	}

	// Final output
	OutSmoothedDepthTexture = CurrentInput;
}
