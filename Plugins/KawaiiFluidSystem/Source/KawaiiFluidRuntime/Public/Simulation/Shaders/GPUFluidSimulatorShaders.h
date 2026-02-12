// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "Simulation/Parameters/GPUBoundaryAttachment.h"  // For FGPUBoneDeltaAttachment (NEW bone-following system)
#include "Core/KawaiiFluidSimulationTypes.h"  // For EGridResolutionPreset

// Spatial hash constants (must match FluidSpatialHash.ush)
#define GPU_SPATIAL_HASH_SIZE 65536
#define GPU_MAX_PARTICLES_PER_CELL 16

// Neighbor caching constants
// 64 neighbors is sufficient for typical SPH simulations (avg ~30-40 neighbors)
#define GPU_MAX_NEIGHBORS_PER_PARTICLE 64

//=============================================================================
// Grid Resolution Permutation
// Enables different grid sizes (64³, 128³, 256³) via shader permutation
//=============================================================================

/**
 * Grid Resolution Permutation Dimension
 *
 * Permutation Values:
 *   0 = Small  (6 bits, 64³ cells,    262,144 max cells)
 *   1 = Medium (7 bits, 128³ cells, 2,097,152 max cells)
 *   2 = Large  (8 bits, 256³ cells, 16,777,216 max cells)
 */
class FGridResolutionDim : SHADER_PERMUTATION_RANGE_INT("GRID_RESOLUTION_PRESET", 0, 3);

/** Helper to get Morton grid constants from permutation ID */
namespace GridResolutionPermutation
{
	/** Get axis bits for a permutation index */
	inline int32 GetAxisBits(int32 PermutationId)
	{
		switch (PermutationId)
		{
		case 0: return 6;  // Small
		case 1: return 7;  // Medium
		case 2: return 8;  // Large
		default: return 7;
		}
	}

	/** Get grid resolution for a permutation index */
	inline int32 GetGridResolution(int32 PermutationId)
	{
		return 1 << GetAxisBits(PermutationId);
	}

	/** Get max cells for a permutation index */
	inline int32 GetMaxCells(int32 PermutationId)
	{
		const int32 Res = GetGridResolution(PermutationId);
		return Res * Res * Res;
	}

	/** Get radix sort passes needed for Morton code bits */
	inline int32 GetRadixSortPasses(int32 PermutationId)
	{
		const int32 MortonCodeBits = GetAxisBits(PermutationId) * 3;
		return (MortonCodeBits + 3) / 4;  // ceil(bits / 4)
	}

	/** Convert EGridResolutionPreset to permutation index */
	inline int32 FromPreset(EGridResolutionPreset Preset)
	{
		return static_cast<int32>(Preset);
	}
}

//=============================================================================
// Predict Positions Compute Shader
// Pass 1: Apply forces and predict positions
//=============================================================================

/**
 * @class FPredictPositionsCS
 * @brief Pass 1: Apply forces and predict positions.
 * 
 * @param Particles Read-write access to particle buffer.
 * @param ParticleCount Number of particles to process.
 * @param DeltaTime Simulation substep delta time.
 * @param Gravity World gravity vector.
 * @param ExternalForce Global external force.
 * @param CohesionStrength Akinci 2013 cohesion force strength.
 * @param SmoothingRadius SPH smoothing radius.
 * @param RestDensity Target rest density.
 * @param MaxCohesionForce Stability clamp for cohesion.
 * @param ViscosityCoefficient XSPH viscosity coefficient.
 * @param Poly6Coeff Precomputed Poly6 kernel coefficient.
 * @param ViscLaplacianCoeff Precomputed Laplacian viscosity coefficient.
 * @param PrevNeighborList Neighbor list from previous frame.
 * @param PrevNeighborCounts Neighbor counts from previous frame.
 * @param bUsePrevNeighborCache Whether to use previous frame cache for forces.
 * @param PrevParticleCount Particle count in previous frame cache.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FPredictPositionsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPredictPositionsCS);
	SHADER_USE_PARAMETER_STRUCT(FPredictPositionsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(FVector3f, Gravity)
		SHADER_PARAMETER(FVector3f, ExternalForce)
		SHADER_PARAMETER(float, CohesionStrength)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(float, RestDensity)
		SHADER_PARAMETER(float, MaxCohesionForce)
		SHADER_PARAMETER(float, ViscosityCoefficient)
		SHADER_PARAMETER(float, Poly6Coeff)
		SHADER_PARAMETER(float, ViscLaplacianCoeff)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PrevNeighborList)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PrevNeighborCounts)
		SHADER_PARAMETER(int32, bUsePrevNeighborCache)
		SHADER_PARAMETER(int32, PrevParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FComputeDensityCS
 * @brief [DEPRECATED] Pass 3: Calculate density and lambda using spatial hash.
 * 
 * @param Particles Read-write access to particle buffer.
 * @param CellCounts Legacy hash table cell counts.
 * @param ParticleIndices Legacy hash table particle indices.
 * @param ParticleCount Number of particles to process.
 * @param SmoothingRadius SPH smoothing radius.
 * @param RestDensity Target rest density.
 * @param Poly6Coeff Precomputed Poly6 kernel coefficient.
 * @param SpikyCoeff Precomputed Spiky kernel coefficient.
 * @param CellSize Spatial hash cell size.
 * @param Compliance XPBD compliance.
 * @param DeltaTimeSq Precomputed DeltaTime squared.
 */
class FComputeDensityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeDensityCS);
	SHADER_USE_PARAMETER_STRUCT(FComputeDensityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleIndices)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(float, RestDensity)
		SHADER_PARAMETER(float, Poly6Coeff)
		SHADER_PARAMETER(float, SpikyCoeff)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(float, Compliance)
		SHADER_PARAMETER(float, DeltaTimeSq)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FSolvePressureCS
 * @brief [DEPRECATED] Pass 4: Apply position corrections based on density constraints.
 * 
 * @param Particles Read-write access to particle buffer.
 * @param CellCounts Legacy hash table cell counts.
 * @param ParticleIndices Legacy hash table particle indices.
 * @param ParticleCount Number of particles to process.
 * @param SmoothingRadius SPH smoothing radius.
 * @param RestDensity Target rest density.
 * @param SpikyCoeff Precomputed Spiky kernel coefficient.
 * @param Poly6Coeff Precomputed Poly6 kernel coefficient.
 * @param CellSize Spatial hash cell size.
 * @param bEnableTensileInstability Enable tensile instability correction.
 * @param TensileK Scaled strength k for tensile stability.
 * @param TensileN Exponent n for tensile stability.
 * @param InvW_DeltaQ Precomputed 1/W(Δq, h).
 */
class FSolvePressureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSolvePressureCS);
	SHADER_USE_PARAMETER_STRUCT(FSolvePressureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleIndices)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(float, RestDensity)
		SHADER_PARAMETER(float, SpikyCoeff)
		SHADER_PARAMETER(float, Poly6Coeff)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(int32, bEnableTensileInstability)
		SHADER_PARAMETER(float, TensileK)
		SHADER_PARAMETER(int32, TensileN)
		SHADER_PARAMETER(float, InvW_DeltaQ)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// Combined Density + Pressure Compute Shader (OPTIMIZED)
// Pass 3+4 Combined: Single neighbor traversal for both density and pressure
//
// Optimizations:
// 1. Pass Integration: Reduces neighbor search from 2x to 1x per iteration
// 2. rsqrt: Uses fast inverse square root
// 3. Loop Unroll: 27-cell explicit unrolling
// 4. Neighbor Caching: First iteration builds neighbor list, subsequent iterations reuse
//=============================================================================

