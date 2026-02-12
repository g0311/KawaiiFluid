// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Core/KawaiiFluidAnisotropy.h"
#include "Simulation/Shaders/GPUFluidSimulatorShaders.h"  // For FGridResolutionDim, GridResolutionPermutation

// Forward declaration
struct FGPUFluidParticle;
struct FGPUParticleAttachment;
struct FGPUBoundaryParticle;
struct FGPUBoneDeltaAttachment;
struct FGPUCollisionSphere;
struct FGPUCollisionCapsule;
struct FGPUCollisionBox;

/**
 * @struct FAnisotropyComputeParams
 * @brief Parameters for GPU anisotropy calculation.
 * 
 * @param PositionsSRV Structure of Arrays positions.
 * @param PackedVelocitiesSRV Packed velocity data.
 * @param FlagsSRV Particle status flags.
 * @param AttachmentsSRV Particle attachment data for surface normals.
 * @param CellCountsSRV Legacy hash-based spatial lookup cell counts.
 * @param ParticleIndicesSRV Legacy hash-based spatial lookup indices.
 * @param CellStartSRV Morton-sorted spatial lookup cell starts.
 * @param CellEndSRV Morton-sorted spatial lookup cell ends.
 * @param OutAxis1UAV Output axis 1 (direction + scale).
 * @param OutAxis2UAV Output axis 2.
 * @param OutAxis3UAV Output axis 3.
 * @param OutRenderOffsetUAV Offset for surface smoothing.
 * @param ParticleRadius Radius used for simulation.
 * @param ParticleCount Number of particles to process.
 * @param ParticleCountBufferSRV GPU-accurate particle count buffer.
 * @param Mode Anisotropy calculation mode (Velocity, Density, Hybrid).
 * @param VelocityStretchFactor Sensitivity to velocity for stretching.
 * @param Strength Overall intensity of the anisotropy effect.
 * @param MinStretch Minimum allowed axis ratio.
 * @param MaxStretch Maximum allowed elongation ratio.
 * @param DensityWeight Balance in Hybrid mode.
 * @param SmoothingRadius Radius for neighborhood search.
 * @param CellSize Size of spatial hash cell.
 * @param bUseZOrderSorting Whether to use Morton-sorted sequential access.
 * @param MortonBoundsMin Origin for Morton code calculation.
 * @param GridResolutionPreset Resolution for spatial sorting.
 * @param AttachedFlattenScale Flattening factor for attached particles.
 * @param AttachedStretchScale Stretching factor for attached particles.
 * @param BoundaryParticlesSRV Legacy brute-force boundary particle list.
 * @param BoundaryParticleCount Number of boundary particles.
 * @param bUseBoundaryAnisotropy Enable boundary contribution to covariance.
 * @param SortedBoundaryParticlesSRV Z-Order sorted boundary list.
 * @param BoundaryCellStartSRV Cell starts for sorted boundaries.
 * @param BoundaryCellEndSRV Cell ends for sorted boundaries.
 * @param bUseBoundaryZOrder Enable O(K) boundary search.
 * @param BoundaryWeight Contribution factor for boundaries.
 * @param bUseHybridTiledZOrder Enable hybrid tiled sorting mode.
 * @param BoneDeltaAttachmentsSRV Attachments for NEAR_BOUNDARY particles.
 * @param bEnableSurfaceNormalAnisotropy Use normals for NEAR_BOUNDARY particles.
 * @param CollisionSpheresSRV World-space collision spheres.
 * @param CollisionCapsulesSRV World-space collision capsules.
 * @param CollisionBoxesSRV World-space collision boxes.
 * @param SphereCount Number of spheres.
 * @param CapsuleCount Number of capsules.
 * @param BoxCount Number of boxes.
 * @param ColliderSearchRadius Radius for collider normal lookup.
 * @param PrevAxis1SRV Previous frame axis 1 for smoothing.
 * @param PrevAxis2SRV Previous frame axis 2.
 * @param PrevAxis3SRV Previous frame axis 3.
 * @param bEnableTemporalSmoothing Enable orientation blending across frames.
 * @param TemporalSmoothFactor Factor for temporal blending.
 * @param bHasPreviousFrame Availability of temporal data.
 * @param bPreserveVolume Maintain unit volume using log-space processing.
 * @param NonPreservedRenderScale Ellipsoid size when volume preservation is off.
 */
struct KAWAIIFLUIDRUNTIME_API FAnisotropyComputeParams
{
	FRDGBufferSRVRef PositionsSRV = nullptr;
	FRDGBufferSRVRef PackedVelocitiesSRV = nullptr;
	FRDGBufferSRVRef FlagsSRV = nullptr;
	FRDGBufferSRVRef AttachmentsSRV = nullptr;

	FRDGBufferSRVRef CellCountsSRV = nullptr;
	FRDGBufferSRVRef ParticleIndicesSRV = nullptr;

	FRDGBufferSRVRef CellStartSRV = nullptr;
	FRDGBufferSRVRef CellEndSRV = nullptr;

	FRDGBufferUAVRef OutAxis1UAV = nullptr;
	FRDGBufferUAVRef OutAxis2UAV = nullptr;
	FRDGBufferUAVRef OutAxis3UAV = nullptr;

	FRDGBufferUAVRef OutRenderOffsetUAV = nullptr;
	float ParticleRadius = 0.0f;

	int32 ParticleCount = 0;
	FRDGBufferSRVRef ParticleCountBufferSRV = nullptr;
	EGPUAnisotropyMode Mode = EGPUAnisotropyMode::DensityBased;

