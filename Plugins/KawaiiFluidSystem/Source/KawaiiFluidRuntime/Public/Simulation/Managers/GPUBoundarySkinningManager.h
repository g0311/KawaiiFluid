// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "Simulation/Resources/GPUFluidSpatialData.h"
#include "Core/KawaiiFluidSimulationTypes.h"

class USkeletalMeshComponent;
class FRDGBuilder;

/**
 * @class FGPUBoundarySkinningManager
 * @brief Manages GPU-based boundary skinning for Flex-style adhesion.
 *
 * @param bIsInitialized State of the manager.
 * @param PersistentStaticBoundaryBuffer GPU buffer for static boundary particles.
 * @param PersistentStaticZOrderSorted Z-Order sorted buffer for static boundaries.
 * @param PersistentStaticCellStart Spatial hash cell start indices for static boundaries.
 * @param PersistentStaticCellEnd Spatial hash cell end indices for static boundaries.
 * @param StaticBoundaryParticleCount Number of static boundary particles.
 * @param StaticBoundaryBufferCapacity Capacity of the static boundary buffer.
 * @param bStaticBoundaryEnabled Whether static boundary processing is active.
 * @param bStaticBoundaryDirty Flag indicating static boundary needs re-upload/sort.
 * @param bStaticZOrderValid Flag indicating static Z-Order data is up-to-date.
 * @param PendingStaticBoundaryParticles CPU storage for particles waiting for GPU upload.
 * @param CachedBoundaryAdhesionParams Configuration for boundary adhesion.
 * @param BoundarySkinningDataMap Map of skinning data per mesh owner.
 * @param TotalLocalBoundaryParticleCount Sum of all local boundary particles.
 * @param PersistentLocalBoundaryBuffers Map of GPU buffers for local boundary particles.
 * @param PersistentWorldBoundaryBuffer GPU buffer for world-space boundary particles.
 * @param PreviousWorldBoundaryBuffer Previous frame buffer for velocity calculation.
 * @param WorldBoundaryBufferCapacity Capacity of the world boundary buffer.
 * @param bHasPreviousFrame Whether previous frame data is available.
 * @param bBoundarySkinningDataDirty Flag indicating skinning data needs update.
 * @param bUseBoundaryZOrder Whether to use Z-Order sorting for boundaries.
 * @param bBoundaryZOrderDirty Flag indicating boundary Z-Order needs re-sort.
 * @param bBoundaryZOrderValid Flag indicating boundary Z-Order data is valid.
 * @param GridResolutionPreset Resolution preset for the spatial hash.
 * @param ZOrderBoundsMin Minimum bounds for Z-Order calculation.
 * @param ZOrderBoundsMax Maximum bounds for Z-Order calculation.
 * @param bUseHybridTiledZOrder Whether to use hybrid tiled Z-Order mode.
 * @param PersistentSortedBoundaryBuffer GPU buffer for sorted boundary particles.
 * @param PersistentBoundaryCellStart Spatial hash cell start indices for boundaries.
 * @param PersistentBoundaryCellEnd Spatial hash cell end indices for boundaries.
 * @param BoundaryZOrderBufferCapacity Capacity of the boundary Z-Order buffer.
 * @param BoundaryOwnerAABBs Map of world-space AABBs per mesh owner.
 * @param CombinedBoundaryAABB Unified AABB encompassing all owners.
 * @param bBoundaryAABBDirty Flag indicating combined AABB needs recalculation.
 * @param PendingBoneTransformSnapshots Queue of snapshotted bone transforms.
 * @param ActiveSnapshot Currently active snapshot for simulation.
 * @param BoundarySkinningLock Critical section for thread-safe access.
 */
class KAWAIIFLUIDRUNTIME_API FGPUBoundarySkinningManager
{
public:
	FGPUBoundarySkinningManager();
	~FGPUBoundarySkinningManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	void Initialize();

	void Release();

	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Static Boundary Particles (StaticMesh colliders - Persistent GPU)
	//=========================================================================

	void UploadStaticBoundaryParticles(const TArray<FGPUBoundaryParticle>& Particles);

	void ClearStaticBoundaryParticles();

	void MarkStaticBoundaryDirty() { bStaticBoundaryDirty = true; }

	bool HasStaticBoundaryData() const { return bStaticBoundaryEnabled && StaticBoundaryParticleCount > 0; }

	int32 GetStaticBoundaryParticleCount() const { return StaticBoundaryParticleCount; }

	void SetStaticBoundaryEnabled(bool bEnabled) { bStaticBoundaryEnabled = bEnabled; }

	bool IsStaticBoundaryEnabled() const { return bStaticBoundaryEnabled; }

