// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Pipeline/KawaiiMetaballRayMarchPipeline.h"
#include "Rendering/KawaiiFluidMetaballRenderer.h"
#include "Rendering/KawaiiFluidRenderResource.h"
#include "GPU/GPUFluidSimulator.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "Core/KawaiiRenderParticle.h"
#include "Rendering/Shaders/FluidSpatialHashShaders.h"

// Separated shading implementation
#include "Rendering/Shading/KawaiiRayMarchShadingImpl.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "SceneView.h"
#include "ScenePrivate.h"
#include "SceneTextures.h"

void FKawaiiMetaballRayMarchPipeline::ProcessPendingBoundsReadback()
{
	// Process readback from previous frame (if available)
	if (bHasPendingBoundsReadback && PendingBoundsReadbackBuffer.IsValid())
	{
		// Map the pooled buffer and read bounds using RHI command list
		FRHIBuffer* BufferRHI = PendingBoundsReadbackBuffer->GetRHI();
		if (BufferRHI)
		{
			// Get the immediate command list for buffer operations
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

			// Lock buffer for reading
			void* MappedData = RHICmdList.LockBuffer(BufferRHI, 0, sizeof(FVector3f) * 2, RLM_ReadOnly);
			if (MappedData)
			{
				FVector3f* BoundsData = static_cast<FVector3f*>(MappedData);
				FVector3f ReadMin = BoundsData[0];
				FVector3f ReadMax = BoundsData[1];
				RHICmdList.UnlockBuffer(BufferRHI);

				// Validate bounds (check for NaN or extreme values)
				bool bValidBounds = !ReadMin.ContainsNaN() && !ReadMax.ContainsNaN() &&
					ReadMin.X < ReadMax.X && ReadMin.Y < ReadMax.Y && ReadMin.Z < ReadMax.Z;

				if (bValidBounds)
				{
					// Update cached bounds for this frame
					SDFVolumeManager.UpdateCachedBoundsFromReadback(ReadMin, ReadMax);

					// Debug log - every frame for debugging
					FVector3f Size = ReadMax - ReadMin;
					UE_LOG(LogTemp, Log, TEXT("[Bounds Readback] Min(%.1f, %.1f, %.1f) Max(%.1f, %.1f, %.1f) Size(%.1f, %.1f, %.1f)"),
						ReadMin.X, ReadMin.Y, ReadMin.Z,
						ReadMax.X, ReadMax.Y, ReadMax.Z,
						Size.X, Size.Y, Size.Z);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("RayMarchPipeline: Invalid GPU bounds detected (Min: %.1f,%.1f,%.1f Max: %.1f,%.1f,%.1f)"),
						ReadMin.X, ReadMin.Y, ReadMin.Z,
						ReadMax.X, ReadMax.Y, ReadMax.Z);
				}
			}
		}
		bHasPendingBoundsReadback = false;
	}
}

