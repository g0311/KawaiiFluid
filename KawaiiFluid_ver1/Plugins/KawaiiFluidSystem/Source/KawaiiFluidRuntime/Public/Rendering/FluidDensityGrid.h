// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"

/**
 * @brief Configuration for 3D density grid.
 * @param Resolution Grid resolution in each dimension (e.g., 64x64x64).
 * @param WorldBounds World-space bounding box of the fluid volume.
 */
struct FFluidDensityGridConfig
{
	/** Grid resolution in each dimension. */
	FIntVector Resolution = FIntVector(64, 64, 64);

	/** World-space bounding box min corner. */
	FVector WorldBoundsMin = FVector::ZeroVector;

	/** World-space bounding box max corner. */
	FVector WorldBoundsMax = FVector(100.0f, 100.0f, 100.0f);

	/** Density threshold for surface detection. */
	float SurfaceThreshold = 0.5f;

	/** Padding around actual bounds (in world units). */
	float BoundsPadding = 10.0f;

	/** Get world-space size of the grid. */
	FVector GetWorldSize() const
	{
		return WorldBoundsMax - WorldBoundsMin;
	}

	/** Get voxel size in world units. */
	FVector GetVoxelSize() const
	{
		FVector WorldSize = GetWorldSize();
		return FVector(
			WorldSize.X / FMath::Max(1, Resolution.X),
			WorldSize.Y / FMath::Max(1, Resolution.Y),
			WorldSize.Z / FMath::Max(1, Resolution.Z)
		);
	}
};

/**
 * @brief Manages a 3D density grid texture for fluid volume rendering.
 *
 * This class handles the creation and management of a 3D render target
 * that stores fluid density values. Used by the shadow system to perform
 * ray marching against the fluid volume.
 *
 * @param DensityTexture 3D texture storing density values (R16F).
 * @param Config Grid configuration including resolution and bounds.
 */
class KAWAIIFLUIDRUNTIME_API FFluidDensityGrid
{
public:
	FFluidDensityGrid();
	~FFluidDensityGrid();

	/**
	 * @brief Initialize the density grid with given configuration.
	 * @param InConfig Grid configuration.
	 * @return True if initialization successful.
	 */
	bool Initialize(const FFluidDensityGridConfig& InConfig);

	/**
	 * @brief Release all GPU resources.
	 */
	void Release();

	/**
	 * @brief Update grid bounds based on particle positions.
	 * @param ParticlePositions Array of particle world positions.
	 * @param NumParticles Number of particles.
	 */
	void UpdateBoundsFromParticles(const FVector* ParticlePositions, int32 NumParticles);

	/**
	 * @brief Check if the grid needs reallocation due to config changes.
	 * @param NewConfig New configuration to compare against.
	 * @return True if reallocation is needed.
	 */
	bool NeedsReallocation(const FFluidDensityGridConfig& NewConfig) const;

	/**
	 * @brief Register the density texture with RDG for rendering.
	 * @param GraphBuilder RDG builder.
	 * @return RDG texture reference.
	 */
	FRDGTextureRef RegisterWithRDG(FRDGBuilder& GraphBuilder) const;

	/**
	 * @brief Get the pooled render target for external texture registration.
	 * @return Pooled render target reference.
	 */
	TRefCountPtr<IPooledRenderTarget> GetPooledRenderTarget() const { return DensityRT; }

	/** Get current configuration. */
	const FFluidDensityGridConfig& GetConfig() const { return Config; }

	/** Check if grid is valid and ready for use. */
	bool IsValid() const { return DensityRT.IsValid(); }

	/** Get world-to-grid transform matrix. */
	FMatrix GetWorldToGridMatrix() const;

	/** Get grid-to-world transform matrix. */
	FMatrix GetGridToWorldMatrix() const;

	/**
	 * @brief Get shader parameters for binding.
	 * @param OutBoundsMin Output bounds min.
	 * @param OutBoundsMax Output bounds max.
	 * @param OutResolution Output resolution as float3.
	 * @param OutVoxelSize Output voxel size.
	 */
	void GetShaderParameters(
		FVector3f& OutBoundsMin,
		FVector3f& OutBoundsMax,
		FVector3f& OutResolution,
		FVector3f& OutVoxelSize) const;

private:
	/** Create the 3D render target. */
	bool CreateRenderTarget();

	/** Current grid configuration. */
	FFluidDensityGridConfig Config;

	/** 3D density texture (R16F format). */
	TRefCountPtr<IPooledRenderTarget> DensityRT;

	/** Whether the grid has been initialized. */
	bool bIsInitialized = false;
};

/**
 * @brief Parameters for density grid rasterization shader.
 * @param GridBoundsMin World-space min bounds.
 * @param GridBoundsMax World-space max bounds.
 * @param GridResolution Grid resolution in each dimension.
 * @param ParticleRadius Radius of influence for each particle.
 */
struct FFluidDensityRasterizeParams
{
	FVector3f GridBoundsMin;
	FVector3f GridBoundsMax;
	FIntVector GridResolution;
	float ParticleRadius;
	int32 NumParticles;
};
