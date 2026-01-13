// Copyright KawaiiFluid Team. All Rights Reserved.
// FGPUSpawnManager - Thread-safe particle spawn queue manager

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "GPU/GPUFluidParticle.h"
#include <atomic>

class FRDGBuilder;

/**
 * FGPUSpawnManager
 *
 * Manages thread-safe particle spawn requests for GPU fluid simulation.
 * Game thread queues spawn requests, render thread processes them.
 * Uses double-buffering to avoid lock contention during simulation.
 */
class KAWAIIFLUIDRUNTIME_API FGPUSpawnManager
{
public:
	FGPUSpawnManager();
	~FGPUSpawnManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	/** Initialize the spawn manager with max particle capacity */
	void Initialize(int32 InMaxParticleCount);

	/** Release all resources */
	void Release();

	/** Check if initialized and ready */
	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Thread-Safe Public API (callable from any thread)
	//=========================================================================

	/**
	 * Add a spawn request (thread-safe)
	 * @param Position - World position to spawn at
	 * @param Velocity - Initial velocity
	 * @param Mass - Particle mass (0 = use default)
	 */
	void AddSpawnRequest(const FVector3f& Position, const FVector3f& Velocity, float Mass = 1.0f);

	/**
	 * Add multiple spawn requests at once (thread-safe, more efficient)
	 * @param Requests - Array of spawn requests
	 */
	void AddSpawnRequests(const TArray<FGPUSpawnRequest>& Requests);

	/** Clear all pending spawn requests */
	void ClearSpawnRequests();

	/** Get number of pending spawn requests (thread-safe) */
	int32 GetPendingSpawnCount() const;

	/** Check if there are pending spawn requests (lock-free) */
	bool HasPendingSpawnRequests() const { return bHasPendingSpawnRequests.load(); }

	//=========================================================================
	// Configuration
	//=========================================================================

	/** Set default particle radius for spawning */
	void SetDefaultRadius(float Radius) { DefaultSpawnRadius = Radius; }

	/** Get default particle radius */
	float GetDefaultRadius() const { return DefaultSpawnRadius; }

	/** Set default particle mass (used when spawn request Mass = 0) */
	void SetDefaultMass(float Mass) { DefaultSpawnMass = Mass; }

	/** Get default particle mass */
	float GetDefaultMass() const { return DefaultSpawnMass; }

	/** Get next particle ID that will be assigned */
	int32 GetNextParticleID() const { return NextParticleID.load(); }

	//=========================================================================
	// Render Thread API
	//=========================================================================

	/**
	 * Swap pending requests to active buffer (call at start of simulation frame)
	 * This moves pending requests from game thread buffer to render thread buffer.
	 * Must be called from render thread.
	 */
	void SwapBuffers();

	/** Check if there are active requests to process */
	bool HasActiveRequests() const { return ActiveSpawnRequests.Num() > 0; }

	/** Get number of active spawn requests for this frame */
	int32 GetActiveRequestCount() const { return ActiveSpawnRequests.Num(); }

	/** Get the active spawn requests array (render thread only) */
	const TArray<FGPUSpawnRequest>& GetActiveRequests() const { return ActiveSpawnRequests; }

	/** Clear active requests after processing */
	void ClearActiveRequests() { ActiveSpawnRequests.Empty(); }

	/**
	 * Add spawn particles RDG pass
	 * @param GraphBuilder - RDG builder
	 * @param ParticlesUAV - Particle buffer UAV
	 * @param ParticleCounterUAV - Atomic counter for particle allocation
	 * @param MaxParticleCount - Maximum particle capacity
	 */
	void AddSpawnParticlesPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferUAVRef ParticlesUAV,
		FRDGBufferUAVRef ParticleCounterUAV,
		int32 MaxParticleCount);

	/**
	 * Update next particle ID after spawning
	 * @param SpawnedCount - Number of particles successfully spawned
	 */
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

	// Game thread writes to PendingSpawnRequests
	TArray<FGPUSpawnRequest> PendingSpawnRequests;

	// Render thread reads from ActiveSpawnRequests
	TArray<FGPUSpawnRequest> ActiveSpawnRequests;

	// Lock for PendingSpawnRequests access (mutable for const methods)
	mutable FCriticalSection SpawnLock;

	// Lock-free flag for quick pending check
	std::atomic<bool> bHasPendingSpawnRequests{false};

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
};
