// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FluidAnisotropy.generated.h"

/**
 * @brief Anisotropy calculation mode for ellipsoid rendering.
 * Based on NVIDIA FleX and Yu & Turk 2013 paper:
 * "Reconstructing surfaces of particle-based fluids using anisotropic kernels"
 */
UENUM(BlueprintType)
enum class EFluidAnisotropyMode : uint8
{
	/** No anisotropy - render as spheres */
	None UMETA(DisplayName = "None (Spheres)"),

	/** Stretch ellipsoids along velocity direction */
	VelocityBased UMETA(DisplayName = "Velocity Based"),

	/** Calculate from neighbor particle distribution (covariance matrix) */
	DensityBased UMETA(DisplayName = "Density Based"),

	/** Combine velocity and density-based approaches */
	Hybrid UMETA(DisplayName = "Hybrid")
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
 * @brief Parameters for anisotropy calculation.
 * @param bEnabled Enable anisotropy calculation.
 * @param Mode Calculation mode (velocity, density, hybrid).
 * @param Strength Intensity multiplier (0=sphere, 1=normal, >1=amplified).
 * @param MinStretch Minimum axis scale to prevent paper-thin ellipsoids.
 * @param MaxStretch Maximum axis scale to limit elongation.
 * @param VelocityStretchFactor How much velocity affects stretching.
 * @param DensityWeight Weight for density-based component in hybrid mode.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFluidAnisotropyParams
{
	GENERATED_BODY()

	/** Enable anisotropy calculation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy")
	bool bEnabled = false;

	/** Calculation mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy")
	EFluidAnisotropyMode Mode = EFluidAnisotropyMode::DensityBased;

	/**
	 * Anisotropy strength (intensity multiplier).
	 * 0 = spheres only, 1 = normal effect, >1 = amplified stretching
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5"))
	float Strength = 1.0f;

	/** Minimum axis stretch (prevents paper-thin ellipsoids, should be < 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (ClampMin = "0.1", ClampMax = "0.9", UIMin = "0.1", UIMax = "0.9"))
	float MinStretch = 0.2f;

	/** Maximum axis stretch (limits elongation, should be >= 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (ClampMin = "1.0", ClampMax = "5.0", UIMin = "1.0", UIMax = "5.0"))
	float MaxStretch = 2.0f;

	/** Velocity stretch factor (velocity-based mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (ClampMin = "0", ClampMax = "0.1", UIMin = "0", UIMax = "0.1", EditCondition = "Mode == EFluidAnisotropyMode::VelocityBased || Mode == EFluidAnisotropyMode::Hybrid"))
	float VelocityStretchFactor = 0.01f;

	/** Weight for density-based component in hybrid mode (0 = velocity only, 1 = density only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", EditCondition = "Mode == EFluidAnisotropyMode::Hybrid"))
	float DensityWeight = 0.5f;

	/** Update interval in frames (1 = every frame, 2 = every other frame, etc.)
	 *  Higher values reduce GPU cost but may cause visual lag on fast movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy|Optimization", meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10"))
	int32 UpdateInterval = 1;

	/** Enable temporal smoothing to reduce jitter between frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy|Temporal")
	bool bEnableTemporalSmoothing = true;

	/** Smoothing factor (0.0 = no smoothing/use current, 1.0 = full previous frame)
	 *  Recommended: 0.7~0.9 for smooth animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy|Temporal",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", EditCondition = "bEnableTemporalSmoothing"))
	float TemporalSmoothFactor = 0.8f;
};

// FAnisotropyComputeParams is defined in GPU/FluidAnisotropyComputeShader.h
// to avoid including RenderGraphResources.h before .generated.h
