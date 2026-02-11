// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RHIResources.h"
#include "RenderGraphResources.h"
#include "Core/KawaiiFluidRenderParticle.h"
#include <atomic>

class FGPUFluidSimulator;
class FRDGBuilder;

/**
 * @class FKawaiiFluidRenderResource
 * @brief Manages GPU buffers for fluid particle data, enabling high-performance rendering via Niagara or SSFR.
 * 
 * This resource handles both Legacy AoS (Array of Structures) and SoA (Structure of Arrays) formats 
 * to optimize memory bandwidth and integrates directly with the GPU simulator.
 * 
 * @param ParticleBuffer Structured Buffer containing the AoS particle data.
 * @param ParticleSRV Shader Resource View for reading AoS particles in shaders.
 * @param ParticleUAV Unordered Access View for writing particle data (Phase 2 GPU-to-GPU copy).
 * @param PooledParticleBuffer RDG pooled buffer for the legacy AoS path.
 * @param PooledPositionBuffer SoA buffer containing only float3 world positions.
 * @param PooledVelocityBuffer SoA buffer containing only float3 velocities for motion blur and flow.
 * @param PooledBoundsBuffer Min/Max world-space bounds of the active particle set.
 * @param PooledRenderParticleBuffer Buffer containing formatted render-ready particles.
 * @param PooledParticleCountBuffer GPU-side atomic counter for the current particle set.
 * @param ParticleCount Number of valid particles currently stored in the buffer.
 * @param BufferCapacity Maximum number of particles the current GPU buffer can hold.
 * @param bBufferReadyForRendering Atomic flag indicating if the GPU extract pass has completed.
 * @param CachedGPUSimulator Pointer to the GPU simulator if the resource is in GPU simulation mode.
 * @param CachedParticleRadius Base particle radius for rasterization parameters.
 */
class KAWAIIFLUIDRUNTIME_API FKawaiiFluidRenderResource : public FRenderResource
{
public:
	FKawaiiFluidRenderResource();
	virtual ~FKawaiiFluidRenderResource();

	//========================================
	// FRenderResource Interface
	//========================================

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	virtual void ReleaseRHI() override;

	//========================================
	// GPU buffer access
	//========================================

	FRHIShaderResourceView* GetParticleSRV() const { return ParticleSRV; }

	FRHIBuffer* GetParticleBufferRHI() const 
	{ 
		return ParticleBuffer.GetReference(); 
	}

	int32 GetParticleCount() const { return ParticleCount; }

	int32 GetBufferCapacity() const { return BufferCapacity; }

	bool IsValid() const { return ParticleBuffer.IsValid() && ParticleSRV.IsValid(); }

	TRefCountPtr<FRDGPooledBuffer> GetPooledParticleBuffer() const
	{
		return PooledParticleBuffer;
	}

	bool HasValidGPUBuffer() const
	{
		return PooledParticleBuffer.IsValid() && ParticleCount > 0 && bBufferReadyForRendering.load();
	}

	void SetBufferReadyForRendering(bool bReady) { bBufferReadyForRendering = bReady; }

	//========================================
	// SoA (Structure of Arrays) buffer access
	//========================================

	TRefCountPtr<FRDGPooledBuffer> GetPooledPositionBuffer() const
	{
		return PooledPositionBuffer;
	}

	TRefCountPtr<FRDGPooledBuffer> GetPooledVelocityBuffer() const
	{
		return PooledVelocityBuffer;
	}

	bool HasValidSoABuffers() const
	{
		return PooledPositionBuffer.IsValid() && ParticleCount > 0 && bBufferReadyForRendering.load();
	}

	//========================================
	// GPU simulator interface
	//========================================

	void SetGPUSimulatorReference(FGPUFluidSimulator* InSimulator, int32 InMaxParticleCount, float InParticleRadius);

	void ClearGPUSimulatorReference();

	FGPUFluidSimulator* GetGPUSimulator() const { return CachedGPUSimulator; }

	bool HasGPUSimulator() const { return CachedGPUSimulator != nullptr; }

