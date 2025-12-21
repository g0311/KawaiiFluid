// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/FluidParticle.h"

/**
 * SPH 커널 계수 (사전 계산)
 * Pow() 호출 제거를 위해 미리 계산된 값들
 */
struct FSPHKernelCoeffs
{
	float h;              // 커널 반경 (m)
	float h2;             // h²
	float Poly6Coeff;     // 315 / (64πh⁹)
	float SpikyCoeff;     // -45 / (πh⁶)
	float InvRestDensity; // 1 / ρ₀
	float SmoothingRadiusSq; // h² (cm²)
};

/**
 * PBF 밀도 제약 솔버
 *
 * 제약 조건: C_i = (ρ_i / ρ_0) - 1 = 0
 * 각 입자의 밀도가 기준 밀도(ρ_0)를 유지하도록 위치 보정
 */
class KAWAIIFLUIDRUNTIME_API FDensityConstraint
{
public:
	FDensityConstraint();
	FDensityConstraint(float InRestDensity, float InSmoothingRadius, float InEpsilon);

	/** 밀도 제약 해결 (한 번의 반복) */
	void Solve(TArray<FFluidParticle>& Particles, float InSmoothingRadius, float InRestDensity, float InEpsilon);

	/** 파라미터 설정 */
	void SetRestDensity(float NewRestDensity);
	void SetEpsilon(float NewEpsilon);

private:
	float RestDensity;      // 기준 밀도 (kg/m³)
	float Epsilon;          // 안정성 상수
	float SmoothingRadius;  // 커널 반경 (cm)

	//========================================
	// SoA 캐시 (Structure of Arrays)
	//========================================
	TArray<float> PosX, PosY, PosZ;  // 예측 위치
	TArray<float> Masses;             // 질량
	TArray<float> Densities;          // 밀도
	TArray<float> Lambdas;            // Lambda
	TArray<float> DeltaPX, DeltaPY, DeltaPZ;  // 위치 보정량

	//========================================
	// SoA 관리 함수
	//========================================
	void ResizeSoAArrays(int32 NumParticles);
	void CopyToSoA(const TArray<FFluidParticle>& Particles);
	void ApplyFromSoA(TArray<FFluidParticle>& Particles);

	//========================================
	// SIMD 최적화 함수 (Solve 내부에서 사용)
	//========================================

	/** 1단계: 밀도 + Lambda 동시 계산 (SIMD) */
	void ComputeDensityAndLambda_SIMD(
		const TArray<FFluidParticle>& Particles,
		const FSPHKernelCoeffs& Coeffs);

	/** 2단계: 위치 보정량 계산 (SIMD) */
	void ComputeDeltaP_SIMD(
		const TArray<FFluidParticle>& Particles,
		const FSPHKernelCoeffs& Coeffs);

	//========================================
	// 레거시 함수 (하위 호환성)
	//========================================
	void ComputeDensities(TArray<FFluidParticle>& Particles);
	void ComputeLambdas(TArray<FFluidParticle>& Particles);
	void ApplyPositionCorrection(TArray<FFluidParticle>& Particles);
	float ComputeParticleDensity(const FFluidParticle& Particle, const TArray<FFluidParticle>& Particles);
	float ComputeParticleLambda(const FFluidParticle& Particle, const TArray<FFluidParticle>& Particles);
	FVector ComputeDeltaPosition(int32 ParticleIndex, const TArray<FFluidParticle>& Particles);
};
