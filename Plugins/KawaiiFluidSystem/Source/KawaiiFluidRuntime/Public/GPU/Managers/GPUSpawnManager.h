// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUSpawnManager - Thread-safe particle spawn queue manager

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "GPU/GPUFluidParticle.h"
#include <atomic>

class FRHIGPUBufferReadback;
class FRDGBuilder;

/**
 * @class FGPUSpawnManager
 * @brief Manages thread-safe particle spawn requests for GPU fluid simulation.
 * 
 * @param bIsInitialized State of the manager.
 * @param MaxParticleCapacity Maximum number of particles the system can handle.
 * @param PendingSpawnRequests Queue for new spawn requests from any thread.
 * @param ActiveSpawnRequests Buffer for requests being processed by the render thread.
 * @param SpawnLock Critical section for spawn request thread safety.
 * @param bHasPendingSpawnRequests Atomic flag for quick pending check.
 * @param PendingGPUBrushDespawns Queue for brush-based despawn requests.
 * @param ActiveGPUBrushDespawns Buffer for brush despawns being processed.
 * @param PendingGPUSourceDespawns Queue for source-based despawn requests.
 * @param ActiveGPUSourceDespawns Buffer for source despawns being processed.
 * @param GPUDespawnLock Critical section for despawn request thread safety.
 * @param bHasPendingGPUDespawnRequests Atomic flag for quick despawn pending check.
 * @param PersistentIDHistogramBuffer GPU buffer for ID distribution histogram.
 * @param PersistentOldestThresholdBuffer GPU buffer for oldest-particle ID thresholds.
 * @param PersistentBoundaryCounterBuffer GPU buffer for atomic counter.
 * @param EmitterMaxCountsCPU CPU-side per-source particle limits.
 * @param bEmitterMaxCountsDirty Flag indicating need to update GPU emitter limits.
 * @param ActiveEmitterMaxCount Number of sources with active limits.
 * @param PersistentEmitterMaxCountsBuffer GPU buffer for source limits.
 * @param PersistentPerSourceExcessBuffer GPU buffer for excess particle counts per source.
 * @param NextParticleID Atomic counter for assigning unique particle IDs.
 * @param DefaultSpawnRadius Default radius assigned to new particles.
 * @param DefaultSpawnMass Default mass assigned to new particles.
 * @param SourceCounterBuffer GPU buffer tracking particle count per source.
 * @param SourceCounterReadbacks Ring buffer for async GPU->CPU source count transfers.
 * @param SourceCounterWriteIndex Current write index in readback ring buffer.
 * @param SourceCounterReadIndex Current read index in readback ring buffer.
 * @param SourceCounterPendingCount Number of active readback operations.
 * @param CachedSourceCounts Locally cached particle counts per source.
 * @param SourceCountLock Critical section for source count access.
 * @param PersistentAliveMaskBuffer Reusable GPU buffer for particle survival flags.
 * @param PersistentPrefixSumsBuffer Reusable GPU buffer for parallel scan outputs.
 * @param PersistentBlockSumsBuffer Reusable GPU buffer for block-level scan data.
 * @param PersistentCompactedBuffer Reusable double-buffered GPU buffers for compaction output.
 * @param CompactedBufferIndex Swap index for double-buffered output.
 * @param StreamCompactionCapacity Current capacity of compaction buffers.
 */
class KAWAIIFLUIDRUNTIME_API FGPUSpawnManager
{
public:
	FGPUSpawnManager();
	~FGPUSpawnManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	void Initialize(int32 InMaxParticleCount);

	void Release();

	bool IsReady() const { return bIsInitialized; }

	void Reset()
	{
		FScopeLock SpawnGuard(&SpawnLock);
		FScopeLock DespawnGuard(&GPUDespawnLock);

		PendingSpawnRequests.Empty();
		ActiveSpawnRequests.Empty();
		PendingGPUBrushDespawns.Empty();
		ActiveGPUBrushDespawns.Empty();
		PendingGPUSourceDespawns.Empty();
		ActiveGPUSourceDespawns.Empty();

		bHasPendingSpawnRequests.store(false);
		bHasPendingGPUDespawnRequests.store(false);
		NextParticleID.store(0);
	}