	TRefCountPtr<FRDGPooledBuffer>& GetStaticBoundaryBuffer() { return PersistentStaticBoundaryBuffer; }
	TRefCountPtr<FRDGPooledBuffer>& GetStaticZOrderSortedBuffer() { return PersistentStaticZOrderSorted; }
	TRefCountPtr<FRDGPooledBuffer>& GetStaticCellStartBuffer() { return PersistentStaticCellStart; }
	TRefCountPtr<FRDGPooledBuffer>& GetStaticCellEndBuffer() { return PersistentStaticCellEnd; }

	bool HasStaticZOrderData() const
	{
		return bStaticZOrderValid
			&& PersistentStaticZOrderSorted.IsValid()
			&& PersistentStaticCellStart.IsValid()
			&& PersistentStaticCellEnd.IsValid();
	}

	void ExecuteStaticBoundaryZOrderSort(FRDGBuilder& GraphBuilder, const FGPUFluidSimulationParams& Params);

	//=========================================================================
	// GPU Boundary Skinning (Persistent Local + GPU Transform)
	//=========================================================================

	void UploadLocalBoundaryParticles(int32 OwnerID, const TArray<FGPUBoundaryParticleLocal>& LocalParticles);

	void UploadBoneTransformsForBoundary(int32 OwnerID, const TArray<FMatrix44f>& BoneTransforms, const FMatrix44f& ComponentTransform);

	void RegisterSkeletalMeshReference(int32 OwnerID, USkeletalMeshComponent* SkelMesh);

	void RefreshAllBoneTransforms();

	void RemoveBoundarySkinningData(int32 OwnerID);

	void ClearAllBoundarySkinningData();

	bool IsGPUBoundarySkinningEnabled() const { return TotalLocalBoundaryParticleCount > 0; }

	int32 GetTotalLocalBoundaryParticleCount() const { return TotalLocalBoundaryParticleCount; }

	//=========================================================================
	// Bone Transform Access (for BoneDeltaAttachment system)
	//=========================================================================

	const TArray<FMatrix44f>* GetBoneTransforms(int32 OwnerID) const;

	const TArray<FMatrix44f>* GetFirstAvailableBoneTransforms(int32* OutOwnerID = nullptr) const;

	int32 GetBoneCount(int32 OwnerID) const;

	//=========================================================================
	// Boundary Adhesion Parameters
	//=========================================================================

	void SetBoundaryAdhesionParams(const FGPUBoundaryAdhesionParams& Params) { CachedBoundaryAdhesionParams = Params; }

	const FGPUBoundaryAdhesionParams& GetBoundaryAdhesionParams() const { return CachedBoundaryAdhesionParams; }

	bool IsBoundaryAdhesionEnabled() const;

	//=========================================================================
	// Boundary Owner AABB (for early-out optimization)
	//=========================================================================

	void UpdateBoundaryOwnerAABB(int32 OwnerID, const FGPUBoundaryOwnerAABB& AABB);

	const FGPUBoundaryOwnerAABB& GetCombinedBoundaryAABB() const { return CombinedBoundaryAABB; }

	bool DoesBoundaryOverlapVolume(const FVector3f& VolumeMin, const FVector3f& VolumeMax, float AdhesionRadius) const;

	bool ShouldSkipBoundaryAdhesionPass(const FGPUFluidSimulationParams& Params) const;

	//=========================================================================
	// RDG Pass (called from simulator)
	//=========================================================================

	struct FBoundarySkinningOutputs
	{
		FRDGBufferRef LocalBoundaryParticlesBuffer = nullptr;
		FRDGBufferRef BoneTransformsBuffer = nullptr;
		int32 BoneCount = 0;
		FMatrix44f ComponentTransform = FMatrix44f::Identity;
	};

