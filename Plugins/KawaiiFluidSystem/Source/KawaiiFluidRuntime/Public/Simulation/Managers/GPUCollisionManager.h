// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUCollisionManager - Unified collision system manager

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "Simulation/Resources/GPUFluidSpatialData.h"
#include "Simulation/Managers/GPUCollisionFeedbackManager.h"

class FRHICommandListImmediate;
class FRDGBuilder;

/**
 * @class FGPUCollisionManager
 * @brief Manages all collision-related systems for GPU fluid simulation.
 * 
 * @param bIsInitialized State of the manager.
 * @param CollisionLock Critical section for thread-safe access.
 * @param HeightmapParams Parameters for Landscape heightmap collision.
 * @param HeightmapTextureRHI RHI texture resource for the heightmap.
 * @param bHeightmapDataValid Flag indicating valid heightmap data.
 * @param CachedSpheres Array of collision spheres.
 * @param CachedCapsules Array of collision capsules.
 * @param CachedBoxes Array of collision boxes.
 * @param CachedConvexHeaders Headers for convex collision primitives.
 * @param CachedConvexPlanes Plane data for convex collision primitives.
 * @param CachedBoneTransforms Bone transforms for primitive attachment.
 * @param PrimitiveCollisionThreshold Search threshold for primitive collisions.
 * @param bCollisionPrimitivesValid Flag indicating valid primitive data.
 * @param bBoneTransformsValid Flag indicating valid bone transform data.
 * @param FeedbackManager Internal manager for GPU->CPU collision feedback.
 */
class KAWAIIFLUIDRUNTIME_API FGPUCollisionManager
{
public:
	FGPUCollisionManager();
	~FGPUCollisionManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	void Initialize();

	void Release();

	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Heightmap Collision Configuration (for Landscape terrain)
	//=========================================================================

	void SetHeightmapCollisionEnabled(bool bEnabled) { HeightmapParams.bEnabled = bEnabled ? 1 : 0; }

	void SetHeightmapCollisionParams(const FGPUHeightmapCollisionParams& Params) { HeightmapParams = Params; }

	const FGPUHeightmapCollisionParams& GetHeightmapCollisionParams() const { return HeightmapParams; }

	bool IsHeightmapCollisionEnabled() const { return HeightmapParams.bEnabled != 0 && bHeightmapDataValid; }

	void UploadHeightmapTexture(const TArray<float>& HeightData, int32 Width, int32 Height);

	bool HasValidHeightmapData() const { return bHeightmapDataValid; }

	//=========================================================================
	// Collision Primitives
	//=========================================================================

	void UploadCollisionPrimitives(const FGPUCollisionPrimitives& Primitives);

	void SetPrimitiveCollisionThreshold(float Threshold) { PrimitiveCollisionThreshold = Threshold; }

	float GetPrimitiveCollisionThreshold() const { return PrimitiveCollisionThreshold; }

	bool HasCollisionPrimitives() const { return bCollisionPrimitivesValid; }

	int32 GetCollisionPrimitiveCount() const
	{
		return CachedSpheres.Num() + CachedCapsules.Num() + CachedBoxes.Num() + CachedConvexHeaders.Num();
	}

	int32 GetTotalColliderCount() const { return GetCollisionPrimitiveCount(); }

	//=========================================================================
	// Collision Passes (called from render thread)
	//=========================================================================

	void AddBoundsCollisionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 ParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	void AddPrimitiveCollisionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 ParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	void AddHeightmapCollisionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 ParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	//=========================================================================
	// Collision Feedback
	//=========================================================================

	void SetCollisionFeedbackEnabled(bool bEnabled);

	bool IsCollisionFeedbackEnabled() const;

	void AllocateCollisionFeedbackBuffers(FRHICommandListImmediate& RHICmdList);

	void ReleaseCollisionFeedbackBuffers();

	void ProcessCollisionFeedbackReadback(FRHICommandListImmediate& RHICmdList);

	void ProcessColliderContactCountReadback(FRHICommandListImmediate& RHICmdList);

	bool GetCollisionFeedbackForCollider(int32 ColliderIndex, TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	bool GetAllCollisionFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	int32 GetCollisionFeedbackCount() const;

	bool GetAllStaticMeshCollisionFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	int32 GetStaticMeshCollisionFeedbackCount() const;

	bool GetAllFluidInteractionSMCollisionFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	int32 GetFluidInteractionSMCollisionFeedbackCount() const;

	int32 GetColliderContactCount(int32 ColliderIndex) const;

	void GetAllColliderContactCounts(TArray<int32>& OutCounts) const;

	int32 GetContactCountForOwner(int32 OwnerID) const;

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