	void ClearDespawnTracking()
	{
		FScopeLock Lock(&GPUDespawnLock);
		PendingGPUBrushDespawns.Empty();
		ActiveGPUBrushDespawns.Empty();
		PendingGPUSourceDespawns.Empty();
		ActiveGPUSourceDespawns.Empty();
		bHasPendingGPUDespawnRequests.store(false);
	}

	//=========================================================================
	// Thread-Safe Public API (callable from any thread)
	//=========================================================================

	void AddSpawnRequest(const FVector3f& Position, const FVector3f& Velocity, float Mass = 1.0f);

	void AddSpawnRequests(const TArray<FGPUSpawnRequest>& Requests);

	void ClearSpawnRequests();

	int32 CancelPendingSpawnsForSource(int32 SourceID);

	int32 GetPendingSpawnCount() const;

	bool HasPendingSpawnRequests() const { return bHasPendingSpawnRequests.load(); }

	//=========================================================================
	// GPU-Driven Despawn API (Thread-Safe)
	//=========================================================================

	void AddGPUDespawnBrushRequest(const FVector3f& Center, float Radius);

	void AddGPUDespawnSourceRequest(int32 SourceID);

	void SetSourceEmitterMax(int32 SourceID, int32 MaxCount);

	bool HasPerSourceRecycle() const { return ActiveEmitterMaxCount > 0; }

	bool HasPendingGPUDespawnRequests() const { return bHasPendingGPUDespawnRequests.load(); }

	bool SwapGPUDespawnBuffers();

	void AddGPUDespawnPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferRef& InOutParticleBuffer,
		int32& InOutParticleCount,
		int32 NextParticleIDHint,
		FRDGBufferRef ParticleCountBuffer);

	//=========================================================================
	// Source Counter API (Per-Component Particle Count Tracking)
	//=========================================================================

	TRefCountPtr<FRDGPooledBuffer> GetSourceCounterBuffer() const { return SourceCounterBuffer; }

	FRDGBufferUAVRef RegisterSourceCounterUAV(FRDGBuilder& GraphBuilder);

	int32 GetParticleCountForSource(int32 SourceID) const;

	TArray<int32> GetAllSourceCounts() const;

	void EnqueueSourceCounterReadback(FRHICommandListImmediate& RHICmdList);

	void ProcessSourceCounterReadback();

	void ClearSourceCounters(FRDGBuilder& GraphBuilder);

	void InitializeSourceCountersFromParticles(const TArray<FGPUFluidParticle>& Particles);

	//=========================================================================
	// Configuration
	//=========================================================================

	void SetDefaultRadius(float Radius) { DefaultSpawnRadius = Radius; }

	float GetDefaultRadius() const { return DefaultSpawnRadius; }

	void SetDefaultMass(float Mass) { DefaultSpawnMass = Mass; }

	float GetDefaultMass() const { return DefaultSpawnMass; }

	int32 GetNextParticleID() const { return NextParticleID.load(); }

	void SetNextParticleID(int32 ID) { NextParticleID.store(ID); }

	int32 AllocateParticleIDs(int32 Count) { return NextParticleID.fetch_add(Count); }

	void TryResetParticleID(int32 CurrentParticleCount)
	{
		if (CurrentParticleCount == 0)
		{
			NextParticleID.store(0);
		}
	}

	//=========================================================================
	// Render Thread API
	//=========================================================================

	void SwapBuffers();

	bool HasActiveRequests() const { return ActiveSpawnRequests.Num() > 0; }

	int32 GetActiveRequestCount() const { return ActiveSpawnRequests.Num(); }

	const TArray<FGPUSpawnRequest>& GetActiveRequests() const { return ActiveSpawnRequests; }

	void ClearActiveRequests() { ActiveSpawnRequests.Empty(); }

	void AddSpawnParticlesPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferUAVRef ParticlesUAV,
		FRDGBufferUAVRef ParticleCounterUAV,
		int32 MaxParticleCount);

	void OnSpawnComplete(int32 SpawnedCount);

