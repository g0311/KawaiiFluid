// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 렌더링에 사용할 파티클 구조체 정의
 */
struct KAWAIIFLUIDRUNTIME_API FKawaiiRenderParticle
{
public:
	FVector3f Position;
	FVector3f Velocity;
	float Radius;
	float Padding; // 16바이트 정렬을 위한 패딩

	FKawaiiRenderParticle()
		: Position(FVector3f::ZeroVector)
		, Velocity(FVector3f::ZeroVector)
		, Radius(1.0f)
		, Padding(0.0f)
	{
	}
};

// 32 바이트 크기 확인
static_assert(sizeof(FKawaiiRenderParticle) == 32, "FKawaiiRenderParticle size is not 32 bytes.");

// 오프셋 검증
static_assert(STRUCT_OFFSET(FKawaiiRenderParticle, Radius) == 24, "Radius offset is incorrect.");