//=============================================================================
// CollectParticleBuffers - GPU/CPU 버퍼 수집
//=============================================================================
bool FKawaiiMetaballRayMarchPipeline::CollectParticleBuffers(
	FRDGBuilder& GraphBuilder,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGBufferSRVRef& OutParticleBufferSRV,
	int32& OutParticleCount,
	float& OutAverageRadius,
	TArray<FVector3f>& OutCPUPositions,
	bool& bOutUsingGPUBuffer)
{
	OutParticleBufferSRV = nullptr;
	OutParticleCount = 0;
	OutAverageRadius = 10.0f;
	OutCPUPositions.Empty();
	bOutUsingGPUBuffer = false;

	float TotalRadius = 0.0f;
	int32 ValidCount = 0;

	// GPU 시뮬레이션 모드 체크
	for (UKawaiiFluidMetaballRenderer* Renderer : Renderers)
	{
		FGPUFluidSimulator* GPUSimulator = Renderer->GetGPUSimulator();
		if (GPUSimulator)
		{
			TRefCountPtr<FRDGPooledBuffer> PhysicsPooledBuffer = GPUSimulator->GetPersistentParticleBuffer();
			const int32 PhysicsParticleCount = GPUSimulator->GetPersistentParticleCount();

			UE_LOG(LogTemp, Warning, TEXT("  GPU Simulator: PooledValid=%d, ParticleCount=%d"),
				PhysicsPooledBuffer.IsValid() ? 1 : 0, PhysicsParticleCount);

			if (PhysicsPooledBuffer.IsValid() && PhysicsParticleCount > 0)
			{
				// Physics 버퍼 등록 (FGPUFluidParticle - 64 bytes)
				FRDGBufferRef PhysicsBuffer = GraphBuilder.RegisterExternalBuffer(
					PhysicsPooledBuffer, TEXT("GPUPhysicsParticles"));
				FRDGBufferSRVRef PhysicsBufferSRV = GraphBuilder.CreateSRV(PhysicsBuffer);

				// Render 버퍼 생성 (FKawaiiRenderParticle - 32 bytes)
				FRDGBufferRef RenderBuffer = GraphBuilder.CreateBuffer(
					FRDGBufferDesc::CreateStructuredDesc(sizeof(FKawaiiRenderParticle), PhysicsParticleCount),
					TEXT("GPURenderParticles"));
				FRDGBufferUAVRef RenderBufferUAV = GraphBuilder.CreateUAV(RenderBuffer);

				// Physics → Render 포맷 변환
				float ParticleRadius = Renderer->GetCachedParticleRadius();
				FGPUFluidSimulatorPassBuilder::AddExtractRenderDataPass(
					GraphBuilder, PhysicsBufferSRV, RenderBufferUAV,
					PhysicsParticleCount, ParticleRadius);

				OutParticleBufferSRV = GraphBuilder.CreateSRV(RenderBuffer);
				OutParticleCount = PhysicsParticleCount;
				OutAverageRadius = ParticleRadius;
				bOutUsingGPUBuffer = true;

				UE_LOG(LogTemp, Warning, TEXT("  >>> CONVERTED GPU PHYSICS → RENDER (%d particles, radius: %.2f)"),
					OutParticleCount, ParticleRadius);
				return true;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  >>> GPU MODE - BUFFER NOT READY YET"));
				return false;
			}
		}

		TotalRadius += Renderer->GetCachedParticleRadius();
		ValidCount++;
	}

	// CPU 시뮬레이션 모드
	UE_LOG(LogTemp, Warning, TEXT("  >>> USING CPU CACHE (CPU simulation mode)"));
	TArray<FKawaiiRenderParticle> AllParticles;

	for (UKawaiiFluidMetaballRenderer* Renderer : Renderers)
	{
		FKawaiiFluidRenderResource* RenderResource = Renderer->GetFluidRenderResource();
		if (RenderResource && RenderResource->IsValid())
		{
			const TArray<FKawaiiRenderParticle>& CachedParticles = RenderResource->GetCachedParticles();
			AllParticles.Append(CachedParticles);

			if (CachedParticles.Num() > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("  CPU using %d cached particles"),
					CachedParticles.Num());
			}
		}
	}

	if (AllParticles.Num() == 0)
	{
		return false;
	}

	// 바운딩 박스 계산용 위치 추출
	OutCPUPositions.Reserve(AllParticles.Num());
	for (const FKawaiiRenderParticle& Particle : AllParticles)
	{
		OutCPUPositions.Add(Particle.Position);
	}

	// RDG 버퍼 생성
	FRDGBufferRef ParticleBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FKawaiiRenderParticle), AllParticles.Num()),
		TEXT("RayMarchRenderParticles"));

	GraphBuilder.QueueBufferUpload(
		ParticleBuffer, AllParticles.GetData(),
		AllParticles.Num() * sizeof(FKawaiiRenderParticle),
		ERDGInitialDataFlags::None);

	OutParticleBufferSRV = GraphBuilder.CreateSRV(ParticleBuffer);
	OutParticleCount = AllParticles.Num();
	OutAverageRadius = (ValidCount > 0) ? TotalRadius / ValidCount : 10.0f;

	return true;
}

