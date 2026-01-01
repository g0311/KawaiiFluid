// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Composite/FluidGBufferComposite.h"
#include "Rendering/Shaders/FluidGBufferWriteShaders.h"
#include "Rendering/Shaders/FluidRayMarchGBufferShaders.h"
#include "Rendering/FluidRenderingParameters.h"
#include "RenderGraphBuilder.h"
#include "ScreenPass.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "ScenePrivate.h"

void FFluidGBufferComposite::RenderComposite(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const FFluidIntermediateTextures& IntermediateTextures,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output)
{
	// Validate input textures
	if (!IntermediateTextures.SmoothedDepthTexture ||
		!IntermediateTextures.NormalTexture ||
		!IntermediateTextures.ThicknessTexture ||
		!SceneDepthTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FFluidGBufferComposite: Missing input textures"));
		return;
	}

	// Validate GBuffer textures
	if (!IntermediateTextures.GBufferATexture ||
		!IntermediateTextures.GBufferBTexture ||
		!IntermediateTextures.GBufferCTexture ||
		!IntermediateTextures.GBufferDTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("FFluidGBufferComposite: Missing GBuffer textures!"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "FluidGBufferWrite");

	auto* PassParameters = GraphBuilder.AllocParameters<FFluidGBufferWriteParameters>();

	// Texture bindings
	PassParameters->SmoothedDepthTexture = IntermediateTextures.SmoothedDepthTexture;
	PassParameters->NormalTexture = IntermediateTextures.NormalTexture;
	PassParameters->ThicknessTexture = IntermediateTextures.ThicknessTexture;
	PassParameters->FluidSceneDepthTexture = SceneDepthTexture;

	// Samplers
	PassParameters->PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// Material parameters
	PassParameters->FluidBaseColor = FVector3f(RenderParams.FluidColor.R, RenderParams.FluidColor.G, RenderParams.FluidColor.B);
	PassParameters->Metallic = RenderParams.Metallic;
	PassParameters->Roughness = RenderParams.Roughness;
	PassParameters->SubsurfaceOpacity = RenderParams.SubsurfaceOpacity;
	PassParameters->AbsorptionCoefficient = RenderParams.AbsorptionCoefficient;

	// View uniforms
	PassParameters->View = View.ViewUniformBuffer;

	// MRT: GBuffer A/B/C/D
	PassParameters->RenderTargets[0] = FRenderTargetBinding(
		IntermediateTextures.GBufferATexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[1] = FRenderTargetBinding(
		IntermediateTextures.GBufferBTexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[2] = FRenderTargetBinding(
		IntermediateTextures.GBufferCTexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[3] = FRenderTargetBinding(
		IntermediateTextures.GBufferDTexture, ERenderTargetLoadAction::ELoad);

	// Depth/Stencil binding (write custom depth)
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
		SceneDepthTexture,
		ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthWrite_StencilWrite);

	// Get shaders
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FFluidGBufferWriteVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FFluidGBufferWritePS> PixelShader(GlobalShaderMap);

	// GBuffer 모드에서는 Output이 DummyOutput (빈 FScreenPassRenderTarget)이므로
	// ViewInfo.ViewRect를 사용하여 SceneDepth 유효 영역과 일치시킴
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FIntRect ViewRect = ViewInfo.ViewRect;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("FluidGBufferWriteDraw"),
		PassParameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, PassParameters, ViewRect](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X,
			                       ViewRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(true, ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X,
			                          ViewRect.Max.Y);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Opaque blending for GBuffer write
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

			// Write depth AND stencil (mark slime region with stencil = 0x01)
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				true, CF_DepthNearOrEqual,                    // Depth: write, near or equal
				true, CF_Always,                               // Front face stencil: enable, always pass
				SO_Keep, SO_Keep, SO_Replace,                  // Stencil ops: fail=keep, depthFail=keep, pass=replace
				false, CF_Always,                              // Back face stencil: disabled
				SO_Keep, SO_Keep, SO_Keep,
				0xFF, 0xFF                                     // Read/Write mask
			>::GetRHI();

			// Set stencil reference value for slime regions
			RHICmdList.SetStencilRef(0x01);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(),
			                    *PassParameters);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(),
			                    *PassParameters);

			// Draw fullscreen triangle
			RHICmdList.DrawPrimitive(0, 1, 1);
		});

	UE_LOG(LogTemp, Log, TEXT("FFluidGBufferComposite: GBuffer write executed successfully"));
}

