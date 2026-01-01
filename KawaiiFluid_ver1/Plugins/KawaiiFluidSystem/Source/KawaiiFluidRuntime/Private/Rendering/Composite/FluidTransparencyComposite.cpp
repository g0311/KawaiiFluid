// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Composite/FluidTransparencyComposite.h"
#include "Rendering/Shaders/FluidTransparencyShaders.h"
#include "Rendering/FluidRenderingParameters.h"
#include "RenderGraphBuilder.h"
#include "ScreenPass.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "ScenePrivate.h"

void FFluidTransparencyComposite::RenderTransparency(
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
		UE_LOG(LogTemp, Warning, TEXT("FFluidTransparencyComposite: Missing required textures"));
		return;
	}

	if (!Output.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FFluidTransparencyComposite: Invalid output target"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "FluidTransparency");

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

	UE_LOG(LogTemp, Log, TEXT("FFluidTransparencyComposite: OutputRect=(%d,%d)-(%d,%d), GBufferRect=(%d,%d)-(%d,%d), OutputTexSize=(%.0f,%.0f), GBufferTexSize=(%.0f,%.0f)"),
		OutputRect.Min.X, OutputRect.Min.Y, OutputRect.Max.X, OutputRect.Max.Y,
		GBufferRect.Min.X, GBufferRect.Min.Y, GBufferRect.Max.X, GBufferRect.Max.Y,
		PassParameters->OutputTextureSize.X, PassParameters->OutputTextureSize.Y,
		PassParameters->GBufferTextureSize.X, PassParameters->GBufferTextureSize.Y);

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
		RDG_EVENT_NAME("FluidTransparencyDraw"),
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

			// Stencil test: only render where stencil == SlimeStencilRef (0x01)
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
			RHICmdList.SetStencilRef(0x01);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			// Draw fullscreen triangle
			RHICmdList.DrawPrimitive(0, 1, 1);
		});

	UE_LOG(LogTemp, Log, TEXT("FFluidTransparencyComposite: Transparency pass executed"));
}