private:
	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;
	int32 MaxParticleCapacity = 0;

	//=========================================================================
	// Double-Buffered Spawn Requests
	//=========================================================================
	TArray<FGPUSpawnRequest> PendingSpawnRequests;
	TArray<FGPUSpawnRequest> ActiveSpawnRequests;
	mutable FCriticalSection SpawnLock;

	// Lock-free flag for quick pending check
	std::atomic<bool> bHasPendingSpawnRequests{false};

	//=========================================================================
	// Double-Buffered GPU-Driven Despawn Requests
	//=========================================================================
	TArray<FGPUDespawnBrushRequest> PendingGPUBrushDespawns;
	TArray<FGPUDespawnBrushRequest> ActiveGPUBrushDespawns;
	TArray<int32> PendingGPUSourceDespawns;
	TArray<int32> ActiveGPUSourceDespawns;
	mutable FCriticalSection GPUDespawnLock;

	// Lock-free flag for quick pending check
	std::atomic<bool> bHasPendingGPUDespawnRequests{false};

	// Persistent buffers for Oldest despawn histogram
	TRefCountPtr<FRDGPooledBuffer> PersistentIDHistogramBuffer;      // uint32 x 256
	TRefCountPtr<FRDGPooledBuffer> PersistentOldestThresholdBuffer;  // uint32 x 2
	TRefCountPtr<FRDGPooledBuffer> PersistentBoundaryCounterBuffer;  // uint32 x 1

	//=========================================================================
	// Per-Source Emitter Max (GPU-Driven Recycle)
	//=========================================================================
	TArray<int32> EmitterMaxCountsCPU;                                // [MaxSourceCount], 0 = no limit
	bool bEmitterMaxCountsDirty = false;
	int32 ActiveEmitterMaxCount = 0;                                  // Count of non-zero entries
	TRefCountPtr<FRDGPooledBuffer> PersistentEmitterMaxCountsBuffer;  // uint32 x MaxSourceCount
	TRefCountPtr<FRDGPooledBuffer> PersistentPerSourceExcessBuffer;   // uint32 x MaxSourceCount

	//=========================================================================
	// Particle ID Tracking
	//=========================================================================

	// Next particle ID to assign (atomic for thread safety)
	std::atomic<int32> NextParticleID{0};

	//=========================================================================
	// Configuration
	//=========================================================================

	float DefaultSpawnRadius = 5.0f;
	float DefaultSpawnMass = 1.0f;

	//=========================================================================
	// Source Counter (Per-Component Particle Count)
	//=========================================================================

	// GPU buffer storing per-source particle counts [MaxSourceCount]
	TRefCountPtr<FRDGPooledBuffer> SourceCounterBuffer;

	// Ring buffer for async GPU→CPU readback (handles GPU latency)
	static constexpr int32 SourceCounterRingBufferSize = 4;
	TArray<FRHIGPUBufferReadback*> SourceCounterReadbacks;
	int32 SourceCounterWriteIndex = 0;  // Next slot to write (enqueue)
	int32 SourceCounterReadIndex = 0;   // Next slot to read (process)
	int32 SourceCounterPendingCount = 0; // Number of pending readbacks

	// CPU-cached source counts (updated from readback)
	TArray<int32> CachedSourceCounts;
	mutable FCriticalSection SourceCountLock;

	//=========================================================================
	// Persistent Stream Compaction Buffers (for Despawn)
	//=========================================================================

	// Reusable buffers to avoid per-frame allocation overhead
	TRefCountPtr<FRDGPooledBuffer> PersistentAliveMaskBuffer;
	TRefCountPtr<FRDGPooledBuffer> PersistentPrefixSumsBuffer;
	TRefCountPtr<FRDGPooledBuffer> PersistentBlockSumsBuffer;
	TRefCountPtr<FRDGPooledBuffer> PersistentCompactedBuffer[2];  // Double-buffered for output
	int32 CompactedBufferIndex = 0;
	int32 StreamCompactionCapacity = 0;

	/** Ensure stream compaction buffers are allocated with sufficient capacity */
	void EnsureStreamCompactionBuffers(FRDGBuilder& GraphBuilder, int32 RequiredCapacity);

public:
	/** Get SRV for the last PrefixSums buffer (after despawn compaction) */
	FRDGBufferSRVRef GetLastPrefixSumsSRV(FRDGBuilder& GraphBuilder) const;

	/** Get SRV for the last AliveMask buffer (after despawn marking) */
	FRDGBufferSRVRef GetLastAliveMaskSRV(FRDGBuilder& GraphBuilder) const;
};
