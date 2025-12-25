// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/KawaiiFluidSSFRComponent.h"
#include "Interfaces/IKawaiiFluidDataProvider.h"
#include "Core/FluidParticle.h"

UKawaiiFluidSSFRComponent::UKawaiiFluidSSFRComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UKawaiiFluidSSFRComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidSSFRComponent: Initialized (FluidColor: %s, MaxParticles: %d)"),
		*FluidColor.ToString(),
		MaxRenderParticles);
}

void UKawaiiFluidSSFRComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 캐시 정리
	CachedParticlePositions.Empty();

	Super::EndPlay(EndPlayReason);
}

void UKawaiiFluidSSFRComponent::UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime)
{
	if (!bEnableRendering || !DataProvider)
	{
		bIsRenderingActive = false;
		return;
	}

	// DataProvider에서 시뮬레이션 데이터 가져오기
	const TArray<FFluidParticle>& SimParticles = DataProvider->GetParticles();
	
	if (SimParticles.Num() == 0)
	{
		bIsRenderingActive = false;
		LastRenderedParticleCount = 0;
		return;
	}

	// 렌더링할 파티클 수 제한
	int32 NumParticles = FMath::Min(SimParticles.Num(), MaxRenderParticles);

	// 파티클 반경
	float ParticleRadius = DataProvider->GetParticleRenderRadius();

	// GPU 리소스 업데이트
	UpdateGPUResources(SimParticles, ParticleRadius);

	// SSFR 파이프라인 실행 (ViewExtension을 통해)
	ExecuteSSFRPipeline();

	// 통계 업데이트
	LastRenderedParticleCount = NumParticles;
	bIsRenderingActive = true;
}

void UKawaiiFluidSSFRComponent::UpdateGPUResources(const TArray<FFluidParticle>& Particles, float ParticleRadius)
{
	// 파티클 수 제한
	int32 NumParticles = FMath::Min(Particles.Num(), MaxRenderParticles);

	// 위치 데이터 캐시 업데이트
	CachedParticlePositions.SetNum(NumParticles);
	for (int32 i = 0; i < NumParticles; ++i)
	{
		CachedParticlePositions[i] = Particles[i].Position;
	}

	CachedParticleRadius = ParticleRadius;

	// TODO: GPU 버퍼에 업로드
	// - Structured Buffer로 파티클 위치 전송
	// - Constant Buffer로 렌더링 파라미터 전송
	// 
	// 예시 구조:
	// struct FSSFRParticleData
	// {
	//     FVector Position;
	//     float Radius;
	// };
	//
	// FRHIResourceCreateInfo CreateInfo(TEXT("SSFRParticleBuffer"));
	// ParticleBuffer = RHICreateStructuredBuffer(
	//     sizeof(FSSFRParticleData),
	//     NumParticles * sizeof(FSSFRParticleData),
	//     BUF_ShaderResource | BUF_Dynamic,
	//     CreateInfo
	// );
	//
	// void* BufferData = RHILockBuffer(ParticleBuffer, 0, DataSize, RLM_WriteOnly);
	// FMemory::Memcpy(BufferData, CachedParticlePositions.GetData(), DataSize);
	// RHIUnlockBuffer(ParticleBuffer);

	UE_LOG(LogTemp, VeryVerbose, TEXT("KawaiiFluidSSFRComponent: GPU resources updated (%d particles, radius: %.2f)"),
		NumParticles, ParticleRadius);
}

void UKawaiiFluidSSFRComponent::ExecuteSSFRPipeline()
{
	// FluidRendererSubsystem의 ViewExtension을 통해 SSFR 파이프라인 실행
	// ViewExtension이 다음 렌더링 프레임에서 처리:
	// 
	// 1. Depth Pass: 파티클을 Depth Buffer에 렌더링
	// 2. Smoothing Pass: Bilateral Filter로 Depth 스무딩
	// 3. Thickness Pass: 파티클 두께 계산
	// 4. Normal Pass: Depth Buffer에서 법선 벡터 계산
	// 5. Lighting Pass: 최종 유체 표면 렌더링 (반사/굴절/프레넬)
	//
	// ViewExtension에서 이 컴포넌트를 찾아서 렌더링 파라미터 사용:
	// - FluidColor
	// - Metallic, Roughness
	// - RefractiveIndex
	// - FilterRadius
	// - DepthSmoothingIterations

	// ViewExtension이 다음 프레임에 이 컴포넌트를 렌더링하도록 마킹
	// UWorld* World = GetWorld();
	// UFluidRendererSubsystem* Subsystem = World->GetSubsystem<UFluidRendererSubsystem>();
	// Subsystem->MarkForSSFRRendering(this);
	
	UE_LOG(LogTemp, VeryVerbose, TEXT("KawaiiFluidSSFRComponent: Queued for SSFR rendering"));
}

