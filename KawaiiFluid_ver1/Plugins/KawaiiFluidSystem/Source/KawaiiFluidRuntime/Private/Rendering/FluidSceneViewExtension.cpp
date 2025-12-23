// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidSceneViewExtension.h"
#include "Rendering/FluidRendererSubsystem.h"
#include "Rendering/FluidDepthPass.h"
#include "Rendering/FluidSmoothingPass.h"
#include "Rendering/FluidNormalPass.h"
#include "Rendering/FluidThicknessPass.h"
#include "Rendering/IKawaiiFluidRenderable.h"
#include "Core/FluidSimulator.h"
#include "SceneView.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

FFluidSceneViewExtension::FFluidSceneViewExtension(const FAutoRegister& AutoRegister, UFluidRendererSubsystem* InSubsystem)
	: FSceneViewExtensionBase(AutoRegister)
	, Subsystem(InSubsystem)
{
	UE_LOG(LogTemp, Log, TEXT("FluidSceneViewExtension Created"));
}

FFluidSceneViewExtension::~FFluidSceneViewExtension()
{
	UE_LOG(LogTemp, Log, TEXT("FluidSceneViewExtension Destroyed"));
}

void FFluidSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	// Subsystem 유효성 검사
	UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();
	if (!SubsystemPtr)
	{
		return;
	}

	// 렌더링 비활성화 체크
	if (!SubsystemPtr->RenderingParameters.bEnableRendering)
	{
		return;
	}

	// ✅ 통합 API 사용 (레거시 API 대체)
	TArray<IKawaiiFluidRenderable*> Renderables = SubsystemPtr->GetAllRenderables();
	
	// ✅ SSFR 사용 객체가 있는지 체크
	bool bHasSSFRRenderables = false;
	for (IKawaiiFluidRenderable* Renderable : Renderables)
	{
		if (Renderable && Renderable->ShouldUseSSFR())
		{
			bHasSSFRRenderables = true;
			break;
		}
	}

	if (!bHasSSFRRenderables)
	{
		return;  // SSFR 사용하는 객체가 없으면 스킵
	}

	// 1. Depth Pass
	FRDGTextureRef DepthTexture = nullptr;
	RenderFluidDepthPass(GraphBuilder, View, SubsystemPtr, DepthTexture);

	if (!DepthTexture)
	{
		return;
	}

	// 2. Smoothing Pass
	FRDGTextureRef SmoothedDepthTexture = nullptr;
	RenderSmoothingPass(GraphBuilder, View, DepthTexture, SmoothedDepthTexture);

	if (!SmoothedDepthTexture)
	{
		return;
	}

	// 3. Normal Reconstruction Pass
	FRDGTextureRef NormalTexture = nullptr;
	RenderNormalPass(GraphBuilder, View, SmoothedDepthTexture, NormalTexture);

	// 4. Thickness Pass
	FRDGTextureRef ThicknessTexture = nullptr;
	RenderThicknessPass(GraphBuilder, View, ThicknessTexture);

	// 5. Final Shading Pass (TODO)
	// RenderShadingPass(GraphBuilder, View, Inputs);
}

void FFluidSceneViewExtension::RenderDepthPass(FRDGBuilder& GraphBuilder, const FSceneView& View)
{
	UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();
	if (!SubsystemPtr)
	{
		return;
	}

	FRDGTextureRef DepthTexture = nullptr;
	RenderFluidDepthPass(GraphBuilder, View, SubsystemPtr, DepthTexture);
}

void FFluidSceneViewExtension::RenderSmoothingPass(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef InputDepthTexture, FRDGTextureRef& OutSmoothedDepthTexture)
{
	UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();
	if (!SubsystemPtr || !InputDepthTexture)
	{
		return;
	}

	float BlurRadius = static_cast<float>(SubsystemPtr->RenderingParameters.BilateralFilterRadius);
	float DepthFalloff = SubsystemPtr->RenderingParameters.DepthThreshold;

	RenderFluidSmoothingPass(GraphBuilder, View, InputDepthTexture, OutSmoothedDepthTexture, BlurRadius, DepthFalloff);
}

void FFluidSceneViewExtension::RenderNormalPass(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef SmoothedDepthTexture, FRDGTextureRef& OutNormalTexture)
{
	if (!SmoothedDepthTexture)
	{
		return;
	}

	RenderFluidNormalPass(GraphBuilder, View, SmoothedDepthTexture, OutNormalTexture);
}

void FFluidSceneViewExtension::RenderThicknessPass(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef& OutThicknessTexture)
{
	UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();
	if (!SubsystemPtr)
	{
		return;
	}

	RenderFluidThicknessPass(GraphBuilder, View, SubsystemPtr, OutThicknessTexture);
}

void FFluidSceneViewExtension::RenderShadingPass(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	// TODO: 다음 단계에서 구현
}
