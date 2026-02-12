// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "Simulation/Resources/GPUFluidSpatialData.h"

class FRHICommandListImmediate;
class FRDGBuilder;
class FGPUCollisionManager;

/**
 * @class FGPUAdhesionManager
 * @brief Manages GPU-based particle adhesion system.
 * 
 * @param bIsInitialized State of the manager.
 * @param AdhesionLock Critical section for thread-safe resource management.
 * @param AdhesionParams Current configuration for the adhesion system.
 * @param PersistentAttachmentBuffer Pooled buffer tracking per-particle attachment states.
 * @param AttachmentBufferSize Current capacity of the attachment buffer.
 */
class KAWAIIFLUIDRUNTIME_API FGPUAdhesionManager
{
public:
	FGPUAdhesionManager();
	~FGPUAdhesionManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	void Initialize();

	void Release();

	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Configuration
	//=========================================================================

	void SetAdhesionParams(const FGPUAdhesionParams& Params) { AdhesionParams = Params; }

	const FGPUAdhesionParams& GetAdhesionParams() const { return AdhesionParams; }

	bool IsAdhesionEnabled() const { return AdhesionParams.bEnableAdhesion != 0; }

	//=========================================================================
	// Attachment Buffer Management
	//=========================================================================

	TRefCountPtr<FRDGPooledBuffer> GetPersistentAttachmentBuffer() const { return PersistentAttachmentBuffer; }

	TRefCountPtr<FRDGPooledBuffer>& AccessPersistentAttachmentBuffer() { return PersistentAttachmentBuffer; }

	int32 GetAttachmentBufferSize() const { return AttachmentBufferSize; }

	void SetAttachmentBufferSize(int32 Size) { AttachmentBufferSize = Size; }

	//=========================================================================
	// Adhesion Passes (called from render thread)
	//=========================================================================

	void AddAdhesionPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		FRDGBufferUAVRef AttachmentUAV,
		FGPUCollisionManager* CollisionManager,
		int32 CurrentParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	void AddUpdateAttachedPositionsPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		FRDGBufferUAVRef AttachmentUAV,
		FGPUCollisionManager* CollisionManager,
		int32 CurrentParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	void AddClearDetachedFlagPass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		int32 CurrentParticleCount,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	void AddStackPressurePass(
		FRDGBuilder& GraphBuilder,
		const FSimulationSpatialData& SpatialData,
		FRDGBufferSRVRef AttachmentSRV,
		FRDGBufferSRVRef CellCountsSRV,
		FRDGBufferSRVRef ParticleIndicesSRV,
		FGPUCollisionManager* CollisionManager,
		int32 CurrentParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

private:
	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;
	mutable FCriticalSection AdhesionLock;

	//=========================================================================
	// Adhesion Parameters
	//=========================================================================

	FGPUAdhesionParams AdhesionParams;

	//=========================================================================
	// Attachment Buffer
	//=========================================================================

	// Persistent attachment buffer (one entry per particle)
	TRefCountPtr<FRDGPooledBuffer> PersistentAttachmentBuffer;
	int32 AttachmentBufferSize = 0;  // Track buffer size for resize detection
};