//=============================================================================
// BuildSDFVolume - SDF 3D 텍스처 베이크
//=============================================================================
void FKawaiiMetaballRayMarchPipeline::BuildSDFVolume(
	FRDGBuilder& GraphBuilder,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGBufferSRVRef ParticleBufferSRV,
	int32 ParticleCount,
	float AverageRadius,
	const FVector3f& BoundsMin,
	const FVector3f& BoundsMax)
{
	RDG_EVENT_SCOPE(GraphBuilder, "SDFVolumeBake");

	int32 Resolution = FMath::Clamp(RenderParams.SDFVolumeResolution, 32, 256);
	SDFVolumeManager.SetVolumeResolution(FIntVector(Resolution, Resolution, Resolution));

	FRDGTextureSRVRef SDFVolumeSRV = SDFVolumeManager.BakeSDFVolume(
		GraphBuilder, ParticleBufferSRV, ParticleCount,
		AverageRadius, RenderParams.SDFSmoothness, BoundsMin, BoundsMax);

	CachedPipelineData.SDFVolumeData.SDFVolumeTextureSRV = SDFVolumeSRV;
	CachedPipelineData.SDFVolumeData.VolumeMin = BoundsMin;
	CachedPipelineData.SDFVolumeData.VolumeMax = BoundsMax;
	CachedPipelineData.SDFVolumeData.VolumeResolution = SDFVolumeManager.GetVolumeResolution();
	CachedPipelineData.SDFVolumeData.bUseSDFVolume = true;

	// 디버그 시각화
	for (UKawaiiFluidMetaballRenderer* Renderer : Renderers)
	{
		if (Renderer && Renderer->GetLocalParameters().bDebugDrawSDFVolume)
		{
			Renderer->SetSDFVolumeBounds(FVector(BoundsMin), FVector(BoundsMax));
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("KawaiiFluid: Built SDF Volume (%dx%dx%d)"),
		Resolution, Resolution, Resolution);
}

//=============================================================================
// BuildSpatialHash - Spatial Hash 가속 구조 빌드
//=============================================================================
void FKawaiiMetaballRayMarchPipeline::BuildSpatialHash(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef ParticleBufferSRV,
	int32 ParticleCount,
	float AverageRadius,
	float SDFSmoothness)
{
	RDG_EVENT_SCOPE(GraphBuilder, "SpatialHashBuild");

	const float InfluenceRadius = AverageRadius + SDFSmoothness;
	const float CellSize = InfluenceRadius * 1.1f;

	FSpatialHashGPUResources HashResources;
	if (FSpatialHashBuilder::BuildSpatialHash(GraphBuilder, ParticleBufferSRV, ParticleCount, CellSize, HashResources)
		&& HashResources.IsValid())
	{
		CachedPipelineData.SpatialHashData.bUseSpatialHash = true;
		CachedPipelineData.SpatialHashData.ParticlePositionsSRV = ParticleBufferSRV;
		CachedPipelineData.SpatialHashData.CellCountsSRV = HashResources.CellCountsSRV;
		CachedPipelineData.SpatialHashData.CellStartIndicesSRV = HashResources.CellStartIndicesSRV;
		CachedPipelineData.SpatialHashData.ParticleIndicesSRV = HashResources.ParticleIndicesSRV;
		CachedPipelineData.SpatialHashData.CellSize = CellSize;
		CachedPipelineData.ParticlePositionsSRV = ParticleBufferSRV;

		UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: Built Spatial Hash (%d particles, CellSize=%.2f)"),
			ParticleCount, CellSize);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluid: Failed to build Spatial Hash, falling back to O(N)"));
	}
}

