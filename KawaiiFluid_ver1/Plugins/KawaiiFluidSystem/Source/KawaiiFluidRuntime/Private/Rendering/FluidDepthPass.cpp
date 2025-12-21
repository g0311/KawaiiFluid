// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidDepthPass.h"
#include "Rendering/FluidRendererSubsystem.h"
#include "Core/FluidSimulator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "MeshPassProcessor.h"

void RenderFluidDepthPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	UFluidRendererSubsystem* Subsystem,
	FRDGTextureRef& OutDepthTexture)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FluidDepthPass_InstancedMesh");

	// 등록된 시뮬레이터 가져오기
	const TArray<AFluidSimulator*>& Simulators = Subsystem->GetRegisteredSimulators();
	if (Simulators.Num() == 0)
	{
		return;
	}

	// Depth Texture 생성
	FRDGTextureDesc DepthDesc = FRDGTextureDesc::Create2D(
		View.UnscaledViewRect.Size(),
		PF_DepthStencil,
		FClearValueBinding::DepthFar,
		TexCreate_DepthStencilTargetable | TexCreate_ShaderResource);

	OutDepthTexture = GraphBuilder.CreateTexture(DepthDesc, TEXT("FluidDepthTexture"));

	// 각 시뮬레이터의 DebugMeshComponent 렌더링
	for (AFluidSimulator* Simulator : Simulators)
	{
		if (!Simulator || Simulator->GetParticleCount() == 0)
		{
			continue;
		}

		// DebugMeshComponent (InstancedStaticMeshComponent) 가져오기
		UInstancedStaticMeshComponent* MeshComp = Simulator->DebugMeshComponent;
		if (!MeshComp || !MeshComp->IsVisible())
		{
			continue;
		}

		// 파티클 개수 확인
		int32 InstanceCount = MeshComp->GetInstanceCount();
		if (InstanceCount == 0)
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("FluidDepthPass: Rendering %s with %d instances"),
			*Simulator->GetName(), InstanceCount);

		// TODO: 실제 메시 렌더링 구현
		// 현재는 InstancedStaticMeshComponent가 이미 씬에 렌더링되고 있으므로
		// 별도의 depth-only pass로 특정 render target에 다시 렌더링 필요
		//
		// 방법 1: SceneCapture2D 컴포넌트 사용
		// 방법 2: Custom depth pass with stencil
		// 방법 3: MeshDrawCommands를 직접 제출

		// 임시: 로그만 출력
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("FluidMeshDepth_%s", *Simulator->GetName()),
			ERDGPassFlags::Raster,
			[MeshComp, InstanceCount](FRHICommandList& RHICmdList)
			{
				// MeshComp는 이미 씬에서 렌더링됨
				// 여기서는 별도 depth target에 렌더링하는 로직 필요
				UE_LOG(LogTemp, Verbose, TEXT("  -> InstancedMesh ready for depth rendering"));
			});
	}
}
