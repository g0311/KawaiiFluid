// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUCollisionManager - Unified collision system manager

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "GPU/GPUFluidParticle.h"
#include "GPU/GPUFluidSpatialData.h"
#include "GPU/Managers/GPUCollisionFeedbackManager.h"

class FRHICommandListImmediate;
class FRDGBuilder;

/**
 * FGPUCollisionManager
 *
 * Manages all collision-related systems for GPU fluid simulation:
 * - Bounds collision (AABB/OBB)
 * - Distance Field collision
 * - Primitive collision (Spheres, Capsules, Boxes, Convexes)
 * - Collision feedback (GPU -> CPU readback)
 *
 * This consolidates collision logic that was previously scattered across
 * GPUFluidSimulator and GPUFluidSimulator_Collision.cpp
 */
class KAWAIIFLUIDRUNTIME_API FGPUCollisionManager
{
public:
	FGPUCollisionManager();
	~FGPUCollisionManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	/** Initialize the collision manager */
	void Initialize();

	/** Release all resources */
	void Release();

	/** Check if manager is ready */
	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Heightmap Collision Configuration (for Landscape terrain)
	//=========================================================================

	/** Enable or disable Heightmap collision */
	void SetHeightmapCollisionEnabled(bool bEnabled) { HeightmapParams.bEnabled = bEnabled ? 1 : 0; }

	/** Set Heightmap collision parameters */
	void SetHeightmapCollisionParams(const FGPUHeightmapCollisionParams& Params) { HeightmapParams = Params; }

	/** Get Heightmap collision parameters */
	const FGPUHeightmapCollisionParams& GetHeightmapCollisionParams() const { return HeightmapParams; }

	/** Check if Heightmap collision is enabled */
	bool IsHeightmapCollisionEnabled() const { return HeightmapParams.bEnabled != 0 && bHeightmapDataValid; }

	/**
	 * Upload heightmap texture data to GPU
	 * @param HeightData - Array of normalized height values (0-1)
	 * @param Width - Texture width
	 * @param Height - Texture height
	 */
	void UploadHeightmapTexture(const TArray<float>& HeightData, int32 Width, int32 Height);

	/** Check if heightmap data is valid */
	bool HasValidHeightmapData() const { return bHeightmapDataValid; }

	//=========================================================================
	// Collision Primitives
	//=========================================================================

	/**
	 * Upload collision primitives to GPU
	 * @param Primitives - Collection of collision primitives
	 */
	void UploadCollisionPrimitives(const FGPUCollisionPrimitives& Primitives);

	/** Set primitive collision threshold */
	void SetPrimitiveCollisionThreshold(float Threshold) { PrimitiveCollisionThreshold = Threshold; }

	/** Get primitive collision threshold */
	float GetPrimitiveCollisionThreshold() const { return PrimitiveCollisionThreshold; }

	/** Check if collision primitives are available */
	bool HasCollisionPrimitives() const { return bCollisionPrimitivesValid; }

	/** Get total number of collision primitives */
	int32 GetCollisionPrimitiveCount() const
	{
		return CachedSpheres.Num() + CachedCapsules.Num() + CachedBoxes.Num() + CachedConvexHeaders.Num();
	}

	/** Get total collider count */
	int32 GetTotalColliderCount() const { return GetCollisionPrimitiveCount(); }

	//=========================================================================
	// Collision Passes (called from render thread)
	//=========================================================================