/**
 * @class FSolveDensityPressureCS
 * @brief Pass 3+4 Combined: Single neighbor traversal for both density and pressure (OPTIMIZED).
 * 
 * @param Positions SoA positions buffer.
 * @param PredictedPositions SoA predicted positions buffer.
 * @param PackedVelocities Half-precision packed SoA velocities.
 * @param PackedDensityLambda Half-precision packed SoA density and lambda.
 * @param UniformParticleMass Global uniform particle mass.
 * @param Flags Particle state flags buffer.
 * @param NeighborCountsBuffer Neighbor counts buffer for stats.
 * @param CellCounts Legacy hash table cell counts.
 * @param ParticleIndices Legacy hash table particle indices.
 * @param CellStart Z-Order sorted cell start indices.
 * @param CellEnd Z-Order sorted cell end indices.
 * @param bUseZOrderSorting Whether to use Z-Order neighbor search.
 * @param MortonBoundsMin Minimum bounds for Morton code calculation.
 * @param MortonBoundsExtent Bounds extent for Morton code calculation.
 * @param bUseHybridTiledZOrder Whether to use Hybrid Tiled Z-Order mode.
 * @param NeighborList Neighbor list cache buffer.
 * @param NeighborCounts Neighbor count cache buffer.
 * @param ParticleCount Number of particles to process.
 * @param SmoothingRadius SPH smoothing radius.
 * @param RestDensity Target rest density.
 * @param Poly6Coeff Precomputed Poly6 kernel coefficient.
 * @param SpikyCoeff Precomputed Spiky kernel coefficient.
 * @param CellSize Spatial hash cell size.
 * @param Compliance XPBD compliance.
 * @param DeltaTimeSq Precomputed DeltaTime squared.
 * @param bEnableTensileInstability Enable tensile instability correction.
 * @param TensileK Scaled strength k for tensile stability.
 * @param TensileN Exponent n for tensile stability.
 * @param InvW_DeltaQ Precomputed 1/W(Δq, h).
 * @param IterationIndex Current solver iteration.
 * @param BoundaryParticles World-space boundary particles buffer.
 * @param BoundaryParticleCount Number of boundary particles.
 * @param bUseBoundaryDensity Whether to include boundary density.
 * @param SortedBoundaryParticles Sorted world-space boundary particles.
 * @param BoundaryCellStart Sorted boundary cell start indices.
 * @param BoundaryCellEnd Sorted boundary cell end indices.
 * @param bUseBoundaryZOrder Whether to use sorted boundary search.
 * @param bEnableRelativeVelocityDamping Enable relative velocity pressure damping.
 * @param RelativeVelocityDampingStrength Factor for pressure reduction.
 * @param bEnableBoundaryVelocityTransfer Enable boundary velocity transfer.
 * @param BoundaryVelocityTransferStrength Factor for boundary following.
 * @param BoundaryDetachSpeedThreshold Speed where detachment begins.
 * @param BoundaryMaxDetachSpeed Speed for full detachment.
 * @param BoundaryAdhesionStrength Adhesion strength for velocity transfer.
 * @param SolverIterationCount Total number of iterations.
 * @param bEnablePositionBasedSurfaceTension Enable surface tension.
 * @param SurfaceTensionStrength Intensity of surface tension effect.
 * @param SurfaceTensionActivationDistance Activation distance in cm.
 * @param SurfaceTensionFalloffDistance Falloff distance in cm.
 * @param SurfaceTensionSurfaceThreshold Surface detection threshold.
 * @param SurfaceTensionVelocityDamping Velocity damping factor for ST.
 * @param SurfaceTensionTolerance Activation tolerance dead zone.
 * @param MaxSurfaceTensionCorrection Position correction limit per iteration.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FSolveDensityPressureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSolveDensityPressureCS);
	SHADER_USE_PARAMETER_STRUCT(FSolveDensityPressureCS, FGlobalShader);

	// Permutation domain for grid resolution (Z-Order neighbor search)
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, PackedDensityLambda)
		SHADER_PARAMETER(float, UniformParticleMass)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, NeighborCountsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleIndices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellStart)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellEnd)
		SHADER_PARAMETER(int32, bUseZOrderSorting)
		SHADER_PARAMETER(FVector3f, MortonBoundsMin)
		SHADER_PARAMETER(FVector3f, MortonBoundsExtent)
		SHADER_PARAMETER(int32, bUseHybridTiledZOrder)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, NeighborList)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, NeighborCounts)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(float, RestDensity)
		SHADER_PARAMETER(float, Poly6Coeff)
		SHADER_PARAMETER(float, SpikyCoeff)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(float, Compliance)
		SHADER_PARAMETER(float, DeltaTimeSq)
		SHADER_PARAMETER(int32, bEnableTensileInstability)
		SHADER_PARAMETER(float, TensileK)
		SHADER_PARAMETER(int32, TensileN)
		SHADER_PARAMETER(float, InvW_DeltaQ)
		SHADER_PARAMETER(int32, IterationIndex)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER(int32, bUseBoundaryDensity)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, SortedBoundaryParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellStart)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellEnd)
		SHADER_PARAMETER(int32, bUseBoundaryZOrder)
		SHADER_PARAMETER(int32, bEnableRelativeVelocityDamping)
		SHADER_PARAMETER(float, RelativeVelocityDampingStrength)
		SHADER_PARAMETER(int32, bEnableBoundaryVelocityTransfer)
		SHADER_PARAMETER(float, BoundaryVelocityTransferStrength)
		SHADER_PARAMETER(float, BoundaryDetachSpeedThreshold)
		SHADER_PARAMETER(float, BoundaryMaxDetachSpeed)
		SHADER_PARAMETER(float, BoundaryAdhesionStrength)
		SHADER_PARAMETER(int32, SolverIterationCount)
		SHADER_PARAMETER(int32, bEnablePositionBasedSurfaceTension)
		SHADER_PARAMETER(float, SurfaceTensionStrength)
		SHADER_PARAMETER(float, SurfaceTensionActivationDistance)
		SHADER_PARAMETER(float, SurfaceTensionFalloffDistance)
		SHADER_PARAMETER(int32, SurfaceTensionSurfaceThreshold)
		SHADER_PARAMETER(float, SurfaceTensionVelocityDamping)
		SHADER_PARAMETER(float, SurfaceTensionTolerance)
		SHADER_PARAMETER(float, MaxSurfaceTensionCorrection)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FApplyViscosityCS
 * @brief [DEPRECATED] Pass 5: Apply XSPH viscosity.
 * 
 * @param Particles Read-write access to particle buffer.
 * @param CellCounts Legacy hash table cell counts.
 * @param ParticleIndices Legacy hash table particle indices.
 * @param NeighborList Neighbor list cache buffer.
 * @param NeighborCounts Neighbor count cache buffer.
 * @param ParticleCount Number of particles to process.
 * @param SmoothingRadius SPH smoothing radius.
 * @param ViscosityCoefficient XSPH viscosity coefficient.
 * @param Poly6Coeff Precomputed Poly6 kernel coefficient.
 * @param ViscLaplacianCoeff Precomputed Laplacian viscosity coefficient.
 * @param DeltaTime Substep delta time.
 * @param CellSize Spatial hash cell size.
 * @param bUseNeighborCache Whether to use cached neighbor list.
 * @param BoundaryParticles World-space boundary particles buffer.
 * @param BoundaryParticleCount Number of boundary particles.
 * @param bUseBoundaryViscosity Whether to include boundary viscosity.
 * @param AdhesionForceStrength Akinci 2013 adhesion force strength.
 * @param AdhesionVelocityStrength Velocity transfer strength.
 * @param AdhesionRadius Boundary adhesion influence radius.
 * @param SortedBoundaryParticles Sorted world-space boundary particles.
 * @param BoundaryCellStart Sorted boundary cell start indices.
 * @param BoundaryCellEnd Sorted boundary cell end indices.
 * @param bUseBoundaryZOrder Whether to use sorted boundary search.
 * @param MortonBoundsMin Minimum bounds for Morton code calculation.
 * @param BoundaryVelocityTransferStrength Factor for boundary following.
 * @param BoundaryDetachSpeedThreshold Speed where detachment begins.
 * @param BoundaryMaxDetachSpeed Speed for full detachment.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FApplyViscosityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FApplyViscosityCS);
	SHADER_USE_PARAMETER_STRUCT(FApplyViscosityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleIndices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, NeighborList)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, NeighborCounts)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(float, ViscosityCoefficient)
		SHADER_PARAMETER(float, Poly6Coeff)
		SHADER_PARAMETER(float, ViscLaplacianCoeff)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(int32, bUseNeighborCache)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER(int32, bUseBoundaryViscosity)
		SHADER_PARAMETER(float, AdhesionForceStrength)
		SHADER_PARAMETER(float, AdhesionVelocityStrength)
		SHADER_PARAMETER(float, AdhesionRadius)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, SortedBoundaryParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellStart)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellEnd)
		SHADER_PARAMETER(int32, bUseBoundaryZOrder)
		SHADER_PARAMETER(FVector3f, MortonBoundsMin)
		SHADER_PARAMETER(float, BoundaryVelocityTransferStrength)
		SHADER_PARAMETER(float, BoundaryDetachSpeedThreshold)
		SHADER_PARAMETER(float, BoundaryMaxDetachSpeed)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FParticleSleepingCS
 * @brief NVIDIA Flex stabilization technique: sleep low-velocity particles.
 * 
 * @param Particles Read-write access to particle buffer.
 * @param SleepCounters Buffer for sleep persistence tracking.
 * @param NeighborList Neighbor list cache buffer.
 * @param NeighborCounts Neighbor count cache buffer.
 * @param ParticleCount Number of particles to process.
 * @param SleepVelocityThreshold Speed below which sleep timer increments.
 * @param SleepFrameThreshold Frames required to enter sleep state.
 * @param WakeVelocityThreshold Speed required to wake up from sleep.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FParticleSleepingCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FParticleSleepingCS);
	SHADER_USE_PARAMETER_STRUCT(FParticleSleepingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, SleepCounters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, NeighborList)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, NeighborCounts)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, SleepVelocityThreshold)
		SHADER_PARAMETER(int32, SleepFrameThreshold)
		SHADER_PARAMETER(float, WakeVelocityThreshold)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FBoundsCollisionCS
 * @brief Pass 6: Apply AABB/OBB bounds collision.
 * 
 * @param Positions SoA positions buffer.
 * @param PredictedPositions SoA predicted positions buffer.
 * @param PackedVelocities Half-precision packed SoA velocities.
 * @param Flags Particle state flags buffer.
 * @param ParticleCount Number of particles to process.
 * @param ParticleRadius Particle collision radius.
 * @param BoundsCenter OBB center world position.
 * @param BoundsExtent OBB local half extents.
 * @param BoundsRotation OBB rotation quaternion.
 * @param bUseOBB Whether to use OBB mode (1) or AABB mode (0).
 * @param BoundsMin World bounds minimum (AABB mode).
 * @param BoundsMax World bounds maximum (AABB mode).
 * @param Restitution Collision bounciness factor.
 * @param Friction Collision friction factor.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FBoundsCollisionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBoundsCollisionCS);
	SHADER_USE_PARAMETER_STRUCT(FBoundsCollisionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER(FVector3f, BoundsCenter)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(FVector4f, BoundsRotation)
		SHADER_PARAMETER(int32, bUseOBB)
		SHADER_PARAMETER(FVector3f, BoundsMin)
		SHADER_PARAMETER(FVector3f, BoundsMax)
		SHADER_PARAMETER(float, Restitution)
		SHADER_PARAMETER(float, Friction)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// Heightmap Collision Compute Shader
// Apply collision with Landscape terrain via heightmap texture sampling
// Uses Sobel gradient for terrain normal calculation
//=============================================================================

/**
 * @class FHeightmapCollisionCS
 * @brief Apply collision with Landscape terrain via heightmap texture sampling.
 * 
 * Uses Sobel gradient for terrain normal calculation.
 * 
 * @param Positions SoA positions buffer.
 * @param PredictedPositions SoA predicted positions buffer.
 * @param PackedVelocities Half-precision packed SoA velocities.
 * @param Flags Particle state flags buffer.
 * @param ParticleCount Number of particles to process.
 * @param ParticleRadius Particle collision radius.
 * @param HeightmapTexture Input heightmap texture.
 * @param HeightmapSampler Sampler state for heightmap.
 * @param WorldMin Minimum world position covered by heightmap.
 * @param WorldMax Maximum world position covered by heightmap.
 * @param InvWorldExtent 1/(Max - Min) for UV transform.
 * @param TextureWidth Heightmap texture width.
 * @param TextureHeight Heightmap texture height.
 * @param InvTextureWidth Inverse texture width.
 * @param InvTextureHeight Inverse texture height.
 * @param Friction Terrain friction coefficient.
 * @param Restitution Terrain restitution coefficient.
 * @param NormalStrength Normal calculation gradient scale.
 * @param CollisionOffset Extra offset for detection.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FHeightmapCollisionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHeightmapCollisionCS);
	SHADER_USE_PARAMETER_STRUCT(FHeightmapCollisionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, HeightmapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HeightmapSampler)
		SHADER_PARAMETER(FVector3f, WorldMin)
		SHADER_PARAMETER(FVector3f, WorldMax)
		SHADER_PARAMETER(FVector2f, InvWorldExtent)
		SHADER_PARAMETER(int32, TextureWidth)
		SHADER_PARAMETER(int32, TextureHeight)
		SHADER_PARAMETER(float, InvTextureWidth)
		SHADER_PARAMETER(float, InvTextureHeight)
		SHADER_PARAMETER(float, Friction)
		SHADER_PARAMETER(float, Restitution)
		SHADER_PARAMETER(float, NormalStrength)
		SHADER_PARAMETER(float, CollisionOffset)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FPrimitiveCollisionCS
 * @brief Pass 6.5: Apply collision with explicit primitives (spheres, capsules, boxes, convexes).
 * 
 * Also records collision feedback for particle -> player interaction (when enabled).
 * 
 * @param Positions SoA positions buffer.
 * @param PredictedPositions SoA predicted positions buffer.
 * @param PackedVelocities Half-precision packed SoA velocities.
 * @param PackedDensityLambda Half-precision packed SoA density and lambda.
 * @param SourceIDs Particle source component IDs buffer.
 * @param Flags Particle state flags buffer.
 * @param ParticleCount Number of particles to process.
 * @param ParticleRadius Particle collision radius.
 * @param CollisionThreshold Extra threshold for contact.
 * @param CollisionSpheres Buffer of sphere primitives.
 * @param SphereCount Number of sphere primitives.
 * @param CollisionCapsules Buffer of capsule primitives.
 * @param CapsuleCount Number of capsule primitives.
 * @param CollisionBoxes Buffer of box primitives.
 * @param BoxCount Number of box primitives.
 * @param CollisionConvexes Buffer of convex primitive headers.
 * @param ConvexCount Number of convex primitives.
 * @param ConvexPlanes Buffer of planes for convex hulls.
 * @param BoneTransforms Buffer of bone world transforms.
 * @param BoneCount Number of bones.
 * @param UnifiedFeedbackBuffer Atomic output buffer for collision data.
 * @param bEnableCollisionFeedback Whether to record collision events.
 * @param ColliderContactCounts Simple counters per collider.
 * @param MaxColliderCount Maximum supported colliders for counting.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FPrimitiveCollisionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPrimitiveCollisionCS);
	SHADER_USE_PARAMETER_STRUCT(FPrimitiveCollisionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PackedDensityLambda)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, SourceIDs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER(float, CollisionThreshold)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionSphere>, CollisionSpheres)
		SHADER_PARAMETER(int32, SphereCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionCapsule>, CollisionCapsules)
		SHADER_PARAMETER(int32, CapsuleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionBox>, CollisionBoxes)
		SHADER_PARAMETER(int32, BoxCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionConvex>, CollisionConvexes)
		SHADER_PARAMETER(int32, ConvexCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUConvexPlane>, ConvexPlanes)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoneTransform>, BoneTransforms)
		SHADER_PARAMETER(int32, BoneCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, UnifiedFeedbackBuffer)
		SHADER_PARAMETER(int32, bEnableCollisionFeedback)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ColliderContactCounts)
		SHADER_PARAMETER(int32, MaxColliderCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FFinalizePositionsCS
 * @brief Pass 7: Finalize positions and update velocities.
 * 
 * @param Positions SoA positions buffer.
 * @param PredictedPositions SoA predicted positions buffer.
 * @param PackedVelocities Half-precision packed SoA velocities.
 * @param Flags Particle state flags buffer.
 * @param ParticleCount Number of particles to process.
 * @param DeltaTime Simulation substep delta time.
 * @param MaxVelocity Safety velocity clamp.
 * @param GlobalDamping Velocity damping factor.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FFinalizePositionsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFinalizePositionsCS);
	SHADER_USE_PARAMETER_STRUCT(FFinalizePositionsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, MaxVelocity)
		SHADER_PARAMETER(float, GlobalDamping)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// Extract Render Data Compute Shader
// Phase 2: Extract render data from physics buffer to render buffer (GPU → GPU)
//=============================================================================

/**
 * @class FExtractRenderDataCS
 * @brief Phase 2: Extract render data from physics buffer to render buffer (GPU -> GPU).
 * 
 * @param PhysicsParticles Physics particle buffer (input).
 * @param RenderParticles Output render particle buffer (AoS).
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 * @param ParticleRadius Radius for rendering.
 */
class FExtractRenderDataCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FExtractRenderDataCS);
	SHADER_USE_PARAMETER_STRUCT(FExtractRenderDataCS, FGlobalShader);

	// Render particle structure (must match FKawaiiRenderParticle - 32 bytes)
	struct FRenderParticle
	{
		FVector3f Position;
		FVector3f Velocity;
		float Radius;
		float Padding;
	};

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, PhysicsParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FRenderParticle>, RenderParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(float, ParticleRadius)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FExtractRenderDataSoACS
 * @brief Phase 2: Extract to SoA buffers for memory bandwidth optimization.
 * 
 * @param PhysicsParticles Physics particle buffer (input).
 * @param RenderPositions Output render positions buffer.
 * @param RenderVelocities Output render velocities buffer.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 * @param ParticleRadius Radius for rendering.
 */
class FExtractRenderDataSoACS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FExtractRenderDataSoACS);
	SHADER_USE_PARAMETER_STRUCT(FExtractRenderDataSoACS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, PhysicsParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, RenderPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, RenderVelocities)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(float, ParticleRadius)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FExtractRenderDataWithBoundsCS
 * @brief Merged pass: ExtractRenderData + CalculateBounds (OPTIMIZED).
 * 
 * @param PhysicsParticles Physics particle buffer (input).
 * @param RenderParticles Output render particle buffer.
 * @param OutputBounds Output buffer for computed AABB.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 * @param ParticleRadius Radius for rendering.
 * @param BoundsMargin Extra margin to expand computed bounds.
 */
class FExtractRenderDataWithBoundsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FExtractRenderDataWithBoundsCS);
	SHADER_USE_PARAMETER_STRUCT(FExtractRenderDataWithBoundsCS, FGlobalShader);

	// Render particle structure (must match FKawaiiRenderParticle - 32 bytes)
	struct FRenderParticle
	{
		FVector3f Position;
		FVector3f Velocity;
		float Radius;
		float Padding;
	};

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, PhysicsParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FRenderParticle>, RenderParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, OutputBounds)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER(float, BoundsMargin)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FCopyParticlesCS
 * @brief Utility: Copy particles from source buffer to destination buffer.
 * 
 * @param SourceParticles Source particle buffer.
 * @param DestParticles Destination particle buffer.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 * @param SourceOffset Element offset in source.
 * @param DestOffset Element offset in destination.
 * @param CopyCount Number of elements to copy.
 * @param bReadCountFromGPU Whether to read count from ParticleCountBuffer.
 */
class FCopyParticlesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCopyParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FCopyParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, SourceParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, DestParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(int32, SourceOffset)
		SHADER_PARAMETER(int32, DestOffset)
		SHADER_PARAMETER(int32, CopyCount)
		SHADER_PARAMETER(int32, bReadCountFromGPU)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FSpawnParticlesCS
 * @brief GPU-based particle creation from spawn requests.
 * 
 * @param SpawnRequests Buffer of spawn requests from CPU.
 * @param Particles Main particle buffer to write into.
 * @param ParticleCounter Global atomic counter for total particles.
 * @param SourceCounters Per-source atomic counters.
 * @param SpawnRequestCount Number of requests to process.
 * @param MaxParticleCount Maximum capacity.
 * @param NextParticleID Base ID for new particles.
 * @param MaxSourceCount Maximum number of components.
 * @param DefaultRadius Default radius if unspecified.
 * @param DefaultMass Default mass if unspecified.
 */
class FSpawnParticlesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSpawnParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FSpawnParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUSpawnRequest>, SpawnRequests)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleCounter)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SourceCounters)
		SHADER_PARAMETER(int32, SpawnRequestCount)
		SHADER_PARAMETER(int32, MaxParticleCount)
		SHADER_PARAMETER(int32, NextParticleID)
		SHADER_PARAMETER(int32, MaxSourceCount)
		SHADER_PARAMETER(float, DefaultRadius)
		SHADER_PARAMETER(float, DefaultMass)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 64;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};


//=============================================================================
// GPU-Driven Despawn Compute Shaders
// Marks particles for removal by brush, source, or oldest criteria
//=============================================================================

// Initialize AliveMask: sets AliveMask[i] = 1 for valid particles (indirect dispatch)
class FInitAliveMaskCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInitAliveMaskCS);
	SHADER_USE_PARAMETER_STRUCT(FInitAliveMaskCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutAliveMask)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

// Brush-based despawn: marks particles within spherical brush radius
class FMarkDespawnByBrushCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMarkDespawnByBrushCS);
	SHADER_USE_PARAMETER_STRUCT(FMarkDespawnByBrushCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUDespawnBrushRequest>, BrushRequests)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutAliveMask)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(int32, BrushRequestCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

// Source-based despawn: marks particles matching specific SourceIDs
class FMarkDespawnBySourceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMarkDespawnBySourceCS);
	SHADER_USE_PARAMETER_STRUCT(FMarkDespawnBySourceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int>, DespawnSourceIDs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutAliveMask)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(int32, DespawnSourceIDCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

// Oldest despawn Pass 1: Build 256-bucket histogram of ParticleID upper bits
class FBuildIDHistogramCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBuildIDHistogramCS);
	SHADER_USE_PARAMETER_STRUCT(FBuildIDHistogramCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IDHistogram)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PerSourceExcess)
		SHADER_PARAMETER(int32, FilterSourceID)
		SHADER_PARAMETER(int32, IDShiftBits)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

// Oldest despawn Pass 2: Find threshold bucket via prefix sum (single thread)
class FFindOldestThresholdCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFindOldestThresholdCS);
	SHADER_USE_PARAMETER_STRUCT(FFindOldestThresholdCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Note: IDHistogram is RW in HLSL (shared file with BuildIDHistogramCS) but read-only here
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IDHistogram)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OldestThreshold)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PerSourceExcess)
		SHADER_PARAMETER(int32, FilterSourceID)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

// Per-source oldest despawn Pass 3: Mark particles below threshold + atomic boundary
class FMarkOldestParticlesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMarkOldestParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FMarkOldestParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		// Note: OldestThreshold is RW in HLSL (shared file with FindOldestThresholdCS) but read-only here
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OldestThreshold)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutAliveMask)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BoundaryCounter)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PerSourceExcess)
		SHADER_PARAMETER(int32, FilterSourceID)
		SHADER_PARAMETER(int32, IDShiftBits)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

// Per-source recycle: computes per-source excess for sources with emitter max limits
class FComputePerSourceRecycleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputePerSourceRecycleCS);
	SHADER_USE_PARAMETER_STRUCT(FComputePerSourceRecycleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SourceCounters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, EmitterMaxCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, IncomingSpawnCounts)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PerSourceExcess)
		SHADER_PARAMETER(int32, ActiveSourceCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

// Source counter update: decrements counters for particles marked dead
class FUpdateSourceCountersDespawnCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUpdateSourceCountersDespawnCS);
	SHADER_USE_PARAMETER_STRUCT(FUpdateSourceCountersDespawnCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, AliveMask)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SourceCounters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(int32, MaxSourceCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class FPrefixSumBlockCS_RDG : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPrefixSumBlockCS_RDG);
	SHADER_USE_PARAMETER_STRUCT(FPrefixSumBlockCS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MarkedFlags)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PrefixSums)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BlockSums)
		SHADER_PARAMETER(int32, ElementCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * Pass 2b: Scan Block Sums
 * Scans the block sums for multi-block prefix sum
 */
class FScanBlockSumsCS_RDG : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScanBlockSumsCS_RDG);
	SHADER_USE_PARAMETER_STRUCT(FScanBlockSumsCS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BlockSums)
		SHADER_PARAMETER(int32, BlockCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * Pass 2c: Add Block Offsets
 * Adds block offsets to get final prefix sums
 */
class FAddBlockOffsetsCS_RDG : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAddBlockOffsetsCS_RDG);
	SHADER_USE_PARAMETER_STRUCT(FAddBlockOffsetsCS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PrefixSums)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BlockSums)
		SHADER_PARAMETER(int32, ElementCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * Pass 3: Compact
 * Compacts marked particles into contiguous array
 */
class FCompactParticlesCS_RDG : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCompactParticlesCS_RDG);
	SHADER_USE_PARAMETER_STRUCT(FCompactParticlesCS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MarkedFlags)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PrefixSums)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, CompactedParticles)
		SHADER_PARAMETER(int32, ParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * Pass 4: Write Total Count
 * Writes the total number of compacted particles
 */
class FWriteTotalCountCS_RDG : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWriteTotalCountCS_RDG);
	SHADER_USE_PARAMETER_STRUCT(FWriteTotalCountCS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PrefixSums)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MarkedFlags)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutTotalCount)
		SHADER_PARAMETER(int32, ParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};
//=============================================================================
// Extract Positions Compute Shader
// Utility: Extract positions from particle buffer for spatial hash
//=============================================================================

class FExtractPositionsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FExtractPositionsCS);
	SHADER_USE_PARAMETER_STRUCT(FExtractPositionsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float3>, Positions)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(int32, bUsePredictedPosition)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// GPU Fluid Simulator Pass Builder
// Utility class for adding compute passes to RDG
//=============================================================================

