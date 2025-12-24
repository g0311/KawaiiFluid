// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/KawaiiRenderParticle.h"
#include "Rendering/IKawaiiFluidRenderable.h"
#include "Rendering/KawaiiFluidRenderingMode.h"
#include "Test/FluidDummyGenerationMode.h"
#include "Components/KawaiiFluidDummyComponent.h"
#include "KawaiiFluidDummy.generated.h"

class FKawaiiFluidRenderResource;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 렌더링 테스트용 유체 더미 액터 (레거시 래퍼)
 * 
 * @deprecated UKawaiiFluidDummyComponent 사용을 권장합니다.
 * @note 하위 호환성을 위해 유지되며, 내부적으로 UKawaiiFluidDummyComponent를 사용합니다.
 * 
 * 마이그레이션 가이드:
 * @code
 * // 이전 (Actor 방식)
 * AKawaiiFluidDummy* Dummy = GetWorld()->SpawnActor<AKawaiiFluidDummy>();
 * 
 * // 새로운 방식 (Component)
 * AActor* TestActor = GetWorld()->SpawnActor<AActor>();
 * UKawaiiFluidDummyComponent* DummyComp = NewObject<UKawaiiFluidDummyComponent>(TestActor);
 * DummyComp->RegisterComponent();
 * @endcode
 */
UCLASS(BlueprintType, HideCategories = (Collision, Physics, LOD, Cooking))
class KAWAIIFLUIDRUNTIME_API AKawaiiFluidDummy : public AActor
{
	GENERATED_BODY()

public:
	AKawaiiFluidDummy();
	virtual void BeginPlay() override;

	//========================================
	// Components
	//========================================

	/** Root 컴포넌트 (에디터에서 이동 가능하도록) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* RootSceneComponent;

	/** 실제 유체 더미 Component (내부 구현) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UKawaiiFluidDummyComponent* DummyComponent;

	//========================================
	// Niagara 자동 설정 (테스트용)
	//========================================

	/** Niagara 자동 생성 활성화 (테스트 편의 기능) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Niagara")
	bool bAutoCreateNiagara = false;

	/** 자동 생성할 Niagara System (비어있으면 기본 시스템 사용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Niagara", meta = (EditCondition = "bAutoCreateNiagara"))
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystemAsset;

	/** 생성된 Niagara Component (자동 생성 시) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test|Niagara")
	UNiagaraComponent* AutoNiagaraComponent;

	/** Niagara Data Interface 파라미터 이름 (기본: "FluidSource") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Niagara", meta = (EditCondition = "bAutoCreateNiagara"))
	FName DataInterfaceParameterName = TEXT("FluidSource");

	/** Niagara System을 수동으로 설정 (런타임) */
	UFUNCTION(BlueprintCallable, Category = "Test|Niagara")
	void SetNiagaraSystem(UNiagaraSystem* System);

	/** 빠른 테스트 설정 (ParticleCount, DataMode, NiagaraSystem 일괄 설정) */
	UFUNCTION(BlueprintCallable, Category = "Test")
	void QuickTestSetup(int32 InParticleCount = 500, EKawaiiFluidDummyGenMode InDataMode = EKawaiiFluidDummyGenMode::Animated, UNiagaraSystem* InNiagaraSystem = nullptr);

	//========================================
	// 레거시 API (Component로 포워딩)
	//========================================

	/** 테스트 데이터 재생성 */
	UFUNCTION(BlueprintCallable, Category = "Test")
	void RegenerateTestData()
	{
		if (DummyComponent) DummyComponent->RegenerateTestData();
	}

	/** GPU 버퍼 강제 업데이트 */
	UFUNCTION(BlueprintCallable, Category = "Test")
	void ForceUpdateGPUBuffer()
	{
		if (DummyComponent) DummyComponent->ForceUpdateGPUBuffer();
	}

	/** 현재 파티클 수 반환 */
	UFUNCTION(BlueprintPure, Category = "Test")
	int32 GetCurrentParticleCount() const
	{
		return DummyComponent ? DummyComponent->GetCurrentParticleCount() : 0;
	}

	/** 테스트 패턴 변경 */
	UFUNCTION(BlueprintCallable, Category = "Test")
	void SetTestPattern(EKawaiiFluidDummyGenMode NewMode)
	{
		if (DummyComponent) DummyComponent->SetTestPattern(NewMode);
	}

	/** 파티클 개수 변경 */
	UFUNCTION(BlueprintCallable, Category = "Test")
	void SetParticleCount(int32 NewCount)
	{
		if (DummyComponent) DummyComponent->SetParticleCount(NewCount);
	}

	/** 애니메이션 일시정지/재개 */
	UFUNCTION(BlueprintCallable, Category = "Test")
	void ToggleAnimation()
	{
		if (DummyComponent) DummyComponent->ToggleAnimation();
	}

private:
	/** Niagara System 자동 생성 */
	void CreateAutoNiagaraSystem();

	/** 기본 Niagara System 생성 (에셋이 없을 때) */
	UNiagaraSystem* CreateDefaultNiagaraSystem();
};