//=============================================================================
// BuildAccelerationStructure - SDF Volume 또는 Spatial Hash 빌드
//=============================================================================
void FKawaiiMetaballRayMarchPipeline::BuildAccelerationStructure(
	FRDGBuilder& GraphBuilder,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGBufferSRVRef ParticleBufferSRV,
	int32 ParticleCount,
	float AverageRadius,
	const TArray<FVector3f>& CPUPositions,
	bool bUsingGPUBuffer)
{
	// 초기화 - 모든 가속 구조 비활성화
	CachedPipelineData.SDFVolumeData.Reset();
	CachedPipelineData.SpatialHashData.Reset();

	const float Margin = AverageRadius * 2.0f + RenderParams.SDFSmoothness;

	// GPU 바운드 계산 (GPU 모드일 때 공통)
	FVector3f BoundsMin, BoundsMax;
	if (bUsingGPUBuffer)
	{
		FRDGBufferRef BoundsBuffer = SDFVolumeManager.CalculateGPUBounds(
			GraphBuilder, ParticleBufferSRV, ParticleCount, AverageRadius, Margin);

		GraphBuilder.QueueBufferExtraction(BoundsBuffer, &PendingBoundsReadbackBuffer);
		bHasPendingBoundsReadback = true;

		// 캐시된 바운드 사용 (1-frame latency) 또는 스폰 위치 폴백
		if (SDFVolumeManager.HasValidGPUBounds())
		{
			SDFVolumeManager.GetLastGPUBounds(BoundsMin, BoundsMax);
		}
		else
		{
			FVector3f SpawnCenter = FVector3f::ZeroVector;
			for (UKawaiiFluidMetaballRenderer* Renderer : Renderers)
			{
				if (Renderer)
				{
					SpawnCenter = FVector3f(Renderer->GetSpawnPositionHint());
					break;
				}
			}
			float DefaultExtent = 100.0f;
			BoundsMin = SpawnCenter - FVector3f(DefaultExtent);
			BoundsMax = SpawnCenter + FVector3f(DefaultExtent);
		}
	}
	else
	{
		CalculateParticleBoundingBox(CPUPositions, AverageRadius, Margin, BoundsMin, BoundsMax);
	}

	// 파이프라인 바운드 업데이트
	CachedPipelineData.ParticleBoundsMin = BoundsMin;
	CachedPipelineData.ParticleBoundsMax = BoundsMax;
	CachedPipelineData.bHasValidBounds = SDFVolumeManager.HasValidGPUBounds() || !bUsingGPUBuffer;

	// ========== 모드 선택: SDF Volume > Spatial Hash > Direct ==========
	if (RenderParams.bUseSDFVolumeOptimization)
	{
		BuildSDFVolume(GraphBuilder, RenderParams, Renderers, ParticleBufferSRV,
			ParticleCount, AverageRadius, BoundsMin, BoundsMax);
	}
	else if (RenderParams.bUseSpatialHashAcceleration)
	{
		BuildSpatialHash(GraphBuilder, ParticleBufferSRV, ParticleCount, AverageRadius,
			RenderParams.SDFSmoothness);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("KawaiiFluid: Using direct particle iteration O(N)"));
	}
}

bool FKawaiiMetaballRayMarchPipeline::PrepareParticleBuffer(
	FRDGBuilder& GraphBuilder,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers)
{
	// 이전 프레임 readback 처리
	ProcessPendingBoundsReadback();

	// 1. 파티클 버퍼 수집 (GPU 또는 CPU)
	FRDGBufferSRVRef ParticleBufferSRV = nullptr;
	int32 ParticleCount = 0;
	float AverageRadius = 10.0f;
	TArray<FVector3f> CPUPositions;
	bool bUsingGPUBuffer = false;

	if (!CollectParticleBuffers(GraphBuilder, Renderers,
		ParticleBufferSRV, ParticleCount, AverageRadius, CPUPositions, bUsingGPUBuffer))
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballRayMarchPipeline: No particles - skipping"));
		CachedPipelineData.Reset();
		return false;
	}

	// 2. 파이프라인 데이터 설정
	CachedPipelineData.ParticleBufferSRV = ParticleBufferSRV;
	CachedPipelineData.ParticleCount = ParticleCount;
	CachedPipelineData.ParticleRadius = AverageRadius;

	// 3. 가속 구조 빌드 (바운드 계산 포함)
	BuildAccelerationStructure(GraphBuilder, RenderParams, Renderers,
		ParticleBufferSRV, ParticleCount, AverageRadius, CPUPositions, bUsingGPUBuffer);

	return true;
}