class KAWAIIFLUIDRUNTIME_API FGPUFluidSimulatorPassBuilder
{
public:
	/** Add primitive collision pass (explicit primitives from FluidCollider) */
	static void AddPrimitiveCollisionPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferUAVRef ParticlesUAV,
		FRDGBufferSRVRef SpheresSRV,
		FRDGBufferSRVRef CapsulesSRV,
		FRDGBufferSRVRef BoxesSRV,
		FRDGBufferSRVRef ConvexesSRV,
		FRDGBufferSRVRef ConvexPlanesSRV,
		int32 SphereCount,
		int32 CapsuleCount,
		int32 BoxCount,
		int32 ConvexCount,
		int32 ParticleCount,
		float ParticleRadius,
		float CollisionThreshold);

	/** Add extract render data pass (Phase 2: GPU physics → GPU render) */
	static void AddExtractRenderDataPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef PhysicsParticlesSRV,
		FRDGBufferUAVRef RenderParticlesUAV,
		FRDGBufferSRVRef ParticleCountBufferSRV,
		int32 MaxParticleCount,
		float ParticleRadius);

	/** Add merged extract render data + bounds calculation pass (Optimized) */
	static void AddExtractRenderDataWithBoundsPass(
		FRDGBuilder& GraphBuilder,
		FRDGBufferSRVRef PhysicsParticlesSRV,
		FRDGBufferUAVRef RenderParticlesUAV,
		FRDGBufferUAVRef BoundsBufferUAV,
		FRDGBufferSRVRef ParticleCountBufferSRV,
		float ParticleRadius,
		float BoundsMargin);

	/** Add extract render data SoA pass (Memory bandwidth optimized) */
	static void AddExtractRenderDataSoAPass(
	   FRDGBuilder& GraphBuilder,
	   FRDGBufferSRVRef PhysicsParticlesSRV,
	   FRDGBufferUAVRef RenderPositionsUAV,
	   FRDGBufferUAVRef RenderVelocitiesUAV,
	   FRDGBufferSRVRef ParticleCountBufferSRV,
	   int32 MaxParticleCount,
	   float ParticleRadius);

};

//=============================================================================
// GPU Adhesion Compute Shaders
// Bone-based attachment tracking without CPU readback
//=============================================================================

/**
 * Adhesion Compute Shader
 * Checks particles near primitives and creates attachments
 */
class FAdhesionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAdhesionCS);
	SHADER_USE_PARAMETER_STRUCT(FAdhesionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Particle SOA buffers
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)  // B plan: half3 packed
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUParticleAttachment>, Attachments)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, ParticleRadius)

		// Bone transforms
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoneTransform>, BoneTransforms)
		SHADER_PARAMETER(int32, BoneCount)

		// Collision primitives
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionSphere>, CollisionSpheres)
		SHADER_PARAMETER(int32, SphereCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionCapsule>, CollisionCapsules)
		SHADER_PARAMETER(int32, CapsuleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionBox>, CollisionBoxes)
		SHADER_PARAMETER(int32, BoxCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionConvex>, CollisionConvexes)
		SHADER_PARAMETER(int32, ConvexCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUConvexPlane>, ConvexPlanes)

		// Adhesion parameters
		SHADER_PARAMETER(float, AdhesionStrength)
		SHADER_PARAMETER(float, AdhesionRadius)
		SHADER_PARAMETER(float, DetachAccelThreshold)
		SHADER_PARAMETER(float, DetachDistanceThreshold)
		SHADER_PARAMETER(float, ColliderContactOffset)
		SHADER_PARAMETER(float, BoneVelocityScale)
		SHADER_PARAMETER(float, SlidingFriction)
		SHADER_PARAMETER(float, CurrentTime)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(int32, bEnableAdhesion)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	}
};

/**
 * Update Attached Positions Compute Shader
 * Moves attached particles with bone transforms
 */
class FUpdateAttachedPositionsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUpdateAttachedPositionsCS);
	SHADER_USE_PARAMETER_STRUCT(FUpdateAttachedPositionsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Particle SOA buffers
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)  // B plan: half3 packed
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUParticleAttachment>, Attachments)
		SHADER_PARAMETER(int32, ParticleCount)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoneTransform>, BoneTransforms)
		SHADER_PARAMETER(int32, BoneCount)

		// Primitives for surface distance check
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionSphere>, CollisionSpheres)
		SHADER_PARAMETER(int32, SphereCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionCapsule>, CollisionCapsules)
		SHADER_PARAMETER(int32, CapsuleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionBox>, CollisionBoxes)
		SHADER_PARAMETER(int32, BoxCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionConvex>, CollisionConvexes)
		SHADER_PARAMETER(int32, ConvexCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUConvexPlane>, ConvexPlanes)

		SHADER_PARAMETER(float, DetachAccelThreshold)
		SHADER_PARAMETER(float, DetachDistanceThreshold)
		SHADER_PARAMETER(float, ColliderContactOffset)
		SHADER_PARAMETER(float, BoneVelocityScale)
		SHADER_PARAMETER(float, SlidingFriction)
		SHADER_PARAMETER(float, DeltaTime)

		// Gravity sliding parameters
		SHADER_PARAMETER(FVector3f, Gravity)
		SHADER_PARAMETER(float, GravitySlidingScale)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	}
};

/**
 * Clear Detached Flag Compute Shader
 * Clears the JUST_DETACHED flag at end of frame
 */
class FClearDetachedFlagCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearDetachedFlagCS);
	SHADER_USE_PARAMETER_STRUCT(FClearDetachedFlagCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	}
};

//=============================================================================
// Stack Pressure Compute Shader
// Applies weight transfer from stacked attached particles for realistic dripping
// Particles higher up transfer their weight to particles below
//=============================================================================

//=============================================================================
// Boundary Adhesion Compute Shaders (Flex-style Adhesion)
// Applies adhesion forces between fluid particles and boundary particles
// Based on Akinci 2012 "Versatile Rigid-Fluid Coupling"
// Optimized with Spatial Hash for O(N) complexity
//=============================================================================

// Clear boundary spatial hash
class FClearBoundaryHashCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearBoundaryHashCS);
	SHADER_USE_PARAMETER_STRUCT(FClearBoundaryHashCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWBoundaryCellCounts)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	}
};

// Build boundary spatial hash
class FBuildBoundaryHashCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBuildBoundaryHashCS);
	SHADER_USE_PARAMETER_STRUCT(FBuildBoundaryHashCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER(float, BoundaryCellSize)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWBoundaryCellCounts)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWBoundaryParticleIndices)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	}
};

// Boundary Attachment (Flex-style - check and attach)
class FBoundaryAttachCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBoundaryAttachCS);
	SHADER_USE_PARAMETER_STRUCT(FBoundaryAttachCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUBoundaryAttachment>, BoundaryAttachments)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryParticleIndices)
		SHADER_PARAMETER(float, BoundaryCellSize)
		SHADER_PARAMETER(float, AdhesionStrength)
		SHADER_PARAMETER(float, AdhesionRadius)
		SHADER_PARAMETER(float, CurrentTime)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{ return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{ FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment); OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize); }
};

// Update Boundary Attached (Flex-style - position constraint)
class FUpdateBoundaryAttachedCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUpdateBoundaryAttachedCS);
	SHADER_USE_PARAMETER_STRUCT(FUpdateBoundaryAttachedCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryAttachment>, BoundaryAttachments)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{ return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{ FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment); OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize); }
};

// Boundary Detach (Flex-style - detachment check)
class FBoundaryDetachCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBoundaryDetachCS);
	SHADER_USE_PARAMETER_STRUCT(FBoundaryDetachCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUBoundaryAttachment>, BoundaryAttachments)
		SHADER_PARAMETER(float, AdhesionRadius)
		SHADER_PARAMETER(float, DetachThreshold)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, CurrentTime)
		SHADER_PARAMETER(FVector3f, Gravity)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{ return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{ FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment); OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize); }
};

// Boundary adhesion (Force-based, Akinci 2013)
// Supports both Z-Order mode and Legacy Spatial Hash mode
class FBoundaryAdhesionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBoundaryAdhesionCS);
	SHADER_USE_PARAMETER_STRUCT(FBoundaryAdhesionCS, FGlobalShader);

	// Permutation domain for grid resolution (Z-Order neighbor search)
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// SOA particle buffers
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)  // B plan: half3 packed
		SHADER_PARAMETER(float, UniformParticleMass)  // B plan: uniform mass
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Flags)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
		// Legacy boundary particles (unsorted)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		// Legacy Spatial Hash (fallback mode)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryParticleIndices)
		SHADER_PARAMETER(float, BoundaryCellSize)
		// Z-Order sorted boundary particles (new mode)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, SortedBoundaryParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellStart)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellEnd)
		SHADER_PARAMETER(int32, bUseBoundaryZOrder)
		// Z-Order bounds (must match fluid simulation)
		SHADER_PARAMETER(FVector3f, MortonBoundsMin)
		SHADER_PARAMETER(float, CellSize)
		// Hybrid Tiled Z-Order mode (for unlimited simulation range)
		SHADER_PARAMETER(int32, bUseHybridTiledZOrder)
		// Adhesion parameters
		SHADER_PARAMETER(float, AdhesionForceStrength)
		SHADER_PARAMETER(float, AdhesionRadius)
		SHADER_PARAMETER(float, CohesionStrength)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(FVector3f, Gravity)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, RestDensity)
		SHADER_PARAMETER(float, Poly6Coeff)
		// Boundary Owner AABB for particle-level early-out
		// Skip adhesion calculation for particles far from boundary AABB
		SHADER_PARAMETER(FVector3f, BoundaryAABBMin)
		SHADER_PARAMETER(FVector3f, BoundaryAABBMax)
		SHADER_PARAMETER(int32, bUseBoundaryAABBCulling)
		// Attached particle counter for GPU readback (statistics/logging)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, AttachedParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{ return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), GPU_SPATIAL_HASH_SIZE);
		OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), GPU_MAX_PARTICLES_PER_CELL);

		// Get grid resolution from permutation for Z-Order neighbor search
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
		const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
		const int32 GridSize = GridResolutionPermutation::GetGridResolution(GridPreset);
		const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

		OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
		OutEnvironment.SetDefine(TEXT("MORTON_GRID_SIZE"), GridSize);
		OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
	}
};

