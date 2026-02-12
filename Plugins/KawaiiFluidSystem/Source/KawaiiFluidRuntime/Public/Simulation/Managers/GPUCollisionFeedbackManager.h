// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUCollisionFeedbackManager - Collision feedback system with async GPU readback

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "Simulation/Resources/GPUFluidParticle.h"

class FRHICommandListImmediate;
class FRHIGPUBufferReadback;
class FRDGBuilder;

/**
 * @class FGPUCollisionFeedbackManager
 * @brief Manages GPU collision feedback system for particle-collider interaction.
 *
 * @param bIsInitialized State of the manager.
 * @param bFeedbackEnabled Whether collision feedback recording is active.
 * @param UnifiedFeedbackBuffer GPU buffer containing merged feedback data and counters.
 * @param ColliderContactCountBuffer GPU buffer for per-collider contact counts.
 * @param UnifiedFeedbackReadbacks Triple-buffered readback objects for unified feedback.
 * @param ContactCountReadbacks Triple-buffered readback objects for contact counts.
 * @param FeedbackLock Critical section for thread-safe access to ready data.
 * @param ReadyFeedback Cached bone collider feedback data.
 * @param ReadyFeedbackCount Number of bone feedback entries available.
 * @param ReadyStaticMeshFeedback Cached StaticMesh collider feedback data.
 * @param ReadyStaticMeshFeedbackCount Number of StaticMesh feedback entries available.
 * @param ReadyFluidInteractionSMFeedback Cached FluidInteraction StaticMesh feedback data.
 * @param ReadyFluidInteractionSMFeedbackCount Number of FISM feedback entries available.
 * @param ReadyContactCounts Cached contact counts per collider.
 * @param CurrentWriteIndex Index for triple buffering (0-2).
 * @param FeedbackFrameNumber Frame counter for feedback processing.
 * @param ContactCountFrameNumber Frame counter for contact count processing.
 * @param CompletedFeedbackFrame Last completed feedback frame number.
 */
class KAWAIIFLUIDRUNTIME_API FGPUCollisionFeedbackManager
{
public:
	FGPUCollisionFeedbackManager();
	~FGPUCollisionFeedbackManager();

	//=========================================================================
	// Constants
	//=========================================================================

	static constexpr int32 MAX_COLLISION_FEEDBACK = 4096;
	static constexpr int32 MAX_STATICMESH_COLLISION_FEEDBACK = 1024;
	static constexpr int32 MAX_FLUIDINTERACTION_SM_FEEDBACK = 1024;
	static constexpr int32 NUM_FEEDBACK_BUFFERS = 3;
	static constexpr int32 MAX_COLLIDER_COUNT = 256;

	//=========================================================================
	// Unified Buffer Layout Constants
	//=========================================================================

	static constexpr int32 UNIFIED_HEADER_SIZE = 16;
	static constexpr int32 BONE_FEEDBACK_OFFSET = UNIFIED_HEADER_SIZE;
	static constexpr int32 SM_FEEDBACK_OFFSET = BONE_FEEDBACK_OFFSET + MAX_COLLISION_FEEDBACK * 96;
	static constexpr int32 FISM_FEEDBACK_OFFSET = SM_FEEDBACK_OFFSET + MAX_STATICMESH_COLLISION_FEEDBACK * 96;
	static constexpr int32 UNIFIED_BUFFER_SIZE = FISM_FEEDBACK_OFFSET + MAX_FLUIDINTERACTION_SM_FEEDBACK * 96;

	//=========================================================================
	// Lifecycle
	//=========================================================================

	void Initialize();

	void Release();

	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Enable/Disable
	//=========================================================================

	void SetEnabled(bool bEnabled) { bFeedbackEnabled = bEnabled; }

	bool IsEnabled() const { return bFeedbackEnabled; }

	//=========================================================================
	// Buffer Management (called from render thread)
	//=========================================================================