	void AddBoundarySkinningPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferRef& OutWorldBoundaryBuffer,
		int32& OutBoundaryParticleCount,
		float DeltaTime,
		FBoundarySkinningOutputs* OutSkinningOutputs = nullptr);

	void AddBoundaryAdhesionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 CurrentParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef InSameFrameBoundaryBuffer = nullptr,
		int32 InSameFrameBoundaryCount = 0,
		FRDGBufferSRVRef InZOrderSortedSRV = nullptr,
		FRDGBufferSRVRef InZOrderCellStartSRV = nullptr,
		FRDGBufferSRVRef InZOrderCellEndSRV = nullptr,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	TRefCountPtr<FRDGPooledBuffer>& GetWorldBoundaryBuffer() { return PersistentWorldBoundaryBuffer; }

	bool HasWorldBoundaryBuffer() const { return PersistentWorldBoundaryBuffer.IsValid(); }

	//=========================================================================
	// Z-Order Sorting for Boundary Particles
	//=========================================================================

	bool ExecuteBoundaryZOrderSort(
		FRDGBuilder& GraphBuilder,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef InSameFrameBoundaryBuffer,
		int32 InSameFrameBoundaryCount,
		FRDGBufferRef& OutSortedBuffer,
		FRDGBufferRef& OutCellStartBuffer,
		FRDGBufferRef& OutCellEndBuffer,
		int32& OutParticleCount);

	void SetBoundaryZOrderConfig(EGridResolutionPreset Preset, const FVector3f& BoundsMin, const FVector3f& BoundsMax)
	{
		GridResolutionPreset = Preset;
		ZOrderBoundsMin = BoundsMin;
		ZOrderBoundsMax = BoundsMax;
	}

	void SetBoundaryZOrderEnabled(bool bEnabled) { bUseBoundaryZOrder = bEnabled; }
	bool IsBoundaryZOrderEnabled() const { return bUseBoundaryZOrder; }

	void SetHybridTiledZOrderEnabled(bool bEnabled) { bUseHybridTiledZOrder = bEnabled; }
	bool IsHybridTiledZOrderEnabled() const { return bUseHybridTiledZOrder; }

	bool HasBoundaryZOrderData() const
	{
		return bBoundaryZOrderValid
			&& PersistentSortedBoundaryBuffer.IsValid()
			&& PersistentBoundaryCellStart.IsValid()
			&& PersistentBoundaryCellEnd.IsValid();
	}

	TRefCountPtr<FRDGPooledBuffer>& GetSortedBoundaryBuffer() { return PersistentSortedBoundaryBuffer; }
	TRefCountPtr<FRDGPooledBuffer>& GetBoundaryCellStartBuffer() { return PersistentBoundaryCellStart; }
	TRefCountPtr<FRDGPooledBuffer>& GetBoundaryCellEndBuffer() { return PersistentBoundaryCellEnd; }

	void MarkBoundaryZOrderDirty() { bBoundaryZOrderDirty = true; }


