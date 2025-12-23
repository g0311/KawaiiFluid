// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FluidDummyGenerationMode.generated.h"

/**
 * 유체 더미 데이터 생성 모드
 */
UENUM(BlueprintType)
enum class EKawaiiFluidDummyGenMode : uint8
{
	Static      UMETA(DisplayName = "Static (고정)"),
	Animated    UMETA(DisplayName = "Animated (애니메이션)"),
	GridPattern UMETA(DisplayName = "Grid Pattern (격자)"),
	Sphere      UMETA(DisplayName = "Sphere (구)"),
	Wave        UMETA(DisplayName = "Wave (파동)")
};
