// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KawaiiFluidRenderingMode.generated.h"

/**
 * Kawaii Fluid 렌더링 방식
 */
UENUM(BlueprintType)
enum class EKawaiiFluidRenderingMode : uint8
{
	/** 디버그 메시 렌더링 (Instanced Static Mesh) */
	DebugMesh   UMETA(DisplayName = "Debug Mesh"),

	/** Niagara 파티클 시스템 (GPU 최적화) */
	Niagara     UMETA(DisplayName = "Niagara Particles"),

	/** SSFR 파이프라인 렌더링 (Screen Space Fluid Rendering) */
	SSFR        UMETA(DisplayName = "SSFR"),

	/** 둘 다 렌더링 (디버그용) */
	Both        UMETA(DisplayName = "Both")
};
