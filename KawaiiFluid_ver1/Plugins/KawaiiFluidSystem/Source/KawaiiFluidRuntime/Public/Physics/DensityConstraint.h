// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/FluidParticle.h"

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
	void Solve(TArray<FFluidParticle>& Particles, float SmoothingRadius);

	/** 파라미터 설정 */
	void SetRestDensity(float NewRestDensity);
	void SetEpsilon(float NewEpsilon);

private:
	/** 기준 밀도 */
	float RestDensity;

	/** 안정성 상수 */
	float Epsilon;

	/** 커널 반경 */
	float SmoothingRadius;

	/** 1. 모든 입자의 밀도 계산 */
	void ComputeDensities(TArray<FFluidParticle>& Particles);

	/** 2. 모든 입자의 Lambda 계산 */
	void ComputeLambdas(TArray<FFluidParticle>& Particles);

	/** 3. 위치 보정 적용 */
	void ApplyPositionCorrection(TArray<FFluidParticle>& Particles);

	/** 단일 입자의 밀도 계산 */
	float ComputeParticleDensity(const FFluidParticle& Particle, const TArray<FFluidParticle>& Particles);

	/** 단일 입자의 Lambda 계산 */
	float ComputeParticleLambda(const FFluidParticle& Particle, const TArray<FFluidParticle>& Particles);

	/** 단일 입자의 위치 보정량 계산 */
	FVector ComputeDeltaPosition(int32 ParticleIndex, const TArray<FFluidParticle>& Particles);
};
