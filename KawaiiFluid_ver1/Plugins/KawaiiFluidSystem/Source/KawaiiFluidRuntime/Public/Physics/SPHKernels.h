// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * SPH 커널 함수 모음
 *
 * Poly6: 밀도 추정용 (부드러운 감소)
 * Spiky: 그래디언트 계산용 (가까운 거리에서 강한 반발력)
 * Viscosity: 점성 계산용
 * Adhesion: 접착력 계산용
 * Cohesion: 응집력(표면장력) 계산용
 */
namespace SPHKernels
{
	//========================================
	// Poly6 커널 (밀도 추정)
	// W(r, h) = 315 / (64 * π * h^9) * (h² - r²)³
	//========================================

	/** Poly6 커널 계수 */
	KAWAIIFLUIDRUNTIME_API float Poly6Coefficient(float h);

	/** Poly6 커널 값 */
	KAWAIIFLUIDRUNTIME_API float Poly6(float r, float h);

	/** Poly6 커널 (벡터 입력) */
	KAWAIIFLUIDRUNTIME_API float Poly6(const FVector& r, float h);

	//========================================
	// Spiky 커널 (압력 그래디언트)
	// ∇W(r, h) = -45 / (π * h^6) * (h - r)² * r̂
	//========================================

	/** Spiky 커널 그래디언트 계수 */
	KAWAIIFLUIDRUNTIME_API float SpikyGradientCoefficient(float h);

	/** Spiky 커널 그래디언트 */
	KAWAIIFLUIDRUNTIME_API FVector SpikyGradient(const FVector& r, float h);

	//========================================
	// Viscosity 커널 라플라시안 (점성)
	// ∇²W(r, h) = 45 / (π * h^6) * (h - r)
	//========================================

	/** Viscosity 커널 라플라시안 계수 */
	KAWAIIFLUIDRUNTIME_API float ViscosityLaplacianCoefficient(float h);

	/** Viscosity 커널 라플라시안 */
	KAWAIIFLUIDRUNTIME_API float ViscosityLaplacian(float r, float h);

	//========================================
	// Adhesion 커널 (접착력)
	// Akinci et al. 2013
	//========================================

	/** Adhesion 커널 */
	KAWAIIFLUIDRUNTIME_API float Adhesion(float r, float h);

	//========================================
	// Cohesion 커널 (응집력/표면장력)
	// Akinci et al. 2013
	//========================================

	/** Cohesion 커널 */
	KAWAIIFLUIDRUNTIME_API float Cohesion(float r, float h);

	//========================================
	// 유틸리티
	//========================================

	/** 커널 계수들 미리 계산 (최적화용) */
	struct FKernelCoefficients
	{
		float Poly6Coeff;
		float SpikyGradCoeff;
		float ViscosityLapCoeff;
		float h;
		float h2;  // h²
		float h6;  // h^6
		float h9;  // h^9

		FKernelCoefficients() : Poly6Coeff(0), SpikyGradCoeff(0), ViscosityLapCoeff(0), h(0), h2(0), h6(0), h9(0) {}
		void Precompute(float SmoothingRadius);
	};
}