//=============================================================================
// GPU Boundary Skinning Compute Shader
// Transforms bone-local boundary particles to world space using bone transforms
// Runs once per frame for each FluidInteractionComponent
//=============================================================================

class FBoundarySkinningCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBoundarySkinningCS);
	SHADER_USE_PARAMETER_STRUCT(FBoundarySkinningCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input: Local boundary particles (persistent, uploaded once)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticleLocal>, LocalBoundaryParticles)
		// Output: World-space boundary particles (updated each frame)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUBoundaryParticle>, WorldBoundaryParticles)
		// Previous frame positions for velocity calculation
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, PreviousWorldBoundaryParticles)
		// Bone transforms (uploaded each frame)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4x4>, BoneTransforms)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER(int32, BoneCount)
		SHADER_PARAMETER(int32, OwnerID)
		SHADER_PARAMETER(int32, bHasPreviousFrame)
		// Fallback transform for static meshes (BoneIndex == -1)
		SHADER_PARAMETER(FMatrix44f, ComponentTransform)
		// Delta time for velocity calculation
		SHADER_PARAMETER(float, DeltaTime)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	}
};

class FStackPressureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FStackPressureCS);
	SHADER_USE_PARAMETER_STRUCT(FStackPressureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// SOA particle buffers
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Positions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, PackedVelocities)  // B plan: half3 packed
		SHADER_PARAMETER(float, UniformParticleMass)  // B plan: uniform mass
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUParticleAttachment>, Attachments)

		// Spatial hash for neighbor search
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleIndices)

		// Collision primitives (for surface normal calculation)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionSphere>, CollisionSpheres)
		SHADER_PARAMETER(int32, SphereCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionCapsule>, CollisionCapsules)
		SHADER_PARAMETER(int32, CapsuleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionBox>, CollisionBoxes)
		SHADER_PARAMETER(int32, BoxCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionConvex>, CollisionConvexes)
		SHADER_PARAMETER(int32, ConvexCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUConvexPlane>, ConvexPlanes)

		// Parameters
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(float, SmoothingRadius)
		SHADER_PARAMETER(float, StackPressureScale)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(FVector3f, Gravity)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), GPU_SPATIAL_HASH_SIZE);
		OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), GPU_MAX_PARTICLES_PER_CELL);
	}
};

//=============================================================================
// Z-Order (Morton Code) Sorting Shaders
// GPU-based spatial sorting for cache-coherent neighbor access
// Pipeline: Morton Code → Radix Sort → Reorder → CellStart/End
//=============================================================================

//=============================================================================
// Z-Order Sorting Configuration
//
// Key Design: Only TWO parameters drive everything:
//   1. GridAxisBits (GPU_MORTON_GRID_AXIS_BITS) - bits per axis
//   2. SmoothingRadius (from Preset) - determines Cell Size
//
// All other values are auto-calculated:
//   - GridResolution = 2^GridAxisBits (e.g., 2^7 = 128)
//   - MortonCodeBits = GridAxisBits × 3 (e.g., 7 × 3 = 21 bits)
//   - MaxCells = GridResolution^3 (e.g., 128^3 = 2,097,152)
//   - CellSize = SmoothingRadius (1:1 ratio, optimal for SPH neighbor search)
//   - SimulationBounds = GridResolution × CellSize (e.g., 128 × 20 = 2560cm)
//
// Example with GridAxisBits=7, SmoothingRadius=20cm:
//   - GridResolution = 128
//   - MortonCodeBits = 21-bit
//   - MaxCells = 2,097,152
//   - CellSize = 20cm
//   - SimulationBounds = ±1280cm (25.6m total)
//=============================================================================

// Morton grid constants (must match FluidMortonCode.usf and FluidCellStartEnd.usf)
// 7 bits per axis → 21-bit Morton code → 2M cells max
#define GPU_MORTON_GRID_AXIS_BITS 7
#define GPU_MORTON_GRID_SIZE (1 << GPU_MORTON_GRID_AXIS_BITS)  // 128 (2^7)

//=============================================================================
// Cell Start/End Index Shaders
// Build grid lookup structure from sorted Morton codes
//=============================================================================

// MAX_CELLS = GridResolution^3 (must match FluidCellStartEnd.usf)
// 128^3 = 2,097,152 cells (21-bit Morton code = Cell ID)
#define GPU_MAX_CELLS (GPU_MORTON_GRID_SIZE * GPU_MORTON_GRID_SIZE * GPU_MORTON_GRID_SIZE)  // 2,097,152

/**
 * @class FComputeMortonCodesCS
 * @brief Converts 3D particle positions to 1D Morton codes for spatial sorting.
 * 
 * IMPORTANT: Uses PredictedPosition to match Solver's neighbor search.
 * Supports GridResolutionPreset permutation (Small/Medium/Large).
 * 
 * @param Particles Full particle structure (input).
 * @param MortonCodes Output Morton codes buffer.
 * @param ParticleIndices Output particle original indices.
 * @param ParticleCount Number of particles to process.
 * @param BoundsMin Minimum simulation bounds for normalization.
 * @param BoundsExtent Bounds size for normalization.
 * @param CellSize Spatial hash cell size.
 * @param bUseHybridTiledZOrder Whether to use unlimited range tiling.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FComputeMortonCodesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeMortonCodesCS);
	SHADER_USE_PARAMETER_STRUCT(FComputeMortonCodesCS, FGlobalShader);

	// Permutation domain for grid resolution
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MortonCodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleIndices)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER(FVector3f, BoundsMin)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(int32, bUseHybridTiledZOrder)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// GPU Radix Sort Shaders
// 8-bit radix (256 buckets), auto-calculated passes for Morton code coverage
// Algorithm: Histogram → Global Prefix Sum → Scatter (STABLE via LDS ranking)
// OPTIMIZED: 8-bit radix reduces passes from 6 to 3, halving dispatch overhead
//=============================================================================

#define GPU_RADIX_BITS 8
#define GPU_RADIX_SIZE 256  // 2^8 = 256 buckets
#define GPU_RADIX_THREAD_GROUP_SIZE 256
#define GPU_RADIX_ELEMENTS_PER_THREAD 4
#define GPU_RADIX_ELEMENTS_PER_GROUP (GPU_RADIX_THREAD_GROUP_SIZE * GPU_RADIX_ELEMENTS_PER_THREAD)  // 1024

// Auto-calculated: Morton code bits and required sort passes
// Morton code bits = GridAxisBits × 3 (X, Y, Z interleaved)
// Sort passes = ceil(MortonCodeBits / RadixBits)
#define GPU_MORTON_CODE_BITS (GPU_MORTON_GRID_AXIS_BITS * 3)  // 7 × 3 = 21 bits
#define GPU_RADIX_SORT_PASSES ((GPU_MORTON_CODE_BITS + GPU_RADIX_BITS - 1) / GPU_RADIX_BITS)  // ceil(21/8) = 3 passes

/**
 * @class FRadixSortHistogramCS
 * @brief Radix Sort Histogram: Pass 1: Count digit occurrences per block.
 * 
 * @param KeysIn Input Morton codes.
 * @param ValuesIn Input particle indices.
 * @param Histogram Output block-level histograms.
 * @param ElementCount Number of elements to sort.
 * @param BitOffset Current bit offset for radix digit.
 * @param NumGroups Total number of thread groups dispatched.
 */
class FRadixSortHistogramCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRadixSortHistogramCS);
	SHADER_USE_PARAMETER_STRUCT(FRadixSortHistogramCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, KeysIn)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ValuesIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, Histogram)
		SHADER_PARAMETER(int32, ElementCount)
		SHADER_PARAMETER(int32, BitOffset)
		SHADER_PARAMETER(int32, NumGroups)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FRadixSortGlobalPrefixSumCS
 * @brief Radix Sort Global Prefix Sum: Pass 2a: Compute sums across blocks for each bucket.
 * 
 * @param Histogram Input/Output histograms (modified in place).
 * @param GlobalOffsets Output global bucket offsets.
 * @param NumGroups Total number of thread groups.
 */
class FRadixSortGlobalPrefixSumCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRadixSortGlobalPrefixSumCS);
	SHADER_USE_PARAMETER_STRUCT(FRadixSortGlobalPrefixSumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, Histogram)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, GlobalOffsets)
		SHADER_PARAMETER(int32, NumGroups)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;  // One thread per bucket (256 buckets for 8-bit radix)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FRadixSortBucketPrefixSumCS
 * @brief Radix Sort Bucket Prefix Sum: Pass 2b: Compute prefix sum across buckets.
 * 
 * @param GlobalOffsets Global offsets buffer (modified in place).
 */
class FRadixSortBucketPrefixSumCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRadixSortBucketPrefixSumCS);
	SHADER_USE_PARAMETER_STRUCT(FRadixSortBucketPrefixSumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, GlobalOffsets)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FRadixSortScatterCS
 * @brief Radix Sort Scatter: Pass 3: Scatter elements to their sorted positions.
 * 
 * @param KeysIn Input Morton codes.
 * @param ValuesIn Input particle indices.
 * @param KeysOut Output sorted Morton codes.
 * @param ValuesOut Output sorted particle indices.
 * @param HistogramSRV Prefix-summed block histograms.
 * @param GlobalOffsetsSRV Global bucket offsets.
 * @param ElementCount Number of elements to scatter.
 * @param BitOffset Current bit offset for radix digit.
 */
class FRadixSortScatterCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRadixSortScatterCS);
	SHADER_USE_PARAMETER_STRUCT(FRadixSortScatterCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, KeysIn)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ValuesIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, KeysOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ValuesOut)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, HistogramSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, GlobalOffsetsSRV)
		SHADER_PARAMETER(int32, ElementCount)
		SHADER_PARAMETER(int32, BitOffset)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FRadixSortSmallCS
 * @brief Optimized single-pass sort for small arrays (<1024 elements).
 * 
 * @param KeysIn Input keys.
 * @param KeysOut Output sorted keys.
 * @param ValuesIn Input values.
 * @param ValuesOut Output sorted values.
 * @param ElementCount Number of elements to sort.
 */
class FRadixSortSmallCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRadixSortSmallCS);
	SHADER_USE_PARAMETER_STRUCT(FRadixSortSmallCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, KeysIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, KeysOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ValuesIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ValuesOut)
		SHADER_PARAMETER(int32, ElementCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// Particle Reordering Shaders
// Physically reorder particle data based on sorted indices
//=============================================================================

/**
 * @class FReorderParticlesCS
 * @brief Physically reorders particle data for cache-coherent access.
 * 
 * @param OldParticles Original particle buffer.
 * @param SortedIndices Map of [NewIndex] -> OldIndex.
 * @param SortedParticles Output sorted particle buffer.
 * @param OldBoneDeltaAttachments Original attachment buffer.
 * @param SortedBoneDeltaAttachments Output sorted attachment buffer.
 * @param bReorderAttachments Whether to reorder attachment buffer.
 * @param ParticleCount Number of particles to process.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FReorderParticlesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FReorderParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FReorderParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, OldParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SortedIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, SortedParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoneDeltaAttachment>, OldBoneDeltaAttachments)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUBoneDeltaAttachment>, SortedBoneDeltaAttachments)
		SHADER_PARAMETER(int32, bReorderAttachments)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FBuildReverseMappingCS
 * @brief Builds mapping from old indices to new sorted positions.
 * 
 * @param SortedIndices Map of [NewIndex] -> OldIndex.
 * @param OldToNewMapping Output map of [OldIndex] -> NewIndex.
 * @param ParticleCount Number of particles to process.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FBuildReverseMappingCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBuildReverseMappingCS);
	SHADER_USE_PARAMETER_STRUCT(FBuildReverseMappingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SortedIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OldToNewMapping)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FClearCellIndicesCS
 * @brief Initializes CellStart/End arrays to invalid values.
 * 
 * @param CellStart Read-write cell start indices.
 * @param CellEnd Read-write cell end indices.
 */
class FClearCellIndicesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearCellIndicesCS);
	SHADER_USE_PARAMETER_STRUCT(FClearCellIndicesCS, FGlobalShader);

	// Permutation domain for grid resolution
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellStart)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellEnd)
	END_SHADER_PARAMETER_STRUCT()

	// Increased to 512 to avoid exceeding GPU dispatch limit (65535) with Large preset
	// Large preset: 256³ = 16,777,216 cells / 512 = 32,768 groups (under limit)
	static constexpr int32 ThreadGroupSize = 512;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FComputeCellStartEndCS
 * @brief Finds where each cell's particles begin and end in sorted array.
 * 
 * @param SortedMortonCodes Input sorted Morton codes.
 * @param CellStart Output cell start indices.
 * @param CellEnd Output cell end indices.
 * @param ParticleCount Number of particles to process.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FComputeCellStartEndCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeCellStartEndCS);
	SHADER_USE_PARAMETER_STRUCT(FComputeCellStartEndCS, FGlobalShader);

	// Permutation domain for grid resolution
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SortedMortonCodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellStart)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellEnd)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	// Increased to 512 to match FluidCellStartEnd.usf
	static constexpr int32 ThreadGroupSize = 512;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// Boundary Particle Z-Order Sorting Shaders
// Enables O(K) neighbor search for boundary particles instead of O(M) traversal
// Pipeline: Morton Code → Radix Sort → Reorder → BoundaryCellStart/End
//=============================================================================

/**
 * @class FComputeBoundaryMortonCodesCS
 * @brief Converts boundary particle positions to Morton codes for Z-Order sorting.
 * 
 * @param BoundaryParticlesIn Input boundary particles buffer.
 * @param BoundaryMortonCodes Output Morton codes buffer.
 * @param BoundaryParticleIndices Output original indices mapping.
 * @param BoundaryParticleCount Number of boundary particles to process.
 * @param BoundsMin Minimum bounds for Morton normalization.
 * @param CellSize Cell size for Morton normalization.
 * @param bUseHybridTiledZOrder Whether to use unlimited range tiling.
 */
class FComputeBoundaryMortonCodesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeBoundaryMortonCodesCS);
	SHADER_USE_PARAMETER_STRUCT(FComputeBoundaryMortonCodesCS, FGlobalShader);

	// Permutation domain for grid resolution
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, BoundaryParticlesIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BoundaryMortonCodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BoundaryParticleIndices)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER(FVector3f, BoundsMin)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(int32, bUseHybridTiledZOrder)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FClearBoundaryCellIndicesCS
 * @brief Initializes BoundaryCellStart/End arrays to INVALID_INDEX.
 * 
 * @param BoundaryCellStart Read-write boundary cell start indices.
 * @param BoundaryCellEnd Read-write boundary cell end indices.
 */
class FClearBoundaryCellIndicesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearBoundaryCellIndicesCS);
	SHADER_USE_PARAMETER_STRUCT(FClearBoundaryCellIndicesCS, FGlobalShader);

	// Permutation domain for grid resolution
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BoundaryCellStart)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BoundaryCellEnd)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 512;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FReorderBoundaryParticlesCS
 * @brief Physically reorders boundary particles based on sorted Morton codes.
 * 
 * @param OldBoundaryParticles Original boundary particles buffer.
 * @param SortedBoundaryIndices Map of [NewIndex] -> OldIndex.
 * @param SortedBoundaryParticles Output sorted boundary particles.
 * @param BoundaryParticleCount Number of boundary particles to process.
 */
class FReorderBoundaryParticlesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FReorderBoundaryParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FReorderBoundaryParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, OldBoundaryParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SortedBoundaryIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUBoundaryParticle>, SortedBoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FComputeBoundaryCellStartEndCS
 * @brief Determines the range [start, end] of boundary particles in each cell.
 * 
 * @param SortedBoundaryMortonCodes Input sorted boundary Morton codes.
 * @param BoundaryCellStart Output boundary cell start indices.
 * @param BoundaryCellEnd Output boundary cell end indices.
 * @param BoundaryParticleCount Number of boundary particles.
 */
class FComputeBoundaryCellStartEndCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeBoundaryCellStartEndCS);
	SHADER_USE_PARAMETER_STRUCT(FComputeBoundaryCellStartEndCS, FGlobalShader);

	// Permutation domain for grid resolution
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SortedBoundaryMortonCodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BoundaryCellStart)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BoundaryCellEnd)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 512;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// Bone Delta Attachment Compute Shaders (NEW simplified bone-following)
// Part of the new attachment system: ApplyBoneTransform + UpdateBoneDeltaAttachment
//=============================================================================

