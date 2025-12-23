// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FluidRenderingParameters.h"
#include "FluidRendererSubsystem.generated.h"

class AFluidSimulator;
class FFluidSceneViewExtension;
class IKawaiiFluidRenderable;

/**
 * 유체 렌더링 월드 서브시스템
 *
 * 역할:
 * - IKawaiiFluidRenderable 인터페이스 구현 객체 통합 관리
 * - SSFR 렌더링 파이프라인 제공 (ViewExtension)
 * - DebugMesh 렌더링은 Unreal 기본 파이프라인 사용
 *
 * @note SSFR 모드 객체만 ViewExtension에서 처리됨
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UFluidRendererSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End of USubsystem interface

	//========================================
	// 통합 렌더링 관리 (IKawaiiFluidRenderable)
	//========================================

	/** Component 등록 (Component 기반 Renderable) */
	UFUNCTION(BlueprintCallable, Category = "Fluid Rendering")
	void RegisterRenderableComponent(UActorComponent* Component);

	/** Component 해제 */
	UFUNCTION(BlueprintCallable, Category = "Fluid Rendering")
	void UnregisterRenderableComponent(UActorComponent* Component);

	/** Actor의 Component 자동 검색 후 등록 */
	void RegisterRenderableActor(AActor* Actor);

	/** 렌더링 가능한 액터 등록 (레거시, Actor 기반) */
	void RegisterRenderable(AActor* Actor);

	/** 렌더링 가능한 액터 해제 (레거시) */
	void UnregisterRenderable(AActor* Actor);

	/** 등록된 모든 렌더링 가능한 객체 반환 (Actor + Component) */
	TArray<IKawaiiFluidRenderable*> GetAllRenderables() const;

	//========================================
	// 레거시 호환성 (기존 코드 지원)
	//========================================

	/** 시뮬레이터 등록 (내부적으로 RegisterRenderable 호출) */
	void RegisterSimulator(AFluidSimulator* Simulator);

	/** 시뮬레이터 해제 (내부적으로 UnregisterRenderable 호출) */
	void UnregisterSimulator(AFluidSimulator* Simulator);

	/** 등록된 모든 시뮬레이터 (레거시) */
	const TArray<AFluidSimulator*>& GetRegisteredSimulators() const { return RegisteredSimulators; }

	//========================================
	// 렌더링 파라미터
	//========================================

	/** 글로벌 렌더링 파라미터 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Rendering")
	FFluidRenderingParameters RenderingParameters;

	/** View Extension 접근자 */
	TSharedPtr<FFluidSceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() const { return ViewExtension; }

private:
	//========================================
	// 통합 저장소
	//========================================

	/** 등록된 렌더링 가능한 모든 객체 (Actor와 Component 모두 지원) */
	UPROPERTY()
	TArray<TScriptInterface<IKawaiiFluidRenderable>> RegisteredRenderables;

	//========================================
	// 레거시 저장소 (호환성 유지)
	//========================================

	/** 등록된 시뮬레이터들 (레거시) */
	UPROPERTY()
	TArray<AFluidSimulator*> RegisteredSimulators;

	/** Scene View Extension (렌더링 파이프라인 인젝션) */
	TSharedPtr<FFluidSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;

	//========================================
	// 내부 헬퍼
	//========================================

	/** UObject가 IKawaiiFluidRenderable 인터페이스를 구현하는지 확인 */
	bool IsValidRenderable(UObject* Object) const;
};
