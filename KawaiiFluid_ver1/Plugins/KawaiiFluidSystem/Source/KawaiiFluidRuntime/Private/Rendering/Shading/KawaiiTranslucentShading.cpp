// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Shading/KawaiiTranslucentShading.h"
#include "Rendering/Shaders/FluidRayMarchGBufferShaders.h"
#include "Rendering/Shaders/FluidTransparencyShaders.h"
#include "Rendering/FluidRenderingParameters.h"
#include "RenderGraphBuilder.h"
#include "ScreenPass.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "ScenePrivate.h"

// Stencil reference value for Translucent mode
static constexpr uint8 TranslucentStencilRef = 0x01;

void FKawaiiTranslucentShading::RenderForScreenSpacePipeline(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const FMetaballIntermediateTextures& IntermediateTextures,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output)
{
	// Translucent mode only supports RayMarching pipeline
	UE_LOG(LogTemp, Warning, TEXT("FKawaiiTranslucentShading::RenderForScreenSpacePipeline - ScreenSpace not supported for Translucent mode"));
}

void FKawaiiTranslucentShading::RenderForRayMarchingPipeline(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	FRDGBufferSRVRef ParticleBufferSRV,
	int32 ParticleCount,
	float ParticleRadius,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output)
{
	// Validate inputs
	if (!ParticleBufferSRV || ParticleCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiTranslucentShading: No particles to render"));
		return;
	}

	if (!SceneDepthTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiTranslucentShading: Missing scene depth texture"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MetaballShading_RayMarching_Translucent_GBufferWrite");

	// Get GBuffer textures from the view
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	const FSceneTextures& SceneTextures = ViewInfo.GetSceneTextures();

	FRDGTextureRef GBufferATexture = SceneTextures.GBufferA;
	FRDGTextureRef GBufferBTexture = SceneTextures.GBufferB;
	FRDGTextureRef GBufferCTexture = SceneTextures.GBufferC;
	FRDGTextureRef GBufferDTexture = SceneTextures.GBufferD;

	if (!GBufferATexture || !GBufferBTexture || !GBufferCTexture || !GBufferDTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("FKawaiiTranslucentShading: Missing GBuffer textures"));
		return;
	}

	auto* PassParameters = GraphBuilder.AllocParameters<FFluidRayMarchGBufferParameters>();

	// Particle data
	PassParameters->ParticlePositions = ParticleBufferSRV;
	PassParameters->ParticleCount = ParticleCount;
	PassParameters->ParticleRadius = ParticleRadius;

	// Ray marching parameters
	PassParameters->SDFSmoothness = RenderParams.SDFSmoothness;
	PassParameters->MaxRayMarchSteps = RenderParams.MaxRayMarchSteps;
	PassParameters->RayMarchHitThreshold = RenderParams.RayMarchHitThreshold;
	PassParameters->RayMarchMaxDistance = RenderParams.RayMarchMaxDistance;

	// Material parameters for GBuffer
	PassParameters->FluidBaseColor = FVector3f(RenderParams.FluidColor.R, RenderParams.FluidColor.G, RenderParams.FluidColor.B);
	PassParameters->Metallic = RenderParams.Metallic;
	PassParameters->Roughness = RenderParams.Roughness;
	PassParameters->AbsorptionCoefficient = RenderParams.AbsorptionCoefficient;

	// Scene depth texture
	PassParameters->FluidSceneDepthTex = SceneDepthTexture;
	PassParameters->FluidSceneTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// SDF Volume (if using optimization)
	if (IsUsingSDFVolume())
	{
		PassParameters->SDFVolumeTexture = SDFVolumeData.SDFVolumeTextureSRV;
		PassParameters->SDFVolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->SDFVolumeMin = SDFVolumeData.VolumeMin;
		PassParameters->SDFVolumeMax = SDFVolumeData.VolumeMax;
		PassParameters->SDFVolumeResolution = SDFVolumeData.VolumeResolution;
	}

	// SceneDepth UV mapping
	FIntRect ViewRect = ViewInfo.ViewRect;
	PassParameters->SceneViewRect = FVector2f(ViewRect.Width(), ViewRect.Height());
	PassParameters->SceneTextureSize = FVector2f(SceneDepthTexture->Desc.Extent.X, SceneDepthTexture->Desc.Extent.Y);

	// View matrices
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->InverseViewMatrix = FMatrix44f(View.ViewMatrices.GetInvViewMatrix());
	PassParameters->InverseProjectionMatrix = FMatrix44f(View.ViewMatrices.GetInvProjectionMatrix());
	PassParameters->ViewMatrix = FMatrix44f(View.ViewMatrices.GetViewMatrix());
	PassParameters->ProjectionMatrix = FMatrix44f(View.ViewMatrices.GetProjectionMatrix());
	PassParameters->ViewportSize = FVector2f(ViewRect.Width(), ViewRect.Height());

	// MRT: GBuffer A/B/C/D
	PassParameters->RenderTargets[0] = FRenderTargetBinding(GBufferATexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[1] = FRenderTargetBinding(GBufferBTexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[2] = FRenderTargetBinding(GBufferCTexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[3] = FRenderTargetBinding(GBufferDTexture, ERenderTargetLoadAction::ELoad);

	// Depth/Stencil binding - CRITICAL: Write stencil = 0x01 for Transparency pass
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
		SceneDepthTexture,
		ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthWrite_StencilWrite);

	// Get shaders with SDF Volume permutation
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FFluidRayMarchGBufferVS> VertexShader(GlobalShaderMap);

	FFluidRayMarchGBufferPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FUseSDFVolumeGBufferDim>(IsUsingSDFVolume());
	TShaderMapRef<FFluidRayMarchGBufferPS> PixelShader(GlobalShaderMap, PermutationVector);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("MetaballTranslucent_RayMarch_GBufferWrite"),
		PassParameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, PassParameters, ViewRect](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(true, ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Opaque blending for GBuffer write
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

			// Depth test + write, AND stencil write = 0x01 for Transparency pass
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				true, CF_DepthNearOrEqual,                 // Depth: write enabled, pass if near or equal
				true, CF_Always,                           // Front stencil: enabled, always pass
				SO_Keep, SO_Keep, SO_Replace,              // Stencil ops: keep/keep/replace (write on depth pass)
				false, CF_Always,                          // Back stencil: disabled
				SO_Keep, SO_Keep, SO_Keep,
				0xFF, 0xFF                                 // Read/write masks: full
			>::GetRHI();

			// Set stencil reference to mark translucent regions
			RHICmdList.SetStencilRef(TranslucentStencilRef);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *PassParameters);

			// Draw fullscreen triangle
			RHICmdList.DrawPrimitive(0, 1, 1);
		});

	UE_LOG(LogTemp, Log, TEXT("FKawaiiTranslucentShading: RayMarching GBuffer write executed (Stencil=0x%02X), ParticleCount=%d"),
		TranslucentStencilRef, ParticleCount);
}

