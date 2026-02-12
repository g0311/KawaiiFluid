// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUStaticBoundaryManager - Static Mesh Boundary Particle Generator
// Generates boundary particles on static colliders for density contribution (Akinci 2012)
//
// Performance Optimization (v2):
// - Primitive ID-based caching: boundary particles are cached per-primitive
// - Only new primitives trigger generation; existing primitives reuse cached data
// - GPU upload only when active primitive set changes

#pragma once

#include "CoreMinimal.h"
#include "Simulation/Resources/GPUFluidParticle.h"

/**
 * @class FGPUStaticBoundaryManager
 * @brief Generates boundary particles on static mesh colliders for density contribution.
 * 
 * @param bIsInitialized State of the manager.
 * @param bIsEnabled Whether static boundary generation is active.
 * @param ParticleSpacing Desired distance between boundary particles in cm.
 * @param BoundaryParticles List of active boundary particles for the current frame.
 * @param PrimitiveCache Map of cached boundary particles per unique primitive key.
 * @param ActivePrimitiveKeys Set of keys for primitives active in the current frame.
 * @param PreviousActivePrimitiveKeys Set of keys for primitives active in the previous frame.
 * @param CachedSmoothingRadius Smoothing radius used for the current cache.
 * @param CachedRestDensity Rest density used for the current cache.
 * @param CachedParticleSpacing Spacing used for the current cache.
 * @param bCacheInvalidated Flag indicating cache must be fully regenerated.
 */
class KAWAIIFLUIDRUNTIME_API FGPUStaticBoundaryManager
{
public:
	FGPUStaticBoundaryManager();
	~FGPUStaticBoundaryManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	void Initialize();

	void Release();

	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Boundary Particle Generation
	//=========================================================================

	bool GenerateBoundaryParticles(
		const TArray<FGPUCollisionSphere>& Spheres,
		const TArray<FGPUCollisionCapsule>& Capsules,
		const TArray<FGPUCollisionBox>& Boxes,
		const TArray<FGPUCollisionConvex>& Convexes,
		const TArray<FGPUConvexPlane>& ConvexPlanes,
		float SmoothingRadius,
		float RestDensity);

	void ClearBoundaryParticles();

	void InvalidateCache();

	//=========================================================================
	// Accessors
	//=========================================================================

	const TArray<FGPUBoundaryParticle>& GetBoundaryParticles() const { return BoundaryParticles; }

	int32 GetBoundaryParticleCount() const { return BoundaryParticles.Num(); }

	bool HasBoundaryParticles() const { return BoundaryParticles.Num() > 0; }

	//=========================================================================
	// Configuration
	//=========================================================================

	void SetEnabled(bool bEnabled) { bIsEnabled = bEnabled; }

	bool IsEnabled() const { return bIsEnabled; }

	void SetParticleSpacing(float Spacing) { ParticleSpacing = FMath::Max(Spacing, 1.0f); }

	float GetParticleSpacing() const { return ParticleSpacing; }

private:
	//=========================================================================
	// Primitive Key Generation (for cache lookup)
	//=========================================================================

	/** Primitive type enum for cache key */
	enum class EPrimitiveType : uint8
	{
		Sphere = 0,
		Capsule = 1,
		Box = 2,
		Convex = 3
	};

	/** 
	 * Generate unique cache key for a primitive
	 * Key format: (Type:8 | OwnerID:32 | GeometryHash:24) = 64-bit
	 */
	static uint64 MakePrimitiveKey(EPrimitiveType Type, int32 OwnerID, uint32 GeometryHash);
	
	/** Compute geometry hash for each primitive type */
	static uint32 ComputeGeometryHash(const FGPUCollisionSphere& Sphere);
	static uint32 ComputeGeometryHash(const FGPUCollisionCapsule& Capsule);
	static uint32 ComputeGeometryHash(const FGPUCollisionBox& Box);
	static uint32 ComputeGeometryHash(const FGPUCollisionConvex& Convex);

	//=========================================================================
	// Generation Helpers (output to provided array)
	//=========================================================================

	/** Generate boundary particles on a sphere surface */
	void GenerateSphereBoundaryParticles(
		const FVector3f& Center,
		float Radius,
		float Spacing,
		float Psi,
		int32 OwnerID,
		TArray<FGPUBoundaryParticle>& OutParticles);

	/** Generate boundary particles on a capsule surface */
	void GenerateCapsuleBoundaryParticles(
		const FVector3f& Start,
		const FVector3f& End,
		float Radius,
		float Spacing,
		float Psi,
		int32 OwnerID,
		TArray<FGPUBoundaryParticle>& OutParticles);

	/** Generate boundary particles on a box surface */
	void GenerateBoxBoundaryParticles(
		const FVector3f& Center,
		const FVector3f& Extent,
		const FQuat4f& Rotation,
		float Spacing,
		float Psi,
		int32 OwnerID,
		TArray<FGPUBoundaryParticle>& OutParticles);

	/** Generate boundary particles on a convex hull surface */
	void GenerateConvexBoundaryParticles(
		const FGPUCollisionConvex& Convex,
		const TArray<FGPUConvexPlane>& AllPlanes,
		float Spacing,
		float Psi,
		int32 OwnerID,
		TArray<FGPUBoundaryParticle>& OutParticles);

	/** Calculate Psi value based on spacing */
	float CalculatePsi(float Spacing, float RestDensity) const;

	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;
	bool bIsEnabled = true;
	float ParticleSpacing = 5.0f;  // Default: 5.0 cm (same as FluidInteractionComponent)

	//=========================================================================
	// Generated Data (Active boundary particles for current frame)
	//=========================================================================

	TArray<FGPUBoundaryParticle> BoundaryParticles;

	//=========================================================================
	// Primitive Cache (Persistent across frames)
	// Key: PrimitiveKey (Type + OwnerID + GeometryHash)
	// Value: Cached boundary particles for that primitive
	//=========================================================================

	TMap<uint64, TArray<FGPUBoundaryParticle>> PrimitiveCache;
	
	/** Set of active primitive keys in current frame (for change detection) */
	TSet<uint64> ActivePrimitiveKeys;
	TSet<uint64> PreviousActivePrimitiveKeys;

	//=========================================================================
	// Cache Parameters (detect parameter changes requiring recalculation)
	//=========================================================================

	float CachedSmoothingRadius = 0.0f;
	float CachedRestDensity = 0.0f;
	float CachedParticleSpacing = 0.0f;
	bool bCacheInvalidated = false;
};
