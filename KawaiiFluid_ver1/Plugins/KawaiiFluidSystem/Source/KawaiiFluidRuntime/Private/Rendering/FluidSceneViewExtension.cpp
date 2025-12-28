// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidSceneViewExtension.h"

#include "FluidDepthPass.h"
#include "FluidNormalPass.h"
#include "FluidRendererSubsystem.h"
#include "FluidSmoothingPass.h"
#include "FluidThicknessPass.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "PixelShaderUtils.h"
#include "Rendering/FluidCompositeShaders.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Rendering/KawaiiFluidSSFRRenderer.h"
#include "Rendering/Composite/IFluidCompositePass.h"

static TRefCountPtr<IPooledRenderTarget> GFluidCompositeDebug_KeepAlive;

static void RenderFluidCompositePass_Internal(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	FRDGTextureRef FluidDepthTexture,
	FRDGTextureRef FluidNormalTexture,
	FRDGTextureRef FluidThicknessTexture,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output
)
{
	if (!FluidDepthTexture || !FluidNormalTexture || !FluidThicknessTexture || !SceneDepthTexture)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "FluidCompositePass");

	auto* PassParameters = GraphBuilder.AllocParameters<FFluidCompositePS::FParameters>();

	// 텍스처 바인딩
	PassParameters->FluidDepthTexture = FluidDepthTexture;
	PassParameters->FluidNormalTexture = FluidNormalTexture;
	PassParameters->FluidThicknessTexture = FluidThicknessTexture;
	PassParameters->SceneDepthTexture = SceneDepthTexture;
	PassParameters->SceneColorTexture = SceneColorTexture;
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->InputSampler = TStaticSamplerState<
		SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	PassParameters->InverseProjectionMatrix =
		FMatrix44f(View.ViewMatrices.GetInvProjectionMatrix());
	PassParameters->ProjectionMatrix = FMatrix44f(View.ViewMatrices.GetProjectionMatrix());
	PassParameters->ViewMatrix = FMatrix44f(View.ViewMatrices.GetViewMatrix());

	// Use RenderParams directly (passed from caller)
	PassParameters->FluidColor = RenderParams.FluidColor;
	PassParameters->FresnelStrength = RenderParams.FresnelStrength;
	PassParameters->RefractiveIndex = RenderParams.RefractiveIndex;
	PassParameters->AbsorptionCoefficient = RenderParams.AbsorptionCoefficient;
	PassParameters->SpecularStrength = RenderParams.SpecularStrength;
	PassParameters->SpecularRoughness = RenderParams.SpecularRoughness;
	PassParameters->EnvironmentLightColor = RenderParams.EnvironmentLightColor;

	// 배경 위에 그리기
	PassParameters->RenderTargets[0] = FRenderTargetBinding(
		Output.Texture, ERenderTargetLoadAction::ELoad);

	// 쉐이더 가져오기
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FFluidCompositeVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FFluidCompositePS> PixelShader(GlobalShaderMap);

	FIntRect ViewRect = View.UnscaledViewRect;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("FluidCompositeDraw"),
		PassParameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, PassParameters, ViewRect](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X,
			                       ViewRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(true, ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X,
			                          ViewRect.Max.Y);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.
				VertexDeclarationRHI; // [핵심] Input Layout 없음!
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Alpha Blending
			GraphicsPSOInit.BlendState = TStaticBlendState<
				CW_RGBA,
				BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				BO_Add, BF_Zero, BF_One
			>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				false, CF_Always>::GetRHI();

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(),
			                    *PassParameters);

			// 삼각형 1개로 화면 채우기 (VS에서 VertexID로 좌표 생성)
			RHICmdList.DrawPrimitive(0, 1, 1);
		});
}

// ==============================================================================
// Class Implementation
// ==============================================================================

