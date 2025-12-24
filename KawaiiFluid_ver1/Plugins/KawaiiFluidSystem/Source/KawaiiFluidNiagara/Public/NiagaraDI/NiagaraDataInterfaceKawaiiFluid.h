// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "RenderResource.h"
#include "RHIResources.h"
#include "NiagaraDataInterfaceKawaiiFluid.generated.h"

class UKawaiiFluidDummyComponent;
struct FKawaiiRenderParticle;

/**
 * Per-Instance 데이터 구조체
 * Niagara 시스템 인스턴스마다 하나씩 생성됨
 */
struct FNDIKawaiiFluid_InstanceData
{
	/** 참조하는 FluidDummyComponent (약한 포인터) */
	TWeakObjectPtr<UKawaiiFluidDummyComponent> SourceComponent;

	/** 마지막 업데이트 시간 */
	float LastUpdateTime = 0.0f;

	/** 캐시된 파티클 수 */
	int32 CachedParticleCount = 0;

	/** GPU 버퍼 (Position + Velocity) */
	FBufferRHIRef ParticleBuffer;
	FShaderResourceViewRHIRef ParticleSRV;

	/** 버퍼 용량 (재할당 최소화) */
	int32 BufferCapacity = 0;

	/** 버퍼가 유효한지 */
	bool IsBufferValid() const
	{
		return ParticleBuffer.IsValid() && ParticleSRV.IsValid();
	}
};

/**
 * Kawaii Fluid Data Interface (DummyComponent 전용 테스트)
 * CPU에서 생성한 테스트 파티클 데이터를 Niagara GPU 파티클로 전달
 * 
 * @note 현재 UKawaiiFluidDummyComponent만 지원 (테스트용)
 */
UCLASS(EditInlineNew, Category = "KawaiiFluid", meta = (DisplayName = "Kawaii Fluid Dummy Data"))
class KAWAIIFLUIDNIAGARA_API UNiagaraDataInterfaceKawaiiFluid : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	/** 
	 * 연결할 FluidDummyComponent
	 * @note 반드시 UKawaiiFluidDummyComponent를 가진 Actor를 선택하세요
	 */
	UPROPERTY(EditAnywhere, Category = "Kawaii Fluid", meta = (AllowedClasses = "/Script/Engine.Actor"))
	TSoftObjectPtr<AActor> SourceDummyActor;

	/** 자동 업데이트 활성화 (false면 수동 호출 필요) */
	UPROPERTY(EditAnywhere, Category = "Kawaii Fluid")
	bool bAutoUpdate = true;

	/** 업데이트 빈도 (초, 0 = 매 프레임) */
	UPROPERTY(EditAnywhere, Category = "Kawaii Fluid", meta = (ClampMin = "0.0"))
	float UpdateInterval = 0.0f;

	//========================================
	// UNiagaraDataInterface 오버라이드
	//========================================

	/** VM 함수 바인딩 (CPU 시뮬레이션용) */
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, 
	                                    void* InstanceData, 
	                                    FVMExternalFunction& OutFunc) override;

	/** 실행 타겟 지원 여부 */
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

	/** GPU 시뮬레이션 함수 등록 */
#if WITH_EDITORONLY_DATA
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, 
	                                         FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, 
	                               const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, 
	                               int FunctionInstanceIndex, 
	                               FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif

	/** Per-Instance 데이터 크기 */
	virtual int32 PerInstanceDataSize() const override
	{
		return sizeof(FNDIKawaiiFluid_InstanceData);
	}

	/** Per-Instance 데이터 초기화 */
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;

	/** Per-Instance 데이터 파괴 */
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;

	/** 매 프레임 업데이트 (게임 스레드) */
	virtual bool PerInstanceTick(void* PerInstanceData, 
	                              FNiagaraSystemInstance* SystemInstance, 
	                              float DeltaSeconds) override;

	/** 거리 필드 필요 여부 */
	virtual bool RequiresDistanceFieldData() const override { return false; }

	/** PreSimulate Tick 필요 */
	virtual bool HasPreSimulateTick() const override { return true; }

	/** 복사 가능 */
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	/** GPU 컴퓨트 파라미터 설정 */
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, 
	                                                    void* PerInstanceData, 
	                                                    const FNiagaraSystemInstanceID& SystemInstance) override;

	//========================================
	// UObject Interface
	//========================================

	/** Niagara Type Registry 등록 (필수!) */
	virtual void PostInitProperties() override;

	//========================================
	// UNiagaraDataInterface 오버라이드
	//========================================

	/** 데이터 복사 (UPROPERTY 동기화) */
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	//========================================
	// VM 함수들 (CPU 시뮬레이션용)
	//========================================

	/** 파티클 개수 가져오기 */
	void VMGetParticleCount(FVectorVMExternalFunctionContext& Context);

	/** 특정 인덱스의 파티클 위치 가져오기 */
	void VMGetParticlePosition(FVectorVMExternalFunctionContext& Context);

	/** 특정 인덱스의 파티클 속도 가져오기 */
	void VMGetParticleVelocity(FVectorVMExternalFunctionContext& Context);

	/** 파티클 반경 가져오기 */
	void VMGetParticleRadius(FVectorVMExternalFunctionContext& Context);

protected:
#if WITH_EDITORONLY_DATA
	/** 함수 시그니처 등록 (에디터 전용) */
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;

	/** 함수 유효성 검사 (컴파일 타임) */
	virtual void ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors) override;
#endif

	//========================================
	// 내부 헬퍼 함수
	//========================================

private:
	/** GPU 버퍼 업데이트 (렌더 스레드) */
	void UpdateGPUBuffers_RenderThread(FNDIKawaiiFluid_InstanceData* InstanceData, 
	                                     const TArray<FKawaiiRenderParticle>& Particles);

	/** 함수 이름 상수 */
	static const FName GetParticleCountName;
	static const FName GetParticlePositionName;
	static const FName GetParticleVelocityName;
	static const FName GetParticleRadiusName;
};
