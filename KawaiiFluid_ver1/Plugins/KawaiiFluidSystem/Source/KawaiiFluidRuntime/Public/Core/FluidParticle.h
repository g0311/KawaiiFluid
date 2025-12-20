// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FluidParticle.generated.h"

/**
 * 유체 입자 구조체
 * PBF 시뮬레이션의 기본 단위
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFluidParticle
{
	GENERATED_BODY()

	// 위치
	UPROPERTY(BlueprintReadWrite, Category = "Particle")
	FVector Position;

	// 예측 위치 (솔버에서 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	FVector PredictedPosition;

	// 속도
	UPROPERTY(BlueprintReadWrite, Category = "Particle")
	FVector Velocity;

	// 질량
	UPROPERTY(BlueprintReadWrite, Category = "Particle")
	float Mass;

	// 밀도 (매 프레임 계산)
	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	float Density;

	// 라그랑주 승수 (밀도 제약용)
	float Lambda;

	// 접착 상태
	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	bool bIsAttached;

	// 접착된 액터
	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	TWeakObjectPtr<AActor> AttachedActor;

	// 입자 ID
	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	int32 ParticleID;

	// 이웃 입자 인덱스 (캐싱용)
	TArray<int32> NeighborIndices;

	FFluidParticle()
		: Position(FVector::ZeroVector)
		, PredictedPosition(FVector::ZeroVector)
		, Velocity(FVector::ZeroVector)
		, Mass(1.0f)
		, Density(0.0f)
		, Lambda(0.0f)
		, bIsAttached(false)
		, ParticleID(-1)
	{
	}

	FFluidParticle(const FVector& InPosition, int32 InID)
		: Position(InPosition)
		, PredictedPosition(InPosition)
		, Velocity(FVector::ZeroVector)
		, Mass(1.0f)
		, Density(0.0f)
		, Lambda(0.0f)
		, bIsAttached(false)
		, ParticleID(InID)
	{
	}
};
