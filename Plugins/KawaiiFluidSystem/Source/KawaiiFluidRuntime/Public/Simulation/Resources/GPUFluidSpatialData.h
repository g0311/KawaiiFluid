// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// GPU Fluid Spatial Data Structures

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

/**
 * @struct FSimulationSpatialData
 * @brief Contains all spatial hashing and SOA particle buffers for GPU fluid simulation.
 * 
 * @param CellCountsBuffer Legacy hash table cell counts buffer.
 * @param ParticleIndicesBuffer Legacy hash table particle indices buffer.
 * @param CellCountsSRV SRV for legacy cell counts.
 * @param ParticleIndicesSRV SRV for legacy particle indices.
 * @param CellStartBuffer Z-Order sorted cell start indices buffer.
 * @param CellEndBuffer Z-Order sorted cell end indices buffer.
 * @param CellStartSRV SRV for cell start indices.
 * @param CellEndSRV SRV for cell end indices.
 * @param NeighborListBuffer Neighbor list cache buffer.
 * @param NeighborCountsBuffer Neighbor count cache buffer.
 * @param NeighborListSRV SRV for neighbor list.
 * @param NeighborCountsSRV SRV for neighbor counts.
 * @param SkinnedBoundaryBuffer Skinned world-space boundary particles (SkeletalMesh).
 * @param SkinnedBoundarySRV SRV for skinned boundaries.
 * @param SkinnedBoundaryParticleCount Number of skinned boundary particles.
 * @param bSkinnedBoundaryPerformed Whether skinning was executed this frame.
 * @param SkinnedZOrderSortedBuffer Z-Order sorted skinned boundary particles.
 * @param SkinnedZOrderCellStartBuffer Cell start indices for sorted skinned boundaries.
 * @param SkinnedZOrderCellEndBuffer Cell end indices for sorted skinned boundaries.
 * @param SkinnedZOrderSortedSRV SRV for sorted skinned boundaries.
 * @param SkinnedZOrderCellStartSRV SRV for skinned boundary cell start.
 * @param SkinnedZOrderCellEndSRV SRV for skinned boundary cell end.
 * @param SkinnedZOrderParticleCount Number of sorted skinned boundary particles.
 * @param bSkinnedZOrderPerformed Whether skinned Z-Order sort was executed.
 * @param StaticBoundarySRV SRV for persistent static boundary particles (StaticMesh).
 * @param StaticZOrderSortedSRV SRV for sorted static boundary particles.
 * @param StaticZOrderCellStartSRV SRV for static boundary cell start.
 * @param StaticZOrderCellEndSRV SRV for static boundary cell end.
 * @param StaticBoundaryParticleCount Number of static boundary particles.
 * @param bStaticBoundaryAvailable Whether static boundary data is valid and cached on GPU.
 * @param BoneDeltaAttachmentBuffer Buffer for per-particle bone delta attachment (simplified bone-following).
 * @param BoneDeltaAttachmentUAV UAV for bone delta attachment.
 * @param BoneDeltaAttachmentSRV SRV for bone delta attachment.
 * @param SoA_Positions Full-precision SoA positions buffer (float3).
 * @param SoA_PredictedPositions Full-precision SoA predicted positions buffer (float3).
 * @param SoA_PackedVelocities Half-precision packed SoA velocities (uint2 = half4).
 * @param SoA_PackedDensityLambda Half-precision packed SoA density and lambda (uint = half2).
 * @param SoA_Flags Particle state flags buffer (uint).
 * @param SoA_NeighborCounts Particle neighbor counts buffer (uint).
 * @param SoA_ParticleIDs Particle persistent IDs buffer (int).
 * @param SoA_SourceIDs Particle source IDs buffer (int).
 * @param WorldBoundaryBuffer Legacy alias for SkinnedBoundaryBuffer.
 * @param WorldBoundarySRV Legacy alias for SkinnedBoundarySRV.
 * @param WorldBoundaryParticleCount Legacy alias for SkinnedBoundaryParticleCount.
 * @param bBoundarySkinningPerformed Legacy alias for bSkinnedBoundaryPerformed.
 * @param BoundaryZOrderSortedBuffer Legacy alias for SkinnedZOrderSortedBuffer.
 * @param BoundaryZOrderCellStartBuffer Legacy alias for SkinnedZOrderCellStartBuffer.
 * @param BoundaryZOrderCellEndBuffer Legacy alias for SkinnedZOrderCellEndBuffer.
 * @param BoundaryZOrderSortedSRV Legacy alias for SkinnedZOrderSortedSRV.
 * @param BoundaryZOrderCellStartSRV Legacy alias for SkinnedZOrderCellStartSRV.
 * @param BoundaryZOrderCellEndSRV Legacy alias for SkinnedZOrderCellEndSRV.
 * @param BoundaryZOrderParticleCount Legacy alias for SkinnedZOrderParticleCount.
 * @param bBoundaryZOrderPerformed Legacy alias for bSkinnedZOrderPerformed.
 */