private:
	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;

	//=========================================================================
	// Static Boundary Particles (StaticMesh colliders - Persistent GPU)
	//=========================================================================

	TRefCountPtr<FRDGPooledBuffer> PersistentStaticBoundaryBuffer;
	TRefCountPtr<FRDGPooledBuffer> PersistentStaticZOrderSorted;
	TRefCountPtr<FRDGPooledBuffer> PersistentStaticCellStart;
	TRefCountPtr<FRDGPooledBuffer> PersistentStaticCellEnd;
	int32 StaticBoundaryParticleCount = 0;
	int32 StaticBoundaryBufferCapacity = 0;
	bool bStaticBoundaryEnabled = false;
	bool bStaticBoundaryDirty = true;
	bool bStaticZOrderValid = false;

	// Temporary CPU storage for upload (cleared after GPU upload)
	TArray<FGPUBoundaryParticle> PendingStaticBoundaryParticles;

	FGPUBoundaryAdhesionParams CachedBoundaryAdhesionParams;

	//=========================================================================
	// GPU Boundary Skinning Data
	//=========================================================================

	/** Skinning data per owner */
	struct FGPUBoundarySkinningData
	{
		int32 OwnerID = -1;
		TArray<FGPUBoundaryParticleLocal> LocalParticles;

		// =====================================================================
		// ATOMIC SWAP DOUBLE BUFFER for thread-safe bone transform transfer
		// Game Thread writes to Buffer[WriteIndex], then swaps WriteIndex
		// Render Thread reads from Buffer[1 - WriteIndex] (the completed buffer)
		// This prevents race conditions without expensive locks
		// =====================================================================
		TArray<FMatrix44f> BoneTransformsBuffer[2];       // Double buffer
		FMatrix44f ComponentTransformBuffer[2];           // Double buffer
		int32 WriteBufferIndex = 0;                       // Swap index (atomic not needed - see below)
		// NOTE: We don't need TAtomic because:
		// 1. Game Thread writes complete BEFORE Render Thread reads (Unreal's frame sync)
		// 2. The index is only modified by Game Thread
		// 3. int32 read/write is atomic on x64

		// Legacy single buffer (kept for compatibility, will be removed)
		TArray<FMatrix44f> BoneTransforms;
		TArray<FMatrix44f> RenderBoneTransforms;
		FMatrix44f ComponentTransform = FMatrix44f::Identity;
		FMatrix44f RenderComponentTransform = FMatrix44f::Identity;

		TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshRef;  // For late bone refresh
		bool bLocalParticlesUploaded = false;

		FGPUBoundarySkinningData()
		{
			ComponentTransformBuffer[0] = FMatrix44f::Identity;
			ComponentTransformBuffer[1] = FMatrix44f::Identity;
		}
	};

	TMap<int32, FGPUBoundarySkinningData> BoundarySkinningDataMap;
	int32 TotalLocalBoundaryParticleCount = 0;

	//=========================================================================
	// Persistent Buffers
	//=========================================================================

	TMap<int32, TRefCountPtr<FRDGPooledBuffer>> PersistentLocalBoundaryBuffers;
	TRefCountPtr<FRDGPooledBuffer> PersistentWorldBoundaryBuffer;
	TRefCountPtr<FRDGPooledBuffer> PreviousWorldBoundaryBuffer;  // For velocity calculation
	int32 WorldBoundaryBufferCapacity = 0;
	bool bHasPreviousFrame = false;  // True after first frame

	//=========================================================================
	// Dirty Tracking
	//=========================================================================

	bool bBoundarySkinningDataDirty = false;

	//=========================================================================
	// Z-Order Sorting Data
	//=========================================================================

	bool bUseBoundaryZOrder = true;
	bool bBoundaryZOrderDirty = true;
	bool bBoundaryZOrderValid = false;

	// Grid configuration (must match fluid simulation)
	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;
	FVector3f ZOrderBoundsMin = FVector3f(-1280.0f, -1280.0f, -1280.0f);
	FVector3f ZOrderBoundsMax = FVector3f(1280.0f, 1280.0f, 1280.0f);

	// Hybrid Tiled Z-Order mode for unlimited simulation range
	bool bUseHybridTiledZOrder = false;

	// Persistent Z-Order buffers
	TRefCountPtr<FRDGPooledBuffer> PersistentSortedBoundaryBuffer;
	TRefCountPtr<FRDGPooledBuffer> PersistentBoundaryCellStart;
	TRefCountPtr<FRDGPooledBuffer> PersistentBoundaryCellEnd;
	int32 BoundaryZOrderBufferCapacity = 0;

	//=========================================================================
	// Boundary Owner AABBs (for early-out optimization)
	//=========================================================================

	TMap<int32, FGPUBoundaryOwnerAABB> BoundaryOwnerAABBs;
	FGPUBoundaryOwnerAABB CombinedBoundaryAABB;  // Combined AABB of all owners
	bool bBoundaryAABBDirty = true;

	/** Recalculate combined AABB from all owner AABBs */
	void RecalculateCombinedAABB();

	//=========================================================================
	// Bone Transform Snapshot Queue (for deferred simulation execution)
	// Prevents race condition: Game thread overwrites bone transforms before
	// Render thread uses them. Each simulation enqueue captures a snapshot.
	//=========================================================================

	/** Lightweight snapshot data (copyable, no atomics) */
	struct FBoneTransformSnapshotData
	{
		int32 OwnerID = -1;
		TArray<FMatrix44f> BoneTransforms;
		FMatrix44f ComponentTransform = FMatrix44f::Identity;
		bool bLocalParticlesUploaded = false;
	};

	/** Snapshotted bone transforms for a single pending simulation */
	struct FBoneTransformSnapshot
	{
		TMap<int32, FBoneTransformSnapshotData> SkinningDataSnapshot;
	};

	/** Queue of bone transform snapshots (one per pending simulation) */
	TArray<FBoneTransformSnapshot> PendingBoneTransformSnapshots;

	/** Currently active snapshot during AddBoundarySkinningPass execution */
	TOptional<FBoneTransformSnapshot> ActiveSnapshot;

public:
	/**
	 * @brief Snapshot current bone transforms for deferred simulation execution.
	 * Call this from EnqueueSimulation() to capture bone transforms at enqueue time.
	 */
	void SnapshotBoneTransformsForPendingSimulation();

	/**
	 * @brief Pop and activate a snapshotted bone transform for execution.
	 * @return true if a snapshot was available and is now active.
	 */
	bool PopAndActivateSnapshot();

	/**
	 * @brief Clear the active snapshot after simulation execution completes.
	 */
	void ClearActiveSnapshot();

	/**
	 * @brief Check if there's an active snapshot being used.
	 * @return true if set.
	 */
	bool HasActiveSnapshot() const { return ActiveSnapshot.IsSet(); }

	/**
	 * @brief Get the active snapshot's skinning data for a specific owner.
	 * @param OwnerID Unique ID.
	 * @return Pointer to snapshot data.
	 */
	const FBoneTransformSnapshotData* GetActiveSnapshotData(int32 OwnerID) const;

private:
	//=========================================================================
	// Thread Safety
	//=========================================================================

	mutable FCriticalSection BoundarySkinningLock;
};
