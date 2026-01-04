// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "ScreenPass.h"

/**
 * Intermediate textures produced by ScreenSpace pipeline
 * These are cached between PostBasePass and Tonemap passes
 */
struct FMetaballIntermediateTextures
{
	/** Smoothed depth texture (bilateral filtered) */
	FRDGTextureRef SmoothedDepthTexture = nullptr;

	/** View-space normal texture */
	FRDGTextureRef NormalTexture = nullptr;

	/** Accumulated thickness texture */
	FRDGTextureRef ThicknessTexture = nullptr;

	/** GBuffer textures (optional, for GBuffer shading mode) */
	FRDGTextureRef GBufferATexture = nullptr;
	FRDGTextureRef GBufferBTexture = nullptr;
	FRDGTextureRef GBufferCTexture = nullptr;
	FRDGTextureRef GBufferDTexture = nullptr;

	bool IsValid() const
	{
		return SmoothedDepthTexture != nullptr &&
		       NormalTexture != nullptr &&
		       ThicknessTexture != nullptr;
	}

	void Reset()
	{
		SmoothedDepthTexture = nullptr;
		NormalTexture = nullptr;
		ThicknessTexture = nullptr;
		GBufferATexture = nullptr;
		GBufferBTexture = nullptr;
		GBufferCTexture = nullptr;
		GBufferDTexture = nullptr;
	}
};

/**
 * SDF Volume data for Ray Marching optimization
 * Baked 3D SDF texture for O(1) distance lookup
 */
struct FSDFVolumeData
{
	/** Whether to use SDF Volume optimization */
	bool bUseSDFVolume = false;

	/** SDF Volume texture SRV */
	FRDGTextureSRVRef SDFVolumeTextureSRV = nullptr;

	/** Volume bounds in world space */
	FVector3f VolumeMin = FVector3f::ZeroVector;
	FVector3f VolumeMax = FVector3f::ZeroVector;

	/** Volume resolution (voxels per dimension) */
	FIntVector VolumeResolution = FIntVector(64, 64, 64);

	bool IsValid() const
	{
		return bUseSDFVolume && SDFVolumeTextureSRV != nullptr;
	}

	void Reset()
	{
		bUseSDFVolume = false;
		SDFVolumeTextureSRV = nullptr;
		VolumeMin = FVector3f::ZeroVector;
		VolumeMax = FVector3f::ZeroVector;
		VolumeResolution = FIntVector(64, 64, 64);
	}
};

/**
 * Spatial Hash data for O(k) SDF evaluation
 * Multi-pass GPU spatial hash for neighbor search
 */
struct FSpatialHashData
{
	/** Whether to use Spatial Hash acceleration */
	bool bUseSpatialHash = false;

	/** Particle positions buffer (float3) for spatial hash lookup */
	FRDGBufferSRVRef ParticlePositionsSRV = nullptr;

	/** Cell counts buffer [HASH_SIZE] */
	FRDGBufferSRVRef CellCountsSRV = nullptr;

	/** Cell start indices buffer [HASH_SIZE] (Prefix Sum result) */
	FRDGBufferSRVRef CellStartIndicesSRV = nullptr;

	/** Sorted particle indices buffer [ParticleCount] */
	FRDGBufferSRVRef ParticleIndicesSRV = nullptr;

	/** Cell size in world units */
	float CellSize = 0.0f;

	bool IsValid() const
	{
		return bUseSpatialHash &&
			ParticlePositionsSRV != nullptr &&
			CellCountsSRV != nullptr &&
			CellStartIndicesSRV != nullptr &&
			ParticleIndicesSRV != nullptr &&
			CellSize > 0.0f;
	}

	void Reset()
	{
		bUseSpatialHash = false;
		ParticlePositionsSRV = nullptr;
		CellCountsSRV = nullptr;
		CellStartIndicesSRV = nullptr;
		ParticleIndicesSRV = nullptr;
		CellSize = 0.0f;
	}
};

/**
 * RayMarching Pipeline data
 * Contains particle buffer information for ray marching
 */
struct FRayMarchingPipelineData
{
	/** Particle positions buffer (FKawaiiRenderParticle format) */
	FRDGBufferSRVRef ParticleBufferSRV = nullptr;

	/** Particle positions buffer (float3 only, for Spatial Hash) */
	FRDGBufferSRVRef ParticlePositionsSRV = nullptr;

	/** Number of particles */
	int32 ParticleCount = 0;

	/** Average particle radius */
	float ParticleRadius = 0.0f;

	/** Particle bounding box (GPU BoundsReduction 결과) */
	FVector3f ParticleBoundsMin = FVector3f(-1000.0f, -1000.0f, -1000.0f);
	FVector3f ParticleBoundsMax = FVector3f(1000.0f, 1000.0f, 1000.0f);
	bool bHasValidBounds = false;

	/** SDF Volume data for optimized ray marching (O(1)) */
	FSDFVolumeData SDFVolumeData;

	/** Spatial Hash data for accelerated SDF evaluation (O(k)) */
	FSpatialHashData SpatialHashData;

	bool IsValid() const
	{
		return ParticleBufferSRV != nullptr && ParticleCount > 0;
	}

	void Reset()
	{
		ParticleBufferSRV = nullptr;
		ParticlePositionsSRV = nullptr;
		ParticleCount = 0;
		ParticleRadius = 0.0f;
		ParticleBoundsMin = FVector3f(-1000.0f, -1000.0f, -1000.0f);
		ParticleBoundsMax = FVector3f(1000.0f, 1000.0f, 1000.0f);
		bHasValidBounds = false;
		SDFVolumeData.Reset();
		SpatialHashData.Reset();
	}
};