struct FSimulationSpatialData
{
	// Hash Table buffers (Legacy / Compatibility)
	FRDGBufferRef CellCountsBuffer = nullptr;
	FRDGBufferRef ParticleIndicesBuffer = nullptr;
	FRDGBufferSRVRef CellCountsSRV = nullptr;
	FRDGBufferSRVRef ParticleIndicesSRV = nullptr;

	// Z-Order buffers (Sorted)
	FRDGBufferRef CellStartBuffer = nullptr;
	FRDGBufferRef CellEndBuffer = nullptr;
	FRDGBufferSRVRef CellStartSRV = nullptr;
	FRDGBufferSRVRef CellEndSRV = nullptr;


	// Neighbor Cache buffers
	FRDGBufferRef NeighborListBuffer = nullptr;
	FRDGBufferRef NeighborCountsBuffer = nullptr;
	FRDGBufferSRVRef NeighborListSRV = nullptr;
	FRDGBufferSRVRef NeighborCountsSRV = nullptr;

	// Skinned Boundary Particle buffers (SkeletalMesh - same-frame)
	FRDGBufferRef SkinnedBoundaryBuffer = nullptr;
	FRDGBufferSRVRef SkinnedBoundarySRV = nullptr;
	int32 SkinnedBoundaryParticleCount = 0;
	bool bSkinnedBoundaryPerformed = false;

	// Skinned Boundary Z-Order buffers (same-frame)
	FRDGBufferRef SkinnedZOrderSortedBuffer = nullptr;
	FRDGBufferRef SkinnedZOrderCellStartBuffer = nullptr;
	FRDGBufferRef SkinnedZOrderCellEndBuffer = nullptr;
	FRDGBufferSRVRef SkinnedZOrderSortedSRV = nullptr;
	FRDGBufferSRVRef SkinnedZOrderCellStartSRV = nullptr;
	FRDGBufferSRVRef SkinnedZOrderCellEndSRV = nullptr;
	int32 SkinnedZOrderParticleCount = 0;
	bool bSkinnedZOrderPerformed = false;

	// Static Boundary Particle buffers (StaticMesh - persistent GPU)
	FRDGBufferSRVRef StaticBoundarySRV = nullptr;
	FRDGBufferSRVRef StaticZOrderSortedSRV = nullptr;
	FRDGBufferSRVRef StaticZOrderCellStartSRV = nullptr;
	FRDGBufferSRVRef StaticZOrderCellEndSRV = nullptr;
	int32 StaticBoundaryParticleCount = 0;
	bool bStaticBoundaryAvailable = false;

	// Bone Delta Attachment buffer (NEW simplified bone-following)
	FRDGBufferRef BoneDeltaAttachmentBuffer = nullptr;
	FRDGBufferUAVRef BoneDeltaAttachmentUAV = nullptr;
	FRDGBufferSRVRef BoneDeltaAttachmentSRV = nullptr;

	// SoA (Structure of Arrays) Particle Buffers (Memory Bandwidth Optimization)
	FRDGBufferRef SoA_Positions = nullptr;           // float3 as 3 floats
	FRDGBufferRef SoA_PredictedPositions = nullptr;  // float3 as 3 floats
	FRDGBufferRef SoA_PackedVelocities = nullptr;    // uint2 = half4 (vel.xy, vel.z, padding)
	FRDGBufferRef SoA_PackedDensityLambda = nullptr; // uint = half2 (density, lambda)
	FRDGBufferRef SoA_Flags = nullptr;               // uint
	FRDGBufferRef SoA_NeighborCounts = nullptr;      // uint
	FRDGBufferRef SoA_ParticleIDs = nullptr;         // int
	FRDGBufferRef SoA_SourceIDs = nullptr;           // int

	// Legacy aliases (for backward compatibility during transition)
	FRDGBufferRef& WorldBoundaryBuffer = SkinnedBoundaryBuffer;
	FRDGBufferSRVRef& WorldBoundarySRV = SkinnedBoundarySRV;
	int32& WorldBoundaryParticleCount = SkinnedBoundaryParticleCount;
	bool& bBoundarySkinningPerformed = bSkinnedBoundaryPerformed;

	FRDGBufferRef& BoundaryZOrderSortedBuffer = SkinnedZOrderSortedBuffer;
	FRDGBufferRef& BoundaryZOrderCellStartBuffer = SkinnedZOrderCellStartBuffer;
	FRDGBufferRef& BoundaryZOrderCellEndBuffer = SkinnedZOrderCellEndBuffer;
	FRDGBufferSRVRef& BoundaryZOrderSortedSRV = SkinnedZOrderSortedSRV;
	FRDGBufferSRVRef& BoundaryZOrderCellStartSRV = SkinnedZOrderCellStartSRV;
	FRDGBufferSRVRef& BoundaryZOrderCellEndSRV = SkinnedZOrderCellEndSRV;
	int32& BoundaryZOrderParticleCount = SkinnedZOrderParticleCount;
	bool& bBoundaryZOrderPerformed = bSkinnedZOrderPerformed;
};