FFluidSceneViewExtension::FFluidSceneViewExtension(const FAutoRegister& AutoRegister,
                                                   UFluidRendererSubsystem* InSubsystem)
	: FSceneViewExtensionBase(AutoRegister), Subsystem(InSubsystem)
{
}

FFluidSceneViewExtension::~FFluidSceneViewExtension()
{
}

void FFluidSceneViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass,
	const FSceneView& InView,
	FPostProcessingPassDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateLambda(
			[this](FRDGBuilder& GraphBuilder, const FSceneView& View,
			       const FPostProcessMaterialInputs& InInputs)
			{
				UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();

				// 유효성 검사
				bool bHasAnyModules = SubsystemPtr && SubsystemPtr->GetAllRenderingModules().Num() > 0;

				if (!SubsystemPtr || !SubsystemPtr->RenderingParameters.bEnableRendering || !bHasAnyModules)
				{
					return InInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
				}

				RDG_EVENT_SCOPE(GraphBuilder, "KawaiiFluidRendering");

				// ============================================
				// Batch renderers by LocalParameters (New Architecture only)
				// Separate batches by rendering mode
				// ============================================
				TMap<FFluidRenderingParameters, TArray<UKawaiiFluidSSFRRenderer*>> CustomBatches;
				TMap<FFluidRenderingParameters, TArray<UKawaiiFluidSSFRRenderer*>> GBufferBatches;

				const TArray<UKawaiiFluidRenderingModule*>& Modules = SubsystemPtr->GetAllRenderingModules();
				for (UKawaiiFluidRenderingModule* Module : Modules)
				{
					if (!Module) continue;

					UKawaiiFluidSSFRRenderer* SSFRRenderer = Module->GetSSFRRenderer();
					if (SSFRRenderer && SSFRRenderer->IsRenderingActive())
					{
						const FFluidRenderingParameters& Params = SSFRRenderer->GetLocalParameters();

						// Route to appropriate batch based on rendering mode
						if (Params.SSFRMode == ESSFRRenderingMode::Custom)
						{
							CustomBatches.FindOrAdd(Params).Add(SSFRRenderer);
						}
						else if (Params.SSFRMode == ESSFRRenderingMode::GBuffer)
						{
							GBufferBatches.FindOrAdd(Params).Add(SSFRRenderer);
						}
					}
				}

				// Check if we have any renderers
				if (CustomBatches.Num() == 0 && GBufferBatches.Num() == 0)
				{
					return InInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
				}

				// Scene Depth 가져오기
				FRDGTextureRef SceneDepthTexture = nullptr;
				if (InInputs.SceneTextures.SceneTextures)
				{
					SceneDepthTexture = InInputs.SceneTextures.SceneTextures->GetContents()->SceneDepthTexture;
				}

				// Composite Setup (공통)
				FScreenPassTexture SceneColorInput = FScreenPassTexture(
					InInputs.GetInput(EPostProcessMaterialInput::SceneColor));
				if (!SceneColorInput.IsValid())
				{
					return InInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
				}

				// Output Target 결정
				FScreenPassRenderTarget Output = InInputs.OverrideOutput;
				if (!Output.IsValid())
				{
					Output = FScreenPassRenderTarget::CreateFromInput(
						GraphBuilder, SceneColorInput, View.GetOverwriteLoadAction(),
						TEXT("FluidCompositeOutput"));
				}

				// SceneColor 복사
				if (SceneColorInput.Texture != Output.Texture)
				{
					AddDrawTexturePass(GraphBuilder, View, SceneColorInput, Output);
				}

				// ============================================
				// Custom Mode Batched Rendering
				// ============================================
				for (auto& Batch : CustomBatches)
				{
					const FFluidRenderingParameters& BatchParams = Batch.Key;
					const TArray<UKawaiiFluidSSFRRenderer*>& Renderers = Batch.Value;

					RDG_EVENT_SCOPE(GraphBuilder, "FluidBatch");

					// Calculate average particle radius for this batch
					float AverageParticleRadius = 10.0f;
					float TotalRadius = 0.0f;
					int ValidCount = 0;

					for (UKawaiiFluidSSFRRenderer* Renderer : Renderers)
					{
						TotalRadius += Renderer->GetCachedParticleRadius();
						ValidCount++;
					}

					if (ValidCount > 0)
					{
						AverageParticleRadius = TotalRadius / ValidCount;
					}

					// Use BatchParams for rendering parameters
					float BlurRadius = static_cast<float>(BatchParams.BilateralFilterRadius);
					float DepthFalloff = AverageParticleRadius * 0.7f;  // Dynamic calculation
					int32 NumIterations = 3;  // Hardcoded

					// Depth Pass (batched - only render particles from this batch)
					FRDGTextureRef BatchDepthTexture = nullptr;
					RenderFluidDepthPass(GraphBuilder, View, Renderers, SceneDepthTexture, BatchDepthTexture);

					if (BatchDepthTexture)
					{
						// Smoothing Pass
						FRDGTextureRef BatchSmoothedDepthTexture = nullptr;
						RenderFluidSmoothingPass(GraphBuilder, View, BatchDepthTexture, BatchSmoothedDepthTexture,
						                         BlurRadius, DepthFalloff, NumIterations);

						if (BatchSmoothedDepthTexture)
						{
							// Normal Pass
							FRDGTextureRef BatchNormalTexture = nullptr;
							RenderFluidNormalPass(GraphBuilder, View, BatchSmoothedDepthTexture, BatchNormalTexture);

							// Thickness Pass (batched - only render particles from this batch)
							FRDGTextureRef BatchThicknessTexture = nullptr;
							RenderFluidThicknessPass(GraphBuilder, View, Renderers, BatchThicknessTexture);

							if (BatchNormalTexture && BatchThicknessTexture)
							{
								// Composite Pass - Use appropriate composite pass implementation
								// Get composite pass from first renderer (all renderers in batch share same params/mode)
								if (Renderers.Num() > 0 && Renderers[0]->GetCompositePass())
								{
									FFluidIntermediateTextures IntermediateTextures;
									IntermediateTextures.SmoothedDepthTexture = BatchSmoothedDepthTexture;
									IntermediateTextures.NormalTexture = BatchNormalTexture;
									IntermediateTextures.ThicknessTexture = BatchThicknessTexture;

									Renderers[0]->GetCompositePass()->RenderComposite(
										GraphBuilder,
										View,
										BatchParams,
										IntermediateTextures,
										SceneDepthTexture,
										SceneColorInput.Texture,
										Output
									);
								}
							}
						}
					}
				}

				// ============================================
				// G-Buffer Mode Warning (Not Yet Implemented)
				// ============================================
				if (GBufferBatches.Num() > 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("G-Buffer mode renderers detected but not yet supported in Tonemap pass. Count: %d"), GBufferBatches.Num());
					UE_LOG(LogTemp, Warning, TEXT("G-Buffer mode requires implementation in MotionBlur pass (pre-lighting). See IMPLEMENTATION_GUIDE.md"));
				}

				// TODO (TEAM MEMBER): Implement G-Buffer mode rendering in MotionBlur pass
				// G-Buffer mode should write to GBuffer BEFORE lighting (pre-lighting stage)
				// This allows Lumen/VSM to process the fluid surface
				//
				// Implementation steps:
				// 1. Add MotionBlur pass subscription (similar to Tonemap above)
				// 2. Process GBufferBatches in MotionBlur callback
				// 3. Run Depth/Smoothing/Normal/Thickness passes (same as Custom mode)
				// 4. Call CompositePass->RenderComposite() which writes to GBuffer
				// 5. Test with Lumen reflections and VSM shadows

				// Debug Keep Alive
				GraphBuilder.QueueTextureExtraction(Output.Texture, &GFluidCompositeDebug_KeepAlive);

				return FScreenPassTexture(Output);
			}
		));
	}
}
