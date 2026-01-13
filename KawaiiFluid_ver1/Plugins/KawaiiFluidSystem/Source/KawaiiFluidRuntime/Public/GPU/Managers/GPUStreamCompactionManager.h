// Copyright KawaiiFluid Team. All Rights Reserved.
// FGPUStreamCompactionManager - GPU AABB filtering using parallel prefix sum

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "GPU/GPUFluidParticle.h"

class FRDGBuilder;
class FRHICommandListImmediate;

/**
 * FGPUStreamCompactionManager
 *
 * Manages GPU-based Stream Compaction for AABB filtering.
 * Uses parallel prefix sum (Blelloch scan) to efficiently filter particles
 * that are inside specified AABBs for per-polygon collision processing.
 *
 * Pipeline:
 * 1. AABB Mark: Mark particles inside any AABB
 * 2. Prefix Sum: Blelloch scan for compacted output indices
 * 3. Compact: Write marked particles to contiguous output buffer
 * 4. Readback: Get results back to CPU for per-polygon collision
 */
class KAWAIIFLUIDRUNTIME_API FGPUStreamCompactionManager
{
public:
	FGPUStreamCompactionManager();
	~FGPUStreamCompactionManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	/** Initialize with max particle capacity */
	void Initialize(int32 InMaxParticleCount);

	/** Release all GPU resources */
	void Release();

	/** Check if initialized and ready */
	bool IsReady() const { return bIsInitialized && MaxParticleCapacity > 0; }

	//=========================================================================
	// Buffer Management
	//=========================================================================

	/** Allocate GPU buffers for stream compaction (call from render thread) */
	void AllocateBuffers(FRHICommandListImmediate& RHICmdList);

	/** Release GPU buffers */
	void ReleaseBuffers();

	/** Check if buffers are allocated */
	bool AreBuffersAllocated() const { return bBuffersAllocated; }

	//=========================================================================
	// AABB Filtering API
	//=========================================================================

	/**
	 * Execute AABB filtering on GPU
	 * Filters particles inside any of the provided AABBs using Stream Compaction
	 * @param FilterAABBs - Array of AABBs to filter against
	 * @param CurrentParticleCount - Number of active particles
	 * @param PersistentParticleBuffer - Particle buffer (SRV will be created on render thread)
	 * @param FallbackParticleSRV - Fallback SRV if PersistentParticleBuffer is not valid
	 */
	void ExecuteAABBFiltering(
		const TArray<FGPUFilterAABB>& FilterAABBs,
		int32 CurrentParticleCount,
		TRefCountPtr<FRDGPooledBuffer> PersistentParticleBuffer,
		FShaderResourceViewRHIRef FallbackParticleSRV);

	/**
	 * Get filtered candidate particles (GPU→CPU readback)
	 * Call after ExecuteAABBFiltering to get results
	 * @param OutCandidates - Output array of candidate particles
	 * @return true if candidates are available
	 */
	bool GetFilteredCandidates(TArray<FGPUCandidateParticle>& OutCandidates);

	/** Get count of filtered candidates */
	int32 GetFilteredCandidateCount() const { return FilteredCandidateCount; }

	/** Check if filtered candidates are available */
	bool HasFilteredCandidates() const { return bHasFilteredCandidates; }

	//=========================================================================
	// Collision Correction API
	//=========================================================================

	/**
	 * Apply particle corrections from CPU Per-Polygon collision processing
	 * Uploads corrections to GPU and applies them via compute shader
	 * @param Corrections - Array of position corrections from Per-Polygon collision
	 * @param PersistentParticleBuffer - Particle buffer to apply corrections to
	 */
	void ApplyCorrections(
		const TArray<FParticleCorrection>& Corrections,
		TRefCountPtr<FRDGPooledBuffer> PersistentParticleBuffer);

	/**
	 * Apply attached particle position updates
	 * Updates positions for particles attached to skeletal mesh surfaces
	 * Also handles detachment with velocity setting
	 * @param Updates - Array of position updates from attachment system
	 * @param PersistentParticleBuffer - Particle buffer to apply updates to
	 */
	void ApplyAttachmentUpdates(
		const TArray<FAttachedParticleUpdate>& Updates,
		TRefCountPtr<FRDGPooledBuffer> PersistentParticleBuffer);

private:
	//=========================================================================
	// Internal Methods
	//=========================================================================

	/** Dispatch stream compaction compute shaders */
	void DispatchStreamCompactionShaders(
		FRHICommandListImmediate& RHICmdList,
		int32 ParticleCount,
		int32 NumAABBs,
		FShaderResourceViewRHIRef InParticleSRV);

	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;
	int32 MaxParticleCapacity = 0;
	bool bBuffersAllocated = false;

	//=========================================================================
	// Stream Compaction Buffers
	//=========================================================================

	// Marked flags buffer (0 or 1 per particle)
	FBufferRHIRef MarkedFlagsBufferRHI;
	FShaderResourceViewRHIRef MarkedFlagsSRV;
	FUnorderedAccessViewRHIRef MarkedFlagsUAV;

	// Marked AABB index buffer (-1 or interaction index per particle)
	FBufferRHIRef MarkedAABBIndexBufferRHI;
	FShaderResourceViewRHIRef MarkedAABBIndexSRV;
	FUnorderedAccessViewRHIRef MarkedAABBIndexUAV;

	// Prefix sum buffer
	FBufferRHIRef PrefixSumsBufferRHI;
	FShaderResourceViewRHIRef PrefixSumsSRV;
	FUnorderedAccessViewRHIRef PrefixSumsUAV;

	// Block sums buffer for multi-block prefix sum
	FBufferRHIRef BlockSumsBufferRHI;
	FShaderResourceViewRHIRef BlockSumsSRV;
	FUnorderedAccessViewRHIRef BlockSumsUAV;

	// Compacted candidates buffer
	FBufferRHIRef CompactedCandidatesBufferRHI;
	FUnorderedAccessViewRHIRef CompactedCandidatesUAV;

	// Total count buffer (single uint)
	FBufferRHIRef TotalCountBufferRHI;
	FUnorderedAccessViewRHIRef TotalCountUAV;

	// Filter AABBs buffer (uploaded from CPU each frame)
	FBufferRHIRef FilterAABBsBufferRHI;
	FShaderResourceViewRHIRef FilterAABBsSRV;

	// Staging buffers for GPU→CPU readback
	FBufferRHIRef TotalCountStagingBufferRHI;
	FBufferRHIRef CandidatesStagingBufferRHI;

	//=========================================================================
	// Filtering State
	//=========================================================================

	int32 FilteredCandidateCount = 0;
	bool bHasFilteredCandidates = false;
	int32 CurrentFilterAABBCount = 0;
};
