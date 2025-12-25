// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IKawaiiFluidRenderer.h"
#include "Core/FluidParticle.h"
#include "KawaiiFluidSSFRComponent.generated.h"

/**
 * Screen Space Fluid Rendering (SSFR) 기반 유체 렌더러
 * 
 * IKawaiiFluidRenderer를 구현하여 파티클을 SSFR 방식으로 렌더링합니다.
 * GPU 기반 Depth/Thickness 렌더링 후 화면 공간에서 유체 표면을 재구성합니다.
 * 
 * 특징:
 * - 사실적인 유체 표면 렌더링
 * - GPU Compute Shader 기반 고성능
 * - 반사/굴절/프레넬 효과 지원
 * - ViewExtension을 통한 커스텀 렌더링 파이프라인
 * 
 * 사용 예시:
 * @code
 * SSFRRenderer = CreateDefaultSubobject<UKawaiiFluidSSFRComponent>(TEXT("SSFRRenderer"));
 * SSFRRenderer->FluidColor = FLinearColor::Blue;
 * @endcode
 */
UCLASS(ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent, DisplayName="Fluid SSFR Renderer"))
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidSSFRComponent 
	: public UActorComponent
	, public IKawaiiFluidRenderer
{
	GENERATED_BODY()

public:
	UKawaiiFluidSSFRComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//========================================
	// IKawaiiFluidRenderer 구현
	//========================================

	virtual void UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime) override;
	virtual bool IsEnabled() const override { return bEnableRendering; }
	virtual EKawaiiFluidRenderingMode GetRenderingMode() const override { return EKawaiiFluidRenderingMode::SSFR; }
	virtual void SetEnabled(bool bInEnabled) override { bEnableRendering = bInEnabled; }

	//========================================
	// 기본 설정
	//========================================

	/** 렌더링 활성화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering")
	bool bEnableRendering = true;

	/** 유체 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Appearance")
	FLinearColor FluidColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);

	/** 유체 메탈릭 (금속성) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Metallic = 0.0f;

	/** 유체 러프니스 (거칠기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness = 0.1f;

	/** 굴절률 (IOR) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Appearance", meta = (ClampMin = "1.0", ClampMax = "2.5"))
	float RefractiveIndex = 1.33f; // 물의 굴절률

	//========================================
	// 성능 옵션
	//========================================

	/** 최대 렌더링 파티클 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Performance", meta = (ClampMin = "1", ClampMax = "100000"))
	int32 MaxRenderParticles = 50000;

	/** Depth Buffer 해상도 배율 (1.0 = 화면 해상도) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Performance", meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float DepthBufferScale = 1.0f;

	/** Thickness Buffer 사용 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Performance")
	bool bUseThicknessBuffer = true;

	//========================================
	// 필터링 옵션
	//========================================

	/** Depth Smoothing 반복 횟수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Filtering", meta = (ClampMin = "0", ClampMax = "10"))
	int32 DepthSmoothingIterations = 3;

	/** Bilateral Filter 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Filtering", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float FilterRadius = 3.0f;

	//========================================
	// 고급 옵션
	//========================================

	/** Surface Tension (표면 장력) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SurfaceTension = 0.5f;

	/** Foam 생성 임계값 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Advanced", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float FoamThreshold = 5.0f;

	/** Foam 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Advanced")
	FLinearColor FoamColor = FLinearColor::White;

	//========================================
	// 디버그 옵션
	//========================================

	/** 디버그 시각화 모드 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Debug")
	bool bShowDebugVisualization = false;

	/** 디버그 렌더링 타겟 표시 (Depth, Thickness, Normal 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SSFR Rendering|Debug", meta = (EditCondition = "bShowDebugVisualization"))
	bool bShowRenderTargets = false;

	//========================================
	// 런타임 정보
	//========================================

	/** 마지막 프레임에 렌더링된 파티클 수 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SSFR Rendering|Stats")
	int32 LastRenderedParticleCount = 0;

	/** SSFR 렌더링 활성 상태 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SSFR Rendering|Stats")
	bool bIsRenderingActive = false;

protected:
	//========================================
	// 내부 메서드
	//========================================

	/** GPU 렌더 리소스 업데이트 */
	void UpdateGPUResources(const TArray<FFluidParticle>& Particles, float ParticleRadius);

	/** SSFR 파이프라인 실행 (ViewExtension을 통해) */
	void ExecuteSSFRPipeline();

private:
	/** 마지막 업데이트된 파티클 데이터 캐시 */
	TArray<FVector> CachedParticlePositions;
	
	/** 마지막 파티클 반경 */
	float CachedParticleRadius = 5.0f;
};