void FKawaiiTranslucentShading::RenderPostLightingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef LitSceneColorTexture,
	FRDGTextureRef GBufferATexture,
	FRDGTextureRef GBufferDTexture,
	FScreenPassRenderTarget Output)
{
	// Validate inputs
	if (!LitSceneColorTexture || !SceneDepthTexture || !GBufferATexture || !GBufferDTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiTranslucentShading::RenderPostLightingPass: Missing required textures"));
		return;
	}

	if (!Output.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiTranslucentShading::RenderPostLightingPass: Invalid output target"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "TranslucentShading_PostLighting");

	auto* PassParameters = GraphBuilder.AllocParameters<FFluidTransparencyParameters>();

	// Lit scene color (input)
	PassParameters->LitSceneColorTexture = LitSceneColorTexture;
	PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// Scene depth (for stencil test)
	PassParameters->FluidSceneDepthTex = SceneDepthTexture;
	PassParameters->DepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// GBuffer A (normals for refraction)
	PassParameters->FluidGBufferATex = GBufferATexture;
	PassParameters->GBufferSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// GBuffer D (thickness in R channel from Ray Marching pass)
	PassParameters->FluidGBufferDTex = GBufferDTexture;

	// Transparency parameters
	PassParameters->RefractiveIndex = RenderParams.RefractiveIndex;
	PassParameters->RefractionStrength = 0.05f;  // Screen-space refraction strength
	PassParameters->Opacity = 0.7f;  // Base opacity
	PassParameters->FresnelStrength = RenderParams.FresnelStrength;
	PassParameters->TintColor = FVector3f(RenderParams.FluidColor.R, RenderParams.FluidColor.G, RenderParams.FluidColor.B);
	PassParameters->AbsorptionCoefficient = RenderParams.AbsorptionCoefficient;

	// Viewport info - Use Output.ViewRect for rendering, ViewInfo.ViewRect for GBuffer sampling
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FIntRect OutputRect = Output.ViewRect;      // PostProcessing output region
	FIntRect GBufferRect = ViewInfo.ViewRect;   // GBuffer rendering region (may differ due to Screen Percentage)

	// Output coordinates (for rendering)
	PassParameters->OutputViewRect = FVector2f(OutputRect.Width(), OutputRect.Height());
	PassParameters->OutputViewRectMin = FVector2f(OutputRect.Min.X, OutputRect.Min.Y);
	PassParameters->OutputTextureSize = FVector2f(
		Output.Texture->Desc.Extent.X,
		Output.Texture->Desc.Extent.Y);

	// GBuffer coordinates (for sampling - may be different resolution)
	PassParameters->GBufferViewRect = FVector2f(GBufferRect.Width(), GBufferRect.Height());
	PassParameters->GBufferViewRectMin = FVector2f(GBufferRect.Min.X, GBufferRect.Min.Y);
	PassParameters->GBufferTextureSize = FVector2f(
		GBufferATexture->Desc.Extent.X,
		GBufferATexture->Desc.Extent.Y);

	// SceneColor texture size
	PassParameters->SceneTextureSize = FVector2f(
		LitSceneColorTexture->Desc.Extent.X,
		LitSceneColorTexture->Desc.Extent.Y);

	// Viewport size (Output resolution)
	PassParameters->ViewportSize = FVector2f(OutputRect.Width(), OutputRect.Height());
	PassParameters->InverseViewportSize = FVector2f(1.0f / OutputRect.Width(), 1.0f / OutputRect.Height());

	// View uniforms
	PassParameters->View = View.ViewUniformBuffer;

	// Output render target (alpha blend over lit scene)
	PassParameters->RenderTargets[0] = FRenderTargetBinding(
		Output.Texture, ERenderTargetLoadAction::ELoad);

	// Depth/Stencil for stencil test (read stencil, no depth write)
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
		SceneDepthTexture,
		ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthRead_StencilRead);

	// Get shaders
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FFluidTransparencyVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FFluidTransparencyPS> PixelShader(GlobalShaderMap);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("TranslucentShading_Transparency"),
		PassParameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, PassParameters, OutputRect](FRHICommandList& RHICmdList)
		{
			// Use Output.ViewRect for rendering (PostProcessing output resolution)
			RHICmdList.SetViewport(
				OutputRect.Min.X, OutputRect.Min.Y, 0.0f,
				OutputRect.Max.X, OutputRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(
				true,
				OutputRect.Min.X, OutputRect.Min.Y,
				OutputRect.Max.X, OutputRect.Max.Y);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Opaque write (replace) - shader already blends lit slime with refracted background
			// Alpha blending would cause double rendering since SceneColor already has lit slime
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

			// Stencil test: only render where stencil == TranslucentStencilRef (0x01)
			// Depth: read only (no write)
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				false, CF_Always,                              // Depth: no write, always pass
				true, CF_Equal,                                // Front stencil: enable, pass if equal
				SO_Keep, SO_Keep, SO_Keep,                     // Stencil ops: keep all
				false, CF_Always,                              // Back stencil: disabled
				SO_Keep, SO_Keep, SO_Keep,
				0xFF, 0x00                                     // Read mask, write mask (read only)
			>::GetRHI();

			// Set stencil ref to match slime regions
			RHICmdList.SetStencilRef(TranslucentStencilRef);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			// Draw fullscreen triangle
			RHICmdList.DrawPrimitive(0, 1, 1);
		});

	UE_LOG(LogTemp, Log, TEXT("FKawaiiTranslucentShading: PostLighting transparency pass executed"));
}