	/** Add bounds collision pass (AABB/OBB) */
	void AddBoundsCollisionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 ParticleCount,
		const FGPUFluidSimulationParams& Params);

	/** Add primitive collision pass (spheres, capsules, boxes, convexes) */
	void AddPrimitiveCollisionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 ParticleCount,
		const FGPUFluidSimulationParams& Params);

	/** Add heightmap collision pass (Landscape terrain) */
	void AddHeightmapCollisionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 ParticleCount,
		const FGPUFluidSimulationParams& Params);

	//=========================================================================
	// Collision Feedback
	//=========================================================================

	/** Enable or disable collision feedback recording */
	void SetCollisionFeedbackEnabled(bool bEnabled);

	/** Check if collision feedback is enabled */
	bool IsCollisionFeedbackEnabled() const;

	/** Allocate collision feedback readback buffers */
	void AllocateCollisionFeedbackBuffers(FRHICommandListImmediate& RHICmdList);

	/** Release collision feedback buffers */
	void ReleaseCollisionFeedbackBuffers();

	/** Process collision feedback readback (non-blocking) */
	void ProcessCollisionFeedbackReadback(FRHICommandListImmediate& RHICmdList);

	/** Process collider contact count readback (non-blocking) */
	void ProcessColliderContactCountReadback(FRHICommandListImmediate& RHICmdList);

	/** Get collision feedback for a specific collider */
	bool GetCollisionFeedbackForCollider(int32 ColliderIndex, TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	/** Get all collision feedback (unfiltered, bone colliders only) */
	bool GetAllCollisionFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	/** Get current collision feedback count (bone colliders) */
	int32 GetCollisionFeedbackCount() const;

	/** Get all StaticMesh collision feedback (BoneIndex < 0, for buoyancy center) */
	bool GetAllStaticMeshCollisionFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	/** Get current StaticMesh collision feedback count */
	int32 GetStaticMeshCollisionFeedbackCount() const;

	/** Get all FluidInteraction StaticMesh collision feedback (BoneIndex < 0, bHasFluidInteraction = 1) */
	bool GetAllFluidInteractionSMCollisionFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	/** Get current FluidInteraction StaticMesh collision feedback count */
	int32 GetFluidInteractionSMCollisionFeedbackCount() const;

	/** Get collider contact count */
	int32 GetColliderContactCount(int32 ColliderIndex) const;

	/** Get all collider contact counts */
	void GetAllColliderContactCounts(TArray<int32>& OutCounts) const;

	/** Get contact count for a specific owner ID */
	int32 GetContactCountForOwner(int32 OwnerID) const;

	/** Get internal feedback manager (for advanced use) */
	FGPUCollisionFeedbackManager* GetFeedbackManager() const { return FeedbackManager.Get(); }

	//=========================================================================
	// Cached Data Accessors (for SimPasses)
	//=========================================================================

	const TArray<FGPUCollisionSphere>& GetCachedSpheres() const { return CachedSpheres; }
	const TArray<FGPUCollisionCapsule>& GetCachedCapsules() const { return CachedCapsules; }
	const TArray<FGPUCollisionBox>& GetCachedBoxes() const { return CachedBoxes; }
	const TArray<FGPUCollisionConvex>& GetCachedConvexHeaders() const { return CachedConvexHeaders; }
	const TArray<FGPUConvexPlane>& GetCachedConvexPlanes() const { return CachedConvexPlanes; }
	const TArray<FGPUBoneTransform>& GetCachedBoneTransforms() const { return CachedBoneTransforms; }

	/** Check if bone transforms are valid */
	bool AreBoneTransformsValid() const { return bBoneTransformsValid; }

private:
	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;
	mutable FCriticalSection CollisionLock;

	//=========================================================================
	// Heightmap Collision (Landscape terrain)
	//=========================================================================

	FGPUHeightmapCollisionParams HeightmapParams;
	FTextureRHIRef HeightmapTextureRHI;
	bool bHeightmapDataValid = false;

	//=========================================================================
	// Collision Primitives
	//=========================================================================

	TArray<FGPUCollisionSphere> CachedSpheres;
	TArray<FGPUCollisionCapsule> CachedCapsules;
	TArray<FGPUCollisionBox> CachedBoxes;
	TArray<FGPUCollisionConvex> CachedConvexHeaders;
	TArray<FGPUConvexPlane> CachedConvexPlanes;
	TArray<FGPUBoneTransform> CachedBoneTransforms;

	float PrimitiveCollisionThreshold = 1.0f;
	bool bCollisionPrimitivesValid = false;
	bool bBoneTransformsValid = false;

	//=========================================================================
	// Collision Feedback
	//=========================================================================

	TUniquePtr<FGPUCollisionFeedbackManager> FeedbackManager;
};
