// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUZOrderSortManager - Z-Order Morton Code Sorting System

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "Core/KawaiiFluidSimulationTypes.h"  // For EGridResolutionPreset

class FRDGBuilder;

/**
 * @class FGPUZOrderSortManager
 * @brief Manages GPU-based particle sorting using Z-Order (Morton Code) curve.
 * 
 * @param bIsInitialized State of the manager.
 * @param bUseZOrderSorting Whether Z-Order sorting is enabled.
 * @param GridResolutionPreset Resolution preset for classic Morton code sorting.
 * @param bUseHybridTiledZOrder Whether to use hybrid tiling for unlimited simulation range.
 * @param SimulationBoundsMin Minimum world coordinates for classic Morton computation.
 * @param SimulationBoundsMax Maximum world coordinates for classic Morton computation.
 * @param ZOrderBufferParticleCapacity Current capacity of sorting-related GPU buffers.
 */
class KAWAIIFLUIDRUNTIME_API FGPUZOrderSortManager
{
public:
	FGPUZOrderSortManager();
	~FGPUZOrderSortManager();

	//=========================================================================
	// Lifecycle
	//=========================================================================

	void Initialize();

	void Release();

	bool IsReady() const { return bIsInitialized; }

	//=========================================================================
	// Configuration
	//=========================================================================

	void SetSimulationBounds(const FVector3f& BoundsMin, const FVector3f& BoundsMax)
	{
		SimulationBoundsMin = BoundsMin;
		SimulationBoundsMax = BoundsMax;
	}

	void GetSimulationBounds(FVector3f& OutMin, FVector3f& OutMax) const
	{
		OutMin = SimulationBoundsMin;
		OutMax = SimulationBoundsMax;
	}

	void SetZOrderSortingEnabled(bool bEnabled) { bUseZOrderSorting = bEnabled; }

	bool IsZOrderSortingEnabled() const { return bUseZOrderSorting; }

	void SetGridResolutionPreset(EGridResolutionPreset Preset) { GridResolutionPreset = Preset; }

	EGridResolutionPreset GetGridResolutionPreset() const { return GridResolutionPreset; }

	EGridResolutionPreset GetEffectiveGridResolutionPreset() const
	{
		return bUseHybridTiledZOrder ? EGridResolutionPreset::Medium : GridResolutionPreset;
	}

	void SetHybridTiledZOrderEnabled(bool bEnabled) { bUseHybridTiledZOrder = bEnabled; }

	bool IsHybridTiledZOrderEnabled() const { return bUseHybridTiledZOrder; }

	//=========================================================================
	// Z-Order Sorting Pipeline
	//=========================================================================

	FRDGBufferRef ExecuteZOrderSortingPipeline(
		FRDGBuilder& GraphBuilder,
		FRDGBufferRef InParticleBuffer,
		FRDGBufferUAVRef& OutCellStartUAV,
		FRDGBufferSRVRef& OutCellStartSRV,
		FRDGBufferUAVRef& OutCellEndUAV,
		FRDGBufferSRVRef& OutCellEndSRV,
		FRDGBufferRef& OutCellStartBuffer,
		FRDGBufferRef& OutCellEndBuffer,
		int32 CurrentParticleCount,
		const FGPUFluidSimulationParams& Params,
		int32 AllocParticleCount = 0,
		// Optional: BoneDeltaAttachment buffer to reorder along with particles
		FRDGBufferRef InAttachmentBuffer = nullptr,
		FRDGBufferRef* OutSortedAttachmentBuffer = nullptr,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

private:
	//=========================================================================
	// Internal Passes
	//=========================================================================

	void AddComputeMortonCodesPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef ParticlesSRV,
		FRDGBufferUAVRef MortonCodesUAV,
		FRDGBufferUAVRef InParticleIndicesUAV,
		int32 CurrentParticleCount,
		const FGPUFluidSimulationParams& Params,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	void AddRadixSortPasses(
		FRDGBuilder& GraphBuilder,
		FRDGBufferRef& InOutMortonCodes,
		FRDGBufferRef& InOutParticleIndices,
		int32 ParticleCount,
		int32 AllocParticleCount = 0);

	void AddReorderParticlesPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef OldParticlesSRV,
		FRDGBufferSRVRef SortedIndicesSRV,
		FRDGBufferUAVRef SortedParticlesUAV,
		int32 CurrentParticleCount,
		// Optional: BoneDeltaAttachment reordering
		FRDGBufferSRVRef OldAttachmentsSRV = nullptr,
		FRDGBufferUAVRef SortedAttachmentsUAV = nullptr,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	void AddComputeCellStartEndPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef SortedMortonCodesSRV,
		FRDGBufferUAVRef CellStartUAV,
		FRDGBufferUAVRef CellEndUAV,
		int32 CurrentParticleCount,
		FRDGBufferRef IndirectArgsBuffer = nullptr);

	//=========================================================================
	// State
	//=========================================================================

	bool bIsInitialized = false;

	//=========================================================================
	// Configuration
	//=========================================================================

	bool bUseZOrderSorting = true;

	// Grid resolution preset for shader permutation selection
	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;

	// Hybrid Tiled Z-Order mode for unlimited simulation range
	// When enabled, uses 21-bit keys (TileHash 3-bit + LocalMorton 18-bit)
	// Default: true (recommended for character-attached fluids)
	bool bUseHybridTiledZOrder = true;

	// Simulation bounds for Morton code computation
	FVector3f SimulationBoundsMin = FVector3f(-1280.0f, -1280.0f, -1280.0f);
	FVector3f SimulationBoundsMax = FVector3f(1280.0f, 1280.0f, 1280.0f);

	//=========================================================================
	// Buffer Capacity Tracking
	//=========================================================================

	int32 ZOrderBufferParticleCapacity = 0;
};

//=============================================================================
// LEGACY COMPATIBILITY ALIAS
// The old name is kept for backward compatibility during transition.
// New code should use FGPUZOrderSortManager instead.
//=============================================================================
using FGPUSpatialHashManager = FGPUZOrderSortManager;