/**
 * @class FApplyBoneTransformCS
 * @brief Apply Bone Transform: moves attached particles to follow boundary bones.
 * 
 * @param Particles Main particle buffer (read/write).
 * @param ParticleCount Number of particles to process.
 * @param BoneDeltaAttachments Read-only access to attachment data.
 * @param LocalBoundaryParticles Local boundary particles for PERFECT sync.
 * @param BoundaryParticleCount Number of boundary particles.
 * @param BoneTransforms Current bone matrices.
 * @param BoneCount Number of matrices in bone buffer.
 * @param ComponentTransform Fallback component world matrix.
 * @param DeltaTime Substep delta time.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FApplyBoneTransformCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FApplyBoneTransformCS);
	SHADER_USE_PARAMETER_STRUCT(FApplyBoneTransformCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoneDeltaAttachment>, BoneDeltaAttachments)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticleLocal>, LocalBoundaryParticles)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FMatrix44f>, BoneTransforms)
		SHADER_PARAMETER(int32, BoneCount)
		SHADER_PARAMETER(FMatrix44f, ComponentTransform)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FUpdateBoneDeltaAttachmentCS
 * @brief Update Bone Delta Attachment: finds nearest boundary after simulation.
 * 
 * @param Particles Main particle buffer (read/write).
 * @param ParticleCount Number of particles to process.
 * @param BoneDeltaAttachments Output read-write attachment states.
 * @param SortedBoundaryParticles Sorted boundary particles for search.
 * @param BoundaryCellStart Boundary hash cell starts.
 * @param BoundaryCellEnd Boundary hash cell ends.
 * @param BoundaryParticleCount Total boundary particles.
 * @param WorldBoundaryParticles Unsorted boundary particles for local offset.
 * @param WorldBoundaryParticleCount Total unsorted boundary particles.
 * @param AttachRadius Bond threshold distance.
 * @param DetachDistance Break threshold distance.
 * @param AdhesionStrength Global adhesion factor.
 * @param MortonBoundsMin Morton normalization origin.
 * @param CellSize Morton normalization scale.
 * @param bUseHybridTiledZOrder Whether to use tiling.
 * @param CollisionSpheres Collision primitives for normals.
 * @param CollisionCapsules Collision primitives for normals.
 * @param CollisionBoxes Collision primitives for normals.
 * @param BoneTransforms Current bone matrices.
 * @param SphereCount Number of sphere primitives.
 * @param CapsuleCount Number of capsule primitives.
 * @param BoxCount Number of box primitives.
 * @param BoneCount Number of bone matrices.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FUpdateBoneDeltaAttachmentCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUpdateBoneDeltaAttachmentCS);
	SHADER_USE_PARAMETER_STRUCT(FUpdateBoneDeltaAttachmentCS, FGlobalShader);

	// Permutation domain for grid resolution (for Z-Order boundary search)
	using FPermutationDomain = TShaderPermutationDomain<FGridResolutionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, Particles)
		SHADER_PARAMETER(int32, ParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUBoneDeltaAttachment>, BoneDeltaAttachments)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, SortedBoundaryParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellStart)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BoundaryCellEnd)
		SHADER_PARAMETER(int32, BoundaryParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoundaryParticle>, WorldBoundaryParticles)
		SHADER_PARAMETER(int32, WorldBoundaryParticleCount)
		SHADER_PARAMETER(float, AttachRadius)
		SHADER_PARAMETER(float, DetachDistance)
		SHADER_PARAMETER(float, AdhesionStrength)
		SHADER_PARAMETER(FVector3f, MortonBoundsMin)
		SHADER_PARAMETER(float, CellSize)
		SHADER_PARAMETER(int32, bUseHybridTiledZOrder)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionSphere>, CollisionSpheres)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionCapsule>, CollisionCapsules)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUCollisionBox>, CollisionBoxes)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUBoneTransform>, BoneTransforms)
		SHADER_PARAMETER(int32, SphereCount)
		SHADER_PARAMETER(int32, CapsuleCount)
		SHADER_PARAMETER(int32, BoxCount)
		SHADER_PARAMETER(int32, BoneCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// SoA (Structure of Arrays) Conversion Shaders
//=============================================================================

/**
 * @class FSplitAoSToSoACS
 * @brief Converts particle data from AoS to SoA layout.
 * 
 * @param SourceParticles Original AoS particle buffer.
 * @param OutPositions Output SoA positions buffer.
 * @param OutPredictedPositions Output SoA predicted positions buffer.
 * @param OutPackedVelocities Output SoA packed velocities.
 * @param OutPackedDensityLambda Output SoA packed density/lambda.
 * @param OutFlags Output SoA flags buffer.
 * @param OutNeighborCounts Output SoA neighbor counts buffer.
 * @param OutParticleIDs Output SoA persistent IDs buffer.
 * @param OutSourceIDs Output SoA source component IDs buffer.
 * @param SplitParticleCount Number of particles to convert.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FSplitAoSToSoACS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSplitAoSToSoACS);
	SHADER_USE_PARAMETER_STRUCT(FSplitAoSToSoACS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, SourceParticles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutPredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, OutPackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutPackedDensityLambda)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutFlags)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutNeighborCounts)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, OutParticleIDs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, OutSourceIDs)
		SHADER_PARAMETER(int32, SplitParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr uint32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * @class FMergeSoAToAoSCS
 * @brief Converts particle data from SoA back to AoS layout.
 * 
 * @param InPositions Input SoA positions buffer.
 * @param InPredictedPositions Input SoA predicted positions buffer.
 * @param InPackedVelocities Input SoA packed velocities.
 * @param InPackedDensityLambda Input SoA packed density/lambda.
 * @param InFlags Input SoA flags buffer.
 * @param InNeighborCounts Input SoA neighbor counts buffer.
 * @param InParticleIDs Input SoA persistent IDs buffer.
 * @param InSourceIDs Input SoA source component IDs buffer.
 * @param MergeUniformParticleMass Global mass factor.
 * @param TargetParticles Output AoS particle buffer.
 * @param MergeParticleCount Number of particles to merge.
 * @param ParticleCountBuffer GPU-accurate particle count buffer.
 */
class FMergeSoAToAoSCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMergeSoAToAoSCS);
	SHADER_USE_PARAMETER_STRUCT(FMergeSoAToAoSCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InPositions)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InPredictedPositions)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, InPackedVelocities)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InPackedDensityLambda)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InFlags)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InNeighborCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, InParticleIDs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, InSourceIDs)
		SHADER_PARAMETER(float, MergeUniformParticleMass)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUFluidParticle>, TargetParticles)
		SHADER_PARAMETER(int32, MergeParticleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr uint32 ThreadGroupSize = 256;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

//=============================================================================
// Indirect Dispatch Particle Count Shaders
// GPU-driven particle count management for DispatchIndirect
//=============================================================================

/**
 * @class FWriteAliveCountAfterCompactionCS
 * @brief Writes the GPU-accurate particle count after stream compaction.
 * 
 * @param PrefixSums Prefix sums of alive flags.
 * @param AliveMask Survival mask buffer.
 * @param ParticleCountBuffer Output atomic count buffer.
 * @param OldParticleCount Input pre-compaction count.
 */
class FWriteAliveCountAfterCompactionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWriteAliveCountAfterCompactionCS);
	SHADER_USE_PARAMETER_STRUCT(FWriteAliveCountAfterCompactionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PrefixSums)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, AliveMask)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleCountBuffer)
		SHADER_PARAMETER(int32, OldParticleCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

/**
 * @class FUpdateCountAfterSpawnCS
 * @brief Updates global particle count from atomic spawn counter.
 * 
 * @param SpawnCounter Atomic counter from spawn pass.
 * @param SpawnParticleCountBuffer Output global count buffer.
 */
class FUpdateCountAfterSpawnCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUpdateCountAfterSpawnCS);
	SHADER_USE_PARAMETER_STRUCT(FUpdateCountAfterSpawnCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SpawnCounter)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SpawnParticleCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

/**
 * @class FCopyCountToSpawnCounterCS
 * @brief Utility: Copies global particle count to an atomic counter for appending.
 * 
 * @param SourceParticleCountBuffer Global count buffer (input).
 * @param DestSpawnCounter Atomic spawn counter (output).
 */
class FCopyCountToSpawnCounterCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCopyCountToSpawnCounterCS);
	SHADER_USE_PARAMETER_STRUCT(FCopyCountToSpawnCounterCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SourceParticleCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, DestSpawnCounter)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};
