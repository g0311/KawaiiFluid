// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Rendering/KawaiiFluidRenderingMode.h"
#include "IKawaiiFluidRenderer.generated.h"

class IKawaiiFluidDataProvider;

/**
 * UInterface (Unreal Reflection System용)
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UKawaiiFluidRenderer : public UInterface
{
	GENERATED_BODY()
};

/**
 * 유체 렌더링을 수행하는 컴포넌트가 구현해야 하는 인터페이스
 * 
 * 렌더링 방식별 구현체가 이 인터페이스를 구현하여
 * 각자의 방식으로 파티클을 렌더링합니다.
 * 
 * 책임:
 * 1. DataProvider로부터 시뮬레이션 데이터(FFluidParticle) 가져오기
 * 2. 시뮬레이션 데이터를 렌더링 데이터(FKawaiiRenderParticle)로 변환
 * 3. 변환된 데이터로 실제 렌더링 수행
 * 
 * 이 설계를 통해 시뮬레이션 레이어는 렌더링 레이어에 대한 의존성이 없습니다.
 * 
 * 구현 대상:
 * - UKawaiiFluidISMComponent (Instanced Static Mesh)
 * - UKawaiiFluidSSFRComponent (Screen Space Fluid Rendering)
 * - UKawaiiFluidNiagaraComponent (Niagara Particles)
 * 
 * 사용 예시:
 * @code
 * // Visual Component에서 사용
 * for (IKawaiiFluidRenderer* Renderer : Renderers)
 * {
 *     if (Renderer->IsEnabled())
 *     {
 *         Renderer->UpdateRendering(DataProvider, DeltaTime);
 *     }
 * }
 * @endcode
 */
class IKawaiiFluidRenderer
{
	GENERATED_BODY()

public:
	/**
	 * 렌더링 업데이트
	 * 
	 * 구현체는 다음을 수행해야 합니다:
	 * 1. DataProvider->GetParticles()로 시뮬레이션 데이터 가져오기
	 * 2. FFluidParticle -> FKawaiiRenderParticle 변환 (렌더링에 필요한 데이터만 추출)
	 * 3. 변환된 데이터로 렌더링 수행
	 * 
	 * @param DataProvider 파티클 데이터 제공자 (시뮬레이션 데이터)
	 * @param DeltaTime 프레임 델타 타임
	 */
	virtual void UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime) = 0;

	/**
	 * 렌더링 활성화 여부
	 * @return 현재 렌더링이 활성화되어 있는지
	 */
	virtual bool IsEnabled() const = 0;

	/**
	 * 렌더링 모드 반환
	 * @return 이 렌더러의 렌더링 방식
	 */
	virtual EKawaiiFluidRenderingMode GetRenderingMode() const = 0;

	/**
	 * 렌더링 활성화/비활성화
	 * @param bInEnabled 활성화 여부
	 */
	virtual void SetEnabled(bool bInEnabled) = 0;
};

