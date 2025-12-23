// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "FluidTriggerVolume.generated.h"

class AFluidSimulator;

/**
 * 유체가 볼륨에 진입했을 때 Delegate
 * @param FluidSimulator 진입한 유체 시뮬레이터
 * @param ParticleIndices 볼륨 안의 파티클 인덱스 배열
 * @param ParticleCount 파티클 수
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnFluidEnterVolume,
	AFluidSimulator*, FluidSimulator,
	const TArray<int32>&, ParticleIndices,
	int32, ParticleCount
);

/**
 * 유체가 볼륨에서 나갔을 때 Delegate
 * @param FluidSimulator 나간 유체 시뮬레이터
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnFluidExitVolume,
	AFluidSimulator*, FluidSimulator
);

/**
 * 유체 파티클 감지 트리거 볼륨
 * 특정 영역에 유체가 들어오면 이벤트 발생
 * 
 * 사용 예시:
 * - 퍼즐: 물을 채워 문 열기
 * - 게임플레이: 특정 양의 유체가 들어가면 이벤트 발생
 * - 상태 변화: 바닥이 젖으면 미끄럽게
 */
UCLASS(BlueprintType, Blueprintable)
class KAWAIIFLUIDRUNTIME_API AFluidTriggerVolume : public AActor
{
	GENERATED_BODY()
	
public:
	AFluidTriggerVolume();
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	//========================================
	// Components
	//========================================
	
	/** 트리거 박스 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;
	
	//========================================
	// Events
	//========================================
	
	/** 파티클이 들어왔을 때 (Blueprint에서 Bind 가능) */
	UPROPERTY(BlueprintAssignable, Category = "Fluid Trigger")
	FOnFluidEnterVolume OnFluidEnter;
	
	/** 파티클이 나갔을 때 (Blueprint에서 Bind 가능) */
	UPROPERTY(BlueprintAssignable, Category = "Fluid Trigger")
	FOnFluidExitVolume OnFluidExit;
	
	//========================================
	// Settings
	//========================================
	
	/** 감지 활성화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Trigger")
	bool bEnableDetection = true;
	
	/** 감지할 FluidSimulator (비어있으면 모든 Simulator 감지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Trigger")
	AFluidSimulator* TargetSimulator = nullptr;
	
	/** 트리거를 위한 최소 파티클 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Trigger", meta = (ClampMin = "1"))
	int32 MinParticleCountForTrigger = 1;
	
	/** 디버그 시각화 활성화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Trigger|Debug")
	bool bShowDebugInfo = false;
	
	//========================================
	// Query Functions
	//========================================
	
	/** 현재 볼륨 안에 있는 파티클 수 */
	UFUNCTION(BlueprintPure, Category = "Fluid Trigger")
	int32 GetParticleCountInVolume() const { return ParticlesInVolume.Num(); }
	
	/** 현재 볼륨 안에 있는 파티클 인덱스들 */
	UFUNCTION(BlueprintPure, Category = "Fluid Trigger")
	const TArray<int32>& GetParticleIndicesInVolume() const { return ParticlesInVolume; }
	
	/** 볼륨 채움 비율 (0.0 ~ 1.0, 파티클 수 기준) */
	UFUNCTION(BlueprintPure, Category = "Fluid Trigger")
	float GetFillRatio() const;
	
	/** 특정 Simulator의 파티클이 볼륨 안에 있는지 */
	UFUNCTION(BlueprintPure, Category = "Fluid Trigger")
	bool IsSimulatorInVolume(AFluidSimulator* Simulator) const;
	
private:
	//========================================
	// Internal Data
	//========================================
	
	/** 볼륨 안에 있는 파티클 인덱스들 */
	TArray<int32> ParticlesInVolume;
	
	/** 이전 프레임에 있던 파티클 수 */
	int32 PreviousParticleCount = 0;
	
	/** 현재 트리거 상태 (Enter/Exit 이벤트 발생용) */
	bool bIsTriggered = false;
	
	/** Simulator별 트리거 상태 (멀티플 Simulator 지원) */
	TMap<AFluidSimulator*, bool> SimulatorTriggerStates;
	
	//========================================
	// Internal Methods
	//========================================
	
	/** 파티클 감지 */
	void DetectParticles();
	
	/** 특정 Simulator의 파티클 감지 */
	void DetectParticlesFromSimulator(AFluidSimulator* Simulator);
	
	/** 디버그 시각화 */
	void DrawDebugInfo();
};