void FFluidGBufferComposite::RenderRayMarchToGBuffer(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	FRDGBufferSRVRef ParticleBufferSRV,
	int32 ParticleCount,
	float ParticleRadius,
	FRDGTextureSRVRef SDFVolumeSRV,
	const FVector3f& VolumeMin,
	const FVector3f& VolumeMax,
	const FIntVector& VolumeResolution,
	FRDGTextureRef SceneDepthTexture,
	const FFluidIntermediateTextures& GBufferTextures)
{
	// Validate G-Buffer textures
	if (!GBufferTextures.GBufferATexture ||
		!GBufferTextures.GBufferBTexture ||
		!GBufferTextures.GBufferCTexture ||
		!GBufferTextures.GBufferDTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("FFluidGBufferComposite::RenderRayMarchToGBuffer: Missing GBuffer textures!"));
		return;
	}

	if (!SceneDepthTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("FFluidGBufferComposite::RenderRayMarchToGBuffer: Missing SceneDepthTexture!"));
		return;
	}

	const bool bUseSDFVolume = SDFVolumeSRV != nullptr;

	RDG_EVENT_SCOPE(GraphBuilder, "FluidRayMarchGBuffer");

	auto* PassParameters = GraphBuilder.AllocParameters<FFluidRayMarchGBufferParameters>();

	// Particle data
	PassParameters->ParticlePositions = ParticleBufferSRV;
	PassParameters->ParticleCount = ParticleCount;
	PassParameters->ParticleRadius = ParticleRadius;

	// Ray Marching parameters
	PassParameters->SDFSmoothness = RenderParams.SDFSmoothness;
	PassParameters->MaxRayMarchSteps = RenderParams.MaxRayMarchSteps;
	PassParameters->RayMarchHitThreshold = RenderParams.RayMarchHitThreshold;
	PassParameters->RayMarchMaxDistance = RenderParams.RayMarchMaxDistance;

	// Material parameters
	PassParameters->FluidBaseColor = FVector3f(RenderParams.FluidColor.R, RenderParams.FluidColor.G, RenderParams.FluidColor.B);
	PassParameters->Metallic = RenderParams.Metallic;
	PassParameters->Roughness = RenderParams.Roughness;
	PassParameters->AbsorptionCoefficient = RenderParams.AbsorptionCoefficient;

	// Scene textures
	PassParameters->FluidSceneDepthTex = SceneDepthTexture;
	PassParameters->FluidSceneTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// SDF Volume (if using optimized path)
	if (bUseSDFVolume)
	{
		PassParameters->SDFVolumeTexture = SDFVolumeSRV;
		PassParameters->SDFVolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->SDFVolumeMin = VolumeMin;
		PassParameters->SDFVolumeMax = VolumeMax;
		PassParameters->SDFVolumeResolution = VolumeResolution;
	}

	// View matrices
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->InverseViewMatrix = FMatrix44f(View.ViewMatrices.GetInvViewMatrix());
	PassParameters->InverseProjectionMatrix = FMatrix44f(View.ViewMatrices.GetInvProjectionMatrix());
	PassParameters->ViewMatrix = FMatrix44f(View.ViewMatrices.GetViewMatrix());
	PassParameters->ProjectionMatrix = FMatrix44f(View.ViewMatrices.GetProjectionMatrix());

	// Viewport
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FIntRect ViewRect = ViewInfo.ViewRect;
	PassParameters->ViewportSize = FVector2f(ViewRect.Width(), ViewRect.Height());

	// SceneDepth UV mapping
	PassParameters->SceneViewRect = FVector2f(ViewRect.Width(), ViewRect.Height());
	PassParameters->SceneTextureSize = FVector2f(
		SceneDepthTexture->Desc.Extent.X,
		SceneDepthTexture->Desc.Extent.Y);

	// MRT: GBuffer A/B/C/D
	PassParameters->RenderTargets[0] = FRenderTargetBinding(
		GBufferTextures.GBufferATexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[1] = FRenderTargetBinding(
		GBufferTextures.GBufferBTexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[2] = FRenderTargetBinding(
		GBufferTextures.GBufferCTexture, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[3] = FRenderTargetBinding(
		GBufferTextures.GBufferDTexture, ERenderTargetLoadAction::ELoad);

	// Depth/Stencil binding
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
		SceneDepthTexture,
		ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthWrite_StencilWrite);

	// Get shaders with permutation
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FFluidRayMarchGBufferVS> VertexShader(GlobalShaderMap);

	FFluidRayMarchGBufferPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FUseSDFVolumeGBufferDim>(bUseSDFVolume);
	TShaderMapRef<FFluidRayMarchGBufferPS> PixelShader(GlobalShaderMap, PermutationVector);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("FluidRayMarchGBufferDraw (%s)", bUseSDFVolume ? TEXT("SDFVolume") : TEXT("Direct")),
		PassParameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, PassParameters, ViewRect](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(
				ViewRect.Min.X, ViewRect.Min.Y, 0.0f,
				ViewRect.Max.X, ViewRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(
				true,
				ViewRect.Min.X, ViewRect.Min.Y,
				ViewRect.Max.X, ViewRect.Max.Y);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Opaque blending for GBuffer write
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

			// Write depth AND stencil (mark slime region with stencil = 0x01)
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				true, CF_DepthNearOrEqual,
				true, CF_Always,
				SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always,
				SO_Keep, SO_Keep, SO_Keep,
				0xFF, 0xFF
			>::GetRHI();

			RHICmdList.SetStencilRef(SlimeStencilRef);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			// Draw fullscreen triangle
			RHICmdList.DrawPrimitive(0, 1, 1);
		});

	UE_LOG(LogTemp, Log, TEXT("FFluidGBufferComposite::RenderRayMarchToGBuffer: Ray March G-Buffer write complete (UseSDFVolume: %s, Particles: %d)"),
		bUseSDFVolume ? TEXT("true") : TEXT("false"), ParticleCount);
}