	float GetUnifiedParticleRadius() const { return CachedParticleRadius; }

	FRDGBufferSRVRef GetPhysicsBufferSRV(FRDGBuilder& GraphBuilder) const;

	bool GetAnisotropyBufferSRVs(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef& OutAxis1SRV,
		FRDGBufferSRVRef& OutAxis2SRV,
		FRDGBufferSRVRef& OutAxis3SRV) const;

	bool IsAnisotropyEnabled() const;

	FRDGBufferSRVRef GetRenderOffsetBufferSRV(FRDGBuilder& GraphBuilder) const;

	//========================================
	// Bounds data
	//========================================

	void SetBoundsBuffer(TRefCountPtr<FRDGPooledBuffer> InBoundsBuffer);

	TRefCountPtr<FRDGPooledBuffer> GetPooledBoundsBuffer() const { return PooledBoundsBuffer; }

	bool HasValidBoundsBuffer() const { return PooledBoundsBuffer.IsValid(); }

	void SetRenderParticleBuffer(TRefCountPtr<FRDGPooledBuffer> InBuffer);

	TRefCountPtr<FRDGPooledBuffer> GetPooledRenderParticleBuffer() const { return PooledRenderParticleBuffer; }

	TRefCountPtr<FRDGPooledBuffer>* GetPooledBoundsBufferPtr() { return &PooledBoundsBuffer; }

	TRefCountPtr<FRDGPooledBuffer>* GetPooledRenderParticleBufferPtr() { return &PooledRenderParticleBuffer; }

	TRefCountPtr<FRDGPooledBuffer>* GetPooledPositionBufferPtr() { return &PooledPositionBuffer; }

	TRefCountPtr<FRDGPooledBuffer>* GetPooledVelocityBufferPtr() { return &PooledVelocityBuffer; }

	//========================================
	// GPU Particle Count Buffer
	//========================================

	void SetPersistentParticleCountBuffer(TRefCountPtr<FRDGPooledBuffer> InBuffer) { PooledParticleCountBuffer = InBuffer; }

	TRefCountPtr<FRDGPooledBuffer> GetPooledParticleCountBuffer() const { return PooledParticleCountBuffer; }

	bool HasValidParticleCountBuffer() const { return PooledParticleCountBuffer.IsValid(); }

	//========================================
	// Z-Order buffer access
	//========================================

	TRefCountPtr<FRDGPooledBuffer> GetPooledCellStartBuffer() const;

	TRefCountPtr<FRDGPooledBuffer> GetPooledCellEndBuffer() const;

	bool HasValidZOrderBuffers() const;

private:
	//========================================
	// GPU resources
	//========================================

	FBufferRHIRef ParticleBuffer;

	FShaderResourceViewRHIRef ParticleSRV;

	FUnorderedAccessViewRHIRef ParticleUAV;

	TRefCountPtr<FRDGPooledBuffer> PooledParticleBuffer;

	//========================================
	// SoA (Structure of Arrays) buffers
	//========================================

	TRefCountPtr<FRDGPooledBuffer> PooledPositionBuffer;

	TRefCountPtr<FRDGPooledBuffer> PooledVelocityBuffer;

	TRefCountPtr<FRDGPooledBuffer> PooledBoundsBuffer;

	TRefCountPtr<FRDGPooledBuffer> PooledRenderParticleBuffer;

	TRefCountPtr<FRDGPooledBuffer> PooledParticleCountBuffer;

	int32 ParticleCount;

	int32 BufferCapacity;

	std::atomic<bool> bBufferReadyForRendering{false};

	//========================================
	// GPU simulator reference
	//========================================

	std::atomic<FGPUFluidSimulator*> CachedGPUSimulator{nullptr};

	std::atomic<float> CachedParticleRadius{10.0f};

	//========================================
	// Internal helpers
	//========================================

	bool NeedsResize(int32 NewCount) const;

	void ResizeBuffer(FRHICommandListBase& RHICmdList, int32 NewCapacity);
};
