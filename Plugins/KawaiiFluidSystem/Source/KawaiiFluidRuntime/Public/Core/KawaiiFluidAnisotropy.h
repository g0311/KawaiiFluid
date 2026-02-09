// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KawaiiFluidAnisotropy.generated.h"

/**
 * @enum EKawaiiFluidAnisotropyMode
 * @brief Anisotropy calculation mode for ellipsoid rendering.
 */
UENUM(BlueprintType)
enum class EKawaiiFluidAnisotropyMode : uint8
{
	VelocityBased UMETA(DisplayName = "Velocity Based", ToolTip = "Stretch ellipsoids along velocity direction."),
	DensityBased UMETA(DisplayName = "Density Based", ToolTip = "Calculate from neighbor particle distribution (covariance matrix)."),
	Hybrid UMETA(DisplayName = "Hybrid", ToolTip = "Combine velocity and density-based approaches.")
};

/**
 * @brief GPU Anisotropy mode (must match shader defines).
 */
enum class EGPUAnisotropyMode : uint8
{
	VelocityBased = 0,
	DensityBased = 1,
	Hybrid = 2
};

/**
 * @struct FKawaiiFluidAnisotropyParams
 * @brief Parameters for anisotropy calculation to render particles as ellipsoids.
 * 
 * @param bEnabled Render particles as ellipsoids instead of spheres. Creates smoother surfaces by filling gaps.
 * @param Mode How ellipsoid shape is determined (Density-based, Velocity-based, or Hybrid).
 * @param Strength Overall ellipsoid effect intensity. 0 = spheres, 1 = normal.
 * @param bPreserveVolume Maintain unit volume (Scale1 * Scale2 * Scale3 = 1.0) using log-space processing.
 * @param MinStretch Shortest allowed axis ratio to prevent extremely flat ellipsoids.
 * @param MaxStretch Longest allowed axis ratio to limit maximum elongation.
 * @param NonPreservedRenderScale Overall ellipsoid size when volume preservation is disabled.
 * @param VelocityStretchFactor Sensitivity of stretching to particle velocity.
 * @param DensityWeight Balance between velocity and density in Hybrid mode (0=velocity, 1=density).
 * @param UpdateInterval Frames between anisotropy updates for performance optimization.
 * @param bEnableTemporalSmoothing Blend ellipsoid orientation with previous frame to reduce flickering.
 * @param TemporalSmoothFactor Influence of previous frame orientation on current frame.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidAnisotropyParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (EditCondition = "bEnabled"))
	EKawaiiFluidAnisotropyMode Mode = EKawaiiFluidAnisotropyMode::DensityBased;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (EditCondition = "bEnabled", ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5"))
	float Strength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (EditCondition = "bEnabled"))
	bool bPreserveVolume = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (EditCondition = "bEnabled", ClampMin = "0.1", ClampMax = "0.9", UIMin = "0.1", UIMax = "0.9"))
	float MinStretch = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (EditCondition = "bEnabled", ClampMin = "1.0", ClampMax = "5.0", UIMin = "1.0", UIMax = "5.0"))
	float MaxStretch = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy",
		meta = (EditCondition = "bEnabled && !bPreserveVolume", ClampMin = "0.1", ClampMax = "4.0", UIMin = "0.1", UIMax = "4.0"))
	float NonPreservedRenderScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (EditCondition = "bEnabled && (Mode == EKawaiiFluidAnisotropyMode::VelocityBased || Mode == EKawaiiFluidAnisotropyMode::Hybrid)", ClampMin = "0", ClampMax = "1.0", UIMin = "0", UIMax = "1.0"))
	float VelocityStretchFactor = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (EditCondition = "bEnabled && Mode == EKawaiiFluidAnisotropyMode::Hybrid", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DensityWeight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy|Optimization", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10"))
	int32 UpdateInterval = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy|Temporal", meta = (EditCondition = "bEnabled"))
	bool bEnableTemporalSmoothing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy|Temporal",
		meta = (EditCondition = "bEnabled && bEnableTemporalSmoothing", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float TemporalSmoothFactor = 0.8f;

};
