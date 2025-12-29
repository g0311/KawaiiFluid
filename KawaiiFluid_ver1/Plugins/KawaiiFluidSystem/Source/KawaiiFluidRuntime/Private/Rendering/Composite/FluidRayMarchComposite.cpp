// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Composite/FluidRayMarchComposite.h"
#include "Rendering/Shaders/FluidRayMarchShaders.h"
#include "Rendering/FluidRenderingParameters.h"
#include "RenderGraphBuilder.h"
#include "ScreenPass.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"

void FFluidRayMarchComposite::SetParticleData(
	FRDGBufferSRVRef InParticleBufferSRV,
	int32 InParticleCount,
	float InParticleRadius)
{
	ParticleBufferSRV = InParticleBufferSRV;
	ParticleCount = InParticleCount;
	ParticleRadius = InParticleRadius;
}

void FFluidRayMarchComposite::RenderComposite(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const FFluidIntermediateTextures& IntermediateTextures,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output)
{
	UE_LOG(LogTemp, Log, TEXT("FFluidRayMarchComposite::RenderComposite called - Particles: %d, Radius: %.2f"),
		ParticleCount, ParticleRadius);

	// Validate particle data
	if (!ParticleBufferSRV || ParticleCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FFluidRayMarchComposite: No particle data set"));
		return;
	}

	if (!SceneDepthTexture || !SceneColorTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FFluidRayMarchComposite: Missing scene textures (Depth: %p, Color: %p)"),
			SceneDepthTexture, SceneColorTexture);
		return;
	}

	if (!Output.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FFluidRayMarchComposite: Invalid output target"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("FFluidRayMarchComposite: All validations passed, rendering..."));

	RDG_EVENT_SCOPE(GraphBuilder, "FluidRayMarch");

	auto* PassParameters = GraphBuilder.AllocParameters<FFluidRayMarchPS::FParameters>();

	// Particle data
	PassParameters->ParticlePositions = ParticleBufferSRV;
	PassParameters->ParticleCount = ParticleCount;
	PassParameters->ParticleRadius = ParticleRadius;

	// Ray marching parameters
	PassParameters->SDFSmoothness = RenderParams.SDFSmoothness;
	PassParameters->MaxRayMarchSteps = RenderParams.MaxRayMarchSteps;
	PassParameters->RayMarchHitThreshold = RenderParams.RayMarchHitThreshold;
	PassParameters->RayMarchMaxDistance = RenderParams.RayMarchMaxDistance;

	// Appearance parameters
	PassParameters->FluidColor = RenderParams.FluidColor;
	PassParameters->FresnelStrength = RenderParams.FresnelStrength;
	PassParameters->RefractiveIndex = RenderParams.RefractiveIndex;
	PassParameters->AbsorptionCoefficient = RenderParams.AbsorptionCoefficient;
	PassParameters->SpecularStrength = RenderParams.SpecularStrength;
	PassParameters->SpecularRoughness = RenderParams.SpecularRoughness;
	PassParameters->EnvironmentLightColor = RenderParams.EnvironmentLightColor;

	// SSS parameters
	PassParameters->SSSIntensity = RenderParams.SSSIntensity;
	PassParameters->SSSColor = RenderParams.SSSColor;

	// Scene textures
	PassParameters->SceneDepthTexture = SceneDepthTexture;
	PassParameters->SceneColorTexture = SceneColorTexture;
	PassParameters->SceneTextureSampler = TStaticSamplerState<
		SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// View uniforms
	PassParameters->View = View.ViewUniformBuffer;

	// View matrices
	PassParameters->InverseViewMatrix = FMatrix44f(View.ViewMatrices.GetInvViewMatrix());
	PassParameters->InverseProjectionMatrix = FMatrix44f(View.ViewMatrices.GetInvProjectionMatrix());
	PassParameters->ViewMatrix = FMatrix44f(View.ViewMatrices.GetViewMatrix());
	PassParameters->ProjectionMatrix = FMatrix44f(View.ViewMatrices.GetProjectionMatrix());

	// Viewport size
	FIntRect ViewRect = View.UnscaledViewRect;
	PassParameters->ViewportSize = FVector2f(ViewRect.Width(), ViewRect.Height());

	// Light parameters are accessed directly from View uniform buffer in shader
	// (View.DirectionalLightDirection, View.DirectionalLightColor)

	// Render target (blend over existing scene)
	PassParameters->RenderTargets[0] = FRenderTargetBinding(
		Output.Texture, ERenderTargetLoadAction::ELoad);

	// Get shaders
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FFluidRayMarchVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FFluidRayMarchPS> PixelShader(GlobalShaderMap);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("FluidRayMarchDraw (Particles: %d)", ParticleCount),
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
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI =
				GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Alpha blending (premultiplied alpha)
			GraphicsPSOInit.BlendState = TStaticBlendState<
				CW_RGBA,
				BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				BO_Add, BF_Zero, BF_One
			>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			// Draw fullscreen triangle (VS generates coords from VertexID)
			RHICmdList.DrawPrimitive(0, 1, 1);
		});

	// Clear particle data for next frame
	ParticleBufferSRV = nullptr;
	ParticleCount = 0;
}