	float VelocityStretchFactor = 0.01f;

	float Strength = 1.0f;
	float MinStretch = 0.2f;
	float MaxStretch = 2.5f;

	float DensityWeight = 0.5f;
	float SmoothingRadius = 10.0f;
	float CellSize = 10.0f;

	bool bUseZOrderSorting = false;
	FVector3f MortonBoundsMin = FVector3f::ZeroVector;

	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;

	float AttachedFlattenScale = 0.3f;
	float AttachedStretchScale = 1.5f;

	FRDGBufferSRVRef BoundaryParticlesSRV = nullptr;
	int32 BoundaryParticleCount = 0;
	bool bUseBoundaryAnisotropy = false;

	FRDGBufferSRVRef SortedBoundaryParticlesSRV = nullptr;
	FRDGBufferSRVRef BoundaryCellStartSRV = nullptr;
	FRDGBufferSRVRef BoundaryCellEndSRV = nullptr;
	bool bUseBoundaryZOrder = false;
	float BoundaryWeight = 1.0f;

	bool bUseHybridTiledZOrder = false;

	FRDGBufferSRVRef BoneDeltaAttachmentsSRV = nullptr;
	bool bEnableSurfaceNormalAnisotropy = true;

	FRDGBufferSRVRef CollisionSpheresSRV = nullptr;
	FRDGBufferSRVRef CollisionCapsulesSRV = nullptr;
	FRDGBufferSRVRef CollisionBoxesSRV = nullptr;
	int32 SphereCount = 0;
	int32 CapsuleCount = 0;
	int32 BoxCount = 0;
	float ColliderSearchRadius = 50.0f;

	FRDGBufferSRVRef PrevAxis1SRV = nullptr;
	FRDGBufferSRVRef PrevAxis2SRV = nullptr;
	FRDGBufferSRVRef PrevAxis3SRV = nullptr;
	bool bEnableTemporalSmoothing = true;
	float TemporalSmoothFactor = 0.8f;
	bool bHasPreviousFrame = false;

	bool bPreserveVolume = true;

	float NonPreservedRenderScale = 1.0f;
};

// Constants (must match FluidSpatialHash.ush and FluidAnisotropyCompute.usf)
#define ANISOTROPY_SPATIAL_HASH_SIZE 65536
#define ANISOTROPY_MAX_PARTICLES_PER_CELL 16

/**
 * @class FFluidAnisotropyCS
 * @brief Compute shader for particle anisotropy calculation.
 * 
 * Orientation and scale calculation based on NVIDIA FleX and Yu & Turk 2013.
 */
class FFluidAnisotropyCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFluidAnisotropyCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidAnisotropyCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InPositions)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, InPackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InFlags)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUParticleAttachment>, InAttachments)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleIndices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellStart)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellEnd)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector4f>, OutAnisotropyAxis1)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector4f>, OutAnisotropyAxis2)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector4f>, OutAnisotropyAxis3)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, OutRenderOffset)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(uint32, AnisotropyMode)
		SHADER_PARAMETER(float, VelocityStretchFactor)
		SHADER_PARAMETER(float, AnisotropyScale)
		SHADER_PARAMETER(float, AnisotropyMin)
		SHADER_PARAMETER(float, AnisotropyMax)
		SHADER_PARAMETER(float, DensityWeight)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(int32, bUseZOrderSorting)
		SHADER_PARAMETER(FVector3f, MortonBoundsMin)
		SHADER_PARAMETER(int32, bUseHybridTiledZOrder)
		SHADER_PARAMETER(float, AttachedFlattenScale)
		SHADER_PARAMETER(float, AttachedStretchScale)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER(int32, bUseBoundaryAnisotropy)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, SortedBoundaryParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellStart)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellEnd)
		SHADER_PARAMETER(int32, bUseBoundaryZOrder)
		SHADER_PARAMETER(float, BoundaryWeight)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, PrevAnisotropyAxis1)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, PrevAnisotropyAxis2)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, PrevAnisotropyAxis3)
		SHADER_PARAMETER(int32, bEnableTemporalSmoothing)
		SHADER_PARAMETER(float, TemporalSmoothFactor)
		SHADER_PARAMETER(int32, bHasPreviousFrame)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoneDeltaAttachment>, InBoneDeltaAttachments)
		SHADER_PARAMETER(int32, bEnableSurfaceNormalAnisotropy)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionSphere>, CollisionSpheres)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionCapsule>, CollisionCapsules)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionBox>, CollisionBoxes)
		SHADER_PARAMETER(int32, SphereCount)
		SHADER_PARAMETER(int32, CapsuleCount)
		SHADER_PARAMETER(int32, BoxCount)
		SHADER_PARAMETER(float, ColliderSearchRadius)
		SHADER_PARAMETER(int32, bPreserveVolume)
		SHADER_PARAMETER(float, NonPreservedRenderScale)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 64;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FFluidAnisotropyPassBuilder
 * @brief Utility for adding anisotropy compute passes to RDG.
 */
class KAWAIIFLUIDRUNTIME_API FFluidAnisotropyPassBuilder
{
public:
	static void AddAnisotropyPass(
		FRDGBuilder& GraphBuilder,
		const FAnisotropyComputeParams& Params);

	static void CreateAnisotropyBuffers(
		FRDGBuilder& GraphBuilder,
		int32 ParticleCount,
		FRDGBufferRef& OutAxis1,
		FRDGBufferRef& OutAxis2,
		FRDGBufferRef& OutAxis3);
};
