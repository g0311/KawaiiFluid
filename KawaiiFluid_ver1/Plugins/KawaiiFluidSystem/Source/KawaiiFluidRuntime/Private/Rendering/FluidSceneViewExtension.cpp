// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidSceneViewExtension.h"
#include "Rendering/FluidRendererSubsystem.h"
#include "Rendering/FluidDepthPass.h"
#include "Core/FluidSimulator.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "RenderGraphBuilder.h"

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

	// 등록된 시뮬레이터가 없으면 스킵
	const TArray<AFluidSimulator*>& Simulators = SubsystemPtr->GetRegisteredSimulators();
	if (Simulators.Num() == 0)
	{
		return;
	}

	// SSFR 렌더링 파이프라인 실행

	// 1. Depth Pass
	RenderDepthPass(GraphBuilder, View);

	// 2. Smoothing Pass (TODO)
	// RenderSmoothingPass(GraphBuilder, View);

	// 3. Normal Reconstruction Pass (TODO)
	// RenderNormalPass(GraphBuilder, View);

	// 4. Thickness Pass (TODO)
	// RenderThicknessPass(GraphBuilder, View);

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

void FFluidSceneViewExtension::RenderSmoothingPass(FRDGBuilder& GraphBuilder, const FSceneView& View)
{
	// TODO: 다음 단계에서 구현
}

void FFluidSceneViewExtension::RenderNormalPass(FRDGBuilder& GraphBuilder, const FSceneView& View)
{
	// TODO: 다음 단계에서 구현
}

void FFluidSceneViewExtension::RenderThicknessPass(FRDGBuilder& GraphBuilder, const FSceneView& View)
{
	// TODO: 다음 단계에서 구현
}

void FFluidSceneViewExtension::RenderShadingPass(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	// TODO: 다음 단계에서 구현
}
