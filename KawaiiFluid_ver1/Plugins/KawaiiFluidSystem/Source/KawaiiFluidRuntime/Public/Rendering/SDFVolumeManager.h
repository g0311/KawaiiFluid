// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphDefinitions.h"

/**
 * @brief Manages SDF volume texture for optimized ray marching
 *
 * Creates and updates a 3D texture containing baked SDF values.
 * This allows O(1) SDF lookup during ray marching instead of O(N) particle iteration.
 *
 * Usage:
 * 1. Call BakeSDFVolume() each frame with particle data
 * 2. Use GetSDFVolumeTexture() in ray marching shader
 * 3. Use GetVolumeMin/Max() for world-space to UV conversion
 */
class KAWAIIFLUIDRUNTIME_API FSDFVolumeManager
{
public:
	FSDFVolumeManager();
	~FSDFVolumeManager();

	/**
	 * @brief Bake SDF volume from particle positions
	 *
	 * Dispatches compute shader to calculate SDF at each voxel position.
	 * Should be called each frame before ray marching.
	 *
	 * @param GraphBuilder - RDG graph builder
	 * @param ParticleBufferSRV - Structured buffer containing particle positions
	 * @param ParticleCount - Number of particles
	 * @param ParticleRadius - Radius of each particle
	 * @param SDFSmoothness - Smooth minimum factor for metaball blending
	 * @param VolumeMin - World-space minimum corner (from bounding box)
	 * @param VolumeMax - World-space maximum corner (from bounding box)
	 * @return Created SDF volume texture SRV for use in ray marching
	 */
	FRDGTextureSRVRef BakeSDFVolume(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef ParticleBufferSRV,
		int32 ParticleCount,
		float ParticleRadius,
		float SDFSmoothness,
		const FVector3f& VolumeMin,
		const FVector3f& VolumeMax);

	/** Get volume resolution (default: 64x64x64) */
	FIntVector GetVolumeResolution() const { return VolumeResolution; }

	/** Set volume resolution (call before BakeSDFVolume) */
	void SetVolumeResolution(const FIntVector& NewResolution) { VolumeResolution = NewResolution; }

	/** Get cached volume bounds (valid after BakeSDFVolume) */
	FVector3f GetVolumeMin() const { return CachedVolumeMin; }
	FVector3f GetVolumeMax() const { return CachedVolumeMax; }

private:
	/** Volume texture resolution */
	FIntVector VolumeResolution = FIntVector(64, 64, 64);

	/** Cached volume bounds for shader parameters */
	FVector3f CachedVolumeMin = FVector3f::ZeroVector;
	FVector3f CachedVolumeMax = FVector3f::ZeroVector;
};

/**
 * @brief Helper to calculate bounding box from particle positions
 *
 * @param Particles - Array of particle positions
 * @param ParticleRadius - Radius of particles
 * @param Margin - Additional margin around bounding box
 * @param OutMin - Output minimum corner
 * @param OutMax - Output maximum corner
 */
KAWAIIFLUIDRUNTIME_API void CalculateParticleBoundingBox(
	const TArray<FVector3f>& Particles,
	float ParticleRadius,
	float Margin,
	FVector3f& OutMin,
	FVector3f& OutMax);
