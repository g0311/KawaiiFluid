// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IKawaiiFluidDataProvider.generated.h"

struct FFluidParticle;

/**
 * UInterface (Unreal Reflection System용)
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UKawaiiFluidDataProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 유체 시뮬레이션 데이터를 제공하는 인터페이스
 * 
 * 시뮬레이션 컴포넌트가 이 인터페이스를 구현하여
 * 렌더링 컴포넌트에게 파티클 데이터를 제공합니다.
 * 
 * 중요: 이 인터페이스는 시뮬레이션 데이터(FFluidParticle)를 제공하며,
 * 렌더링 레이어에 대한 의존성이 없습니다.
 * 렌더링 컴포넌트가 FFluidParticle을 FKawaiiRenderParticle로 변환해야 합니다.
 * 
 * 구현 대상:
 * - UKawaiiFluidDummyComponent (테스트용)
 * - UKawaiiFluidSimulationComponent (실제 시뮬레이션)
 * 
 * 사용 예시:
 * @code
 * IKawaiiFluidDataProvider* Provider = Cast<IKawaiiFluidDataProvider>(Component);
 * if (Provider && Provider->IsDataValid())
 * {
 *     const TArray<FFluidParticle>& Particles = Provider->GetParticles();
 *     // 렌더링용 데이터로 변환 후 렌더링 처리...
 * }
 * @endcode
 */
class IKawaiiFluidDataProvider
{
	GENERATED_BODY()

public:
	/**
	 * 시뮬레이션 파티클 데이터 반환
	 * 
	 * 주의: 시뮬레이션 원본 데이터를 반환합니다.
	 * 렌더링 컴포넌트는 이 데이터를 렌더링 형식(FKawaiiRenderParticle)으로 변환해야 합니다.
	 * 
	 * @return 시뮬레이션 파티클 배열 (위치, 속도, 밀도, 접착 상태 등 포함)
	 */
	virtual const TArray<FFluidParticle>& GetParticles() const = 0;

	/**
	 * 파티클 개수 반환
	 * @return 현재 시뮬레이션 중인 파티클 개수
	 */
	virtual int32 GetParticleCount() const = 0;

	/**
	 * 파티클 렌더링 반경 반환 (cm)
	 * @return 파티클 표시 크기
	 */
	virtual float GetParticleRenderRadius() const = 0;

	/**
	 * 데이터 유효성 확인
	 * @return 렌더링 가능한 상태인지 여부
	 */
	virtual bool IsDataValid() const = 0;

	/**
	 * 디버그 이름 반환 (프로파일링/로깅용)
	 * @return 데이터 제공자 식별 문자열
	 */
	virtual FString GetDebugName() const = 0;
};