	void AllocateReadbackObjects(FRHICommandListImmediate& RHICmdList);

	void ReleaseReadbackObjects();

	TRefCountPtr<FRDGPooledBuffer>& GetUnifiedFeedbackBuffer() { return UnifiedFeedbackBuffer; }

	TRefCountPtr<FRDGPooledBuffer>& GetContactCountBuffer() { return ColliderContactCountBuffer; }

	//=========================================================================
	// Readback Processing (called from render thread)
	//=========================================================================

	void ProcessFeedbackReadback(FRHICommandListImmediate& RHICmdList);

	void ProcessContactCountReadback(FRHICommandListImmediate& RHICmdList);

	void EnqueueReadbackCopy(FRHICommandListImmediate& RHICmdList);

	void IncrementFrameCounter();

	int32 GetCurrentWriteIndex() const { return CurrentWriteIndex; }

	//=========================================================================
	// Query API (thread-safe)
	//=========================================================================

	bool GetFeedbackForCollider(int32 ColliderIndex, TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	bool GetAllFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	int32 GetFeedbackCount() const { return ReadyFeedbackCount; }

	bool GetAllStaticMeshFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	int32 GetStaticMeshFeedbackCount() const { return ReadyStaticMeshFeedbackCount; }

	bool GetAllFluidInteractionSMFeedback(TArray<FGPUCollisionFeedback>& OutFeedback, int32& OutCount);

	int32 GetFluidInteractionSMFeedbackCount() const { return ReadyFluidInteractionSMFeedbackCount; }

	int32 GetContactCount(int32 ColliderIndex) const;

	void GetAllContactCounts(TArray<int32>& OutCounts) const;


private:
	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;
	bool bFeedbackEnabled = false;

	//=========================================================================
	// GPU Buffers (managed via RDG extraction)
	//=========================================================================

	// Unified feedback buffer containing all feedback types with embedded counters
	// Layout: [Header:16B][BoneFeedback][SMFeedback][FISMFeedback]
	TRefCountPtr<FRDGPooledBuffer> UnifiedFeedbackBuffer;

	// Contact count buffer (separate, needed even when feedback disabled)
	TRefCountPtr<FRDGPooledBuffer> ColliderContactCountBuffer;

	//=========================================================================
	// Async Readback Objects (triple buffered)
	//=========================================================================

	// Unified feedback readback (replaces 6 separate readbacks)
	FRHIGPUBufferReadback* UnifiedFeedbackReadbacks[NUM_FEEDBACK_BUFFERS] = {nullptr};

	// Contact count readback (unchanged)
	FRHIGPUBufferReadback* ContactCountReadbacks[NUM_FEEDBACK_BUFFERS] = {nullptr};

	//=========================================================================
	// Ready Data (thread-safe access via lock)
	//=========================================================================

	mutable FCriticalSection FeedbackLock;

	// Bone collider ready data (BoneIndex >= 0)
	TArray<FGPUCollisionFeedback> ReadyFeedback;
	int32 ReadyFeedbackCount = 0;

	// StaticMesh collider ready data (BoneIndex < 0, bHasFluidInteraction = 0, WorldCollision)
	TArray<FGPUCollisionFeedback> ReadyStaticMeshFeedback;
	int32 ReadyStaticMeshFeedbackCount = 0;

	// FluidInteraction StaticMesh ready data (BoneIndex < 0, bHasFluidInteraction = 1)
	TArray<FGPUCollisionFeedback> ReadyFluidInteractionSMFeedback;
	int32 ReadyFluidInteractionSMFeedbackCount = 0;

	TArray<int32> ReadyContactCounts;

	//=========================================================================
	// Triple Buffer State
	//=========================================================================

	int32 CurrentWriteIndex = 0;
	int32 FeedbackFrameNumber = 0;
	int32 ContactCountFrameNumber = 0;
	std::atomic<int32> CompletedFeedbackFrame{-1};
};
