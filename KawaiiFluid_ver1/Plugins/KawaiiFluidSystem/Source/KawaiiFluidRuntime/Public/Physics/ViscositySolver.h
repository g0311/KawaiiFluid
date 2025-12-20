// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/FluidParticle.h"

/**
 * 점성 솔버
 *
 * XSPH 기반 점성 구현
 * 입자들의 속도를 이웃들과 평균화하여 점성 효과 표현
 *
 * 높은 점성 계수 = 꿀, 슬라임 같은 끈적한 유체
 * 낮은 점성 계수 = 물 같은 흐르는 유체
 */
class KAWAIIFLUIDRUNTIME_API FViscositySolver
{
public:
	FViscositySolver();

	/**
	 * XSPH 점성 적용
	 *
	 * v_i = v_i + c * Σ(v_j - v_i) * W(r_ij, h)
	 *
	 * @param Particles 입자 배열
	 * @param ViscosityCoeff 점성 계수 (0.0 ~ 1.0)
	 * @param SmoothingRadius 커널 반경
	 */
	void ApplyXSPH(TArray<FFluidParticle>& Particles, float ViscosityCoeff, float SmoothingRadius);

	/**
	 * 점탄성 스프링 적용 (선택적 - 슬라임용)
	 * 입자들 사이에 스프링 연결을 유지하여 늘어났다 돌아오는 효과
	 *
	 * @param Particles 입자 배열
	 * @param SpringStiffness 스프링 강성
	 * @param DeltaTime 시간 간격
	 */
	void ApplyViscoelasticSprings(TArray<FFluidParticle>& Particles, float SpringStiffness, float DeltaTime);

private:
	/** 점탄성 스프링 연결 */
	struct FSpringConnection
	{
		int32 ParticleA;
		int32 ParticleB;
		float RestLength;

		FSpringConnection() : ParticleA(-1), ParticleB(-1), RestLength(0.0f) {}
		FSpringConnection(int32 A, int32 B, float Length) : ParticleA(A), ParticleB(B), RestLength(Length) {}
	};

	/** 스프링 연결 목록 */
	TArray<FSpringConnection> Springs;

	/** 스프링 생성 거리 임계값 */
	float SpringThreshold;

public:
	/** 스프링 연결 업데이트 (이웃 기반) */
	void UpdateSprings(const TArray<FFluidParticle>& Particles, float SmoothingRadius);

	/** 모든 스프링 제거 */
	void ClearSprings();
};