void FKawaiiMetaballRayMarchPipeline::ExecutePostBasePass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture)
{
	if (Renderers.Num() == 0)
	{
		return;
	}

	// PostProcess mode uses PrepareForTonemap + ExecuteTonemap at Tonemap timing
	if (RenderParams.ShadingMode == EMetaballShadingMode::PostProcess)
	{
		// Nothing to do here - PostProcess mode preparation happens in PrepareForTonemap
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MetaballPipeline_RayMarching_PostBasePass");

	// Prepare particle buffer for GBuffer/Translucent modes
	if (!PrepareParticleBuffer(GraphBuilder, RenderParams, Renderers))
	{
		return;
	}

	// ShadingMode-specific processing at PostBasePass timing
	// Delegate to separated shading implementation
	switch (RenderParams.ShadingMode)
	{
	case EMetaballShadingMode::GBuffer:
	case EMetaballShadingMode::Opaque:
		KawaiiRayMarchShading::RenderGBufferShading(
			GraphBuilder, View, RenderParams, CachedPipelineData, SceneDepthTexture);
		break;

	case EMetaballShadingMode::Translucent:
		KawaiiRayMarchShading::RenderTranslucentGBufferWrite(
			GraphBuilder, View, RenderParams, CachedPipelineData, SceneDepthTexture);
		break;

	default:
		break;
	}

	UE_LOG(LogTemp, Verbose, TEXT("KawaiiFluid: RayMarching PostBasePass completed (%d particles, ShadingMode=%d)"),
		CachedPipelineData.ParticleCount, static_cast<int32>(RenderParams.ShadingMode));
}

void FKawaiiMetaballRayMarchPipeline::PrepareForTonemap(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture)
{
	if (Renderers.Num() == 0)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MetaballPipeline_RayMarching_PrepareForTonemap");

	// Prepare particle buffer and optional SDF volume for PostProcess shading
	if (!PrepareParticleBuffer(GraphBuilder, RenderParams, Renderers))
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("KawaiiFluid: RayMarching PrepareForTonemap completed (%d particles)"),
		CachedPipelineData.ParticleCount);
}

void FKawaiiMetaballRayMarchPipeline::ExecutePrePostProcess(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output,
	FRDGTextureRef GBufferATexture,
	FRDGTextureRef GBufferDTexture)
{
	if (Renderers.Num() == 0)
	{
		return;
	}

	// Only Translucent mode uses PrePostProcess timing
	if (RenderParams.ShadingMode != EMetaballShadingMode::Translucent)
	{
		return;
	}

	// Validate cached pipeline data
	if (!CachedPipelineData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballRayMarchPipeline: Missing cached pipeline data for PrePostProcess"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MetaballPipeline_RayMarching_PrePostProcess");

	// Delegate to separated shading implementation
	KawaiiRayMarchShading::RenderTranslucentTransparency(
		GraphBuilder, View, RenderParams,
		SceneDepthTexture, SceneColorTexture, Output,
		GBufferATexture, GBufferDTexture);

	UE_LOG(LogTemp, Verbose, TEXT("KawaiiFluid: RayMarching PrePostProcess executed"));
}

void FKawaiiMetaballRayMarchPipeline::ExecuteTonemap(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FFluidRenderingParameters& RenderParams,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneColorTexture,
	FScreenPassRenderTarget Output)
{
	if (Renderers.Num() == 0)
	{
		return;
	}

	// Only PostProcess mode uses Tonemap timing
	if (RenderParams.ShadingMode != EMetaballShadingMode::PostProcess)
	{
		return;
	}

	// Validate cached pipeline data
	if (!CachedPipelineData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FKawaiiMetaballRayMarchPipeline: Missing cached pipeline data for Tonemap"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MetaballPipeline_RayMarching_Tonemap");

	// Delegate to separated shading implementation
	KawaiiRayMarchShading::RenderPostProcessShading(
		GraphBuilder, View, RenderParams, CachedPipelineData,
		SceneDepthTexture, SceneColorTexture, Output);

	UE_LOG(LogTemp, Verbose, TEXT("KawaiiFluid: RayMarching Tonemap executed"));
}
