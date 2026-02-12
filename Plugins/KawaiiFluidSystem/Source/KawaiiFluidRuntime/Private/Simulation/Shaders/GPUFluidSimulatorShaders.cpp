// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Simulation/Shaders/GPUFluidSimulatorShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"

//=============================================================================
// Shader Implementations
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FPredictPositionsCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidPredictPositions.usf",
	"PredictPositionsCS", SF_Compute);

/**
 * @brief Check if predict positions shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FPredictPositionsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify predict positions shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FPredictPositionsCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("MAX_NEIGHBORS_PER_PARTICLE"), GPU_MAX_NEIGHBORS_PER_PARTICLE);
}

// [DEPRECATED] Use FSolveDensityPressureCS instead
IMPLEMENT_GLOBAL_SHADER(FComputeDensityCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidComputeDensity.usf",
	"ComputeDensityCS", SF_Compute);

/**
 * @brief Check if compute density shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FComputeDensityCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify compute density shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FComputeDensityCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), GPU_SPATIAL_HASH_SIZE);
	OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), GPU_MAX_PARTICLES_PER_CELL);
}

// [DEPRECATED] Use FSolveDensityPressureCS instead
IMPLEMENT_GLOBAL_SHADER(FSolvePressureCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidSolvePressure.usf",
	"SolvePressureCS", SF_Compute);

/**
 * @brief Check if solve pressure shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FSolvePressureCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify solve pressure shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FSolvePressureCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), GPU_SPATIAL_HASH_SIZE);
	OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), GPU_MAX_PARTICLES_PER_CELL);
}

IMPLEMENT_GLOBAL_SHADER(FSolveDensityPressureCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidSolveDensityPressure.usf",
	"SolveDensityPressureCS", SF_Compute);

/**
 * @brief Check if solve density pressure shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FSolveDensityPressureCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify solve density pressure shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FSolveDensityPressureCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), GPU_SPATIAL_HASH_SIZE);
	OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), GPU_MAX_PARTICLES_PER_CELL);
	OutEnvironment.SetDefine(TEXT("MAX_NEIGHBORS_PER_PARTICLE"), GPU_MAX_NEIGHBORS_PER_PARTICLE);

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

IMPLEMENT_GLOBAL_SHADER(FApplyViscosityCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidApplyViscosity.usf",
	"ApplyViscosityCS", SF_Compute);

/**
 * @brief Check if apply viscosity shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FApplyViscosityCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify apply viscosity shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FApplyViscosityCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), GPU_SPATIAL_HASH_SIZE);
	OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), GPU_MAX_PARTICLES_PER_CELL);
	OutEnvironment.SetDefine(TEXT("MAX_NEIGHBORS_PER_PARTICLE"), GPU_MAX_NEIGHBORS_PER_PARTICLE);
}

IMPLEMENT_GLOBAL_SHADER(FParticleSleepingCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidParticleSleeping.usf",
	"UpdateParticleSleepingCS", SF_Compute);

/**
 * @brief Check if particle sleeping shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FParticleSleepingCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify particle sleeping shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FParticleSleepingCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("MAX_NEIGHBORS_PER_PARTICLE"), GPU_MAX_NEIGHBORS_PER_PARTICLE);
}

IMPLEMENT_GLOBAL_SHADER(FBoundsCollisionCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundsCollision.usf",
	"BoundsCollisionCS", SF_Compute);

/**
 * @brief Check if bounds collision shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FBoundsCollisionCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify bounds collision shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FBoundsCollisionCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FHeightmapCollisionCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidHeightmapCollision.usf",
	"HeightmapCollisionCS", SF_Compute);

/**
 * @brief Check if heightmap collision shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FHeightmapCollisionCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify heightmap collision shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FHeightmapCollisionCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FPrimitiveCollisionCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidPrimitiveCollision.usf",
	"PrimitiveCollisionCS", SF_Compute);

/**
 * @brief Check if primitive collision shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FPrimitiveCollisionCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify primitive collision shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FPrimitiveCollisionCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FFinalizePositionsCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidFinalizePositions.usf",
	"FinalizePositionsCS", SF_Compute);

/**
 * @brief Check if finalize positions shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FFinalizePositionsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify finalize positions shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FFinalizePositionsCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FExtractPositionsCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidExtractPositions.usf",
	"ExtractPositionsCS", SF_Compute);

/**
 * @brief Check if extract positions shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FExtractPositionsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify extract positions shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FExtractPositionsCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FExtractRenderDataCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidExtractRenderData.usf",
	"ExtractRenderDataCS", SF_Compute);

/**
 * @brief Check if extract render data shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FExtractRenderDataCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify extract render data shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FExtractRenderDataCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FExtractRenderDataWithBoundsCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidExtractRenderDataWithBounds.usf",
	"ExtractRenderDataWithBoundsCS", SF_Compute);

/**
 * @brief Check if extract render data with bounds shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FExtractRenderDataWithBoundsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify extract render data with bounds shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FExtractRenderDataWithBoundsCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FExtractRenderDataSoACS,
	"/Plugin/KawaiiFluidSystem/Private/FluidExtractRenderData.usf",
	"ExtractRenderDataSoACS", SF_Compute);

/**
 * @brief Check if extract render data SoA shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FExtractRenderDataSoACS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify extract render data SoA shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FExtractRenderDataSoACS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FCopyParticlesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidCopyParticles.usf",
	"CopyParticlesCS", SF_Compute);

/**
 * @brief Check if copy particles shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FCopyParticlesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify copy particles shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FCopyParticlesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FSpawnParticlesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidSpawnParticles.usf",
	"SpawnParticlesCS", SF_Compute);

/**
 * @brief Check if spawn particles shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FSpawnParticlesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify spawn particles shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FSpawnParticlesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FInitAliveMaskCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidInitAliveMask.usf",
	"InitAliveMaskCS", SF_Compute);

/**
 * @brief Check if initialize alive mask shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FInitAliveMaskCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify initialize alive mask shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FInitAliveMaskCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FMarkDespawnByBrushCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidDespawnByBrush.usf",
	"MarkDespawnByBrushCS", SF_Compute);

/**
 * @brief Check if mark despawn by brush shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FMarkDespawnByBrushCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify mark despawn by brush shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FMarkDespawnByBrushCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FMarkDespawnBySourceCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidDespawnBySource.usf",
	"MarkDespawnBySourceCS", SF_Compute);

/**
 * @brief Check if mark despawn by source shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FMarkDespawnBySourceCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify mark despawn by source shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FMarkDespawnBySourceCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FBuildIDHistogramCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidDespawnOldest.usf",
	"BuildIDHistogramCS", SF_Compute);

/**
 * @brief Check if build ID histogram shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FBuildIDHistogramCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify build ID histogram shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FBuildIDHistogramCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FFindOldestThresholdCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidDespawnOldest.usf",
	"FindOldestThresholdCS", SF_Compute);

/**
 * @brief Check if find oldest threshold shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FFindOldestThresholdCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

IMPLEMENT_GLOBAL_SHADER(FMarkOldestParticlesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidDespawnOldest.usf",
	"MarkOldestParticlesCS", SF_Compute);

/**
 * @brief Check if mark oldest particles shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FMarkOldestParticlesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify mark oldest particles shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FMarkOldestParticlesCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FComputePerSourceRecycleCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidComputePerSourceRecycle.usf",
	"ComputePerSourceRecycleCS", SF_Compute);

/**
 * @brief Check if compute per-source recycle shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FComputePerSourceRecycleCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

IMPLEMENT_GLOBAL_SHADER(FUpdateSourceCountersDespawnCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidUpdateSourceCountersDespawn.usf",
	"UpdateSourceCountersDespawnCS", SF_Compute);

/**
 * @brief Check if update source counters despawn shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FUpdateSourceCountersDespawnCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify update source counters despawn shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FUpdateSourceCountersDespawnCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FPrefixSumBlockCS_RDG,
	"/Plugin/KawaiiFluidSystem/Private/FluidPrefixSum.usf",
	"PrefixSumBlockCS", SF_Compute);

/**
 * @brief Check if prefix sum block shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FPrefixSumBlockCS_RDG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify prefix sum block shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FPrefixSumBlockCS_RDG::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FScanBlockSumsCS_RDG,
	"/Plugin/KawaiiFluidSystem/Private/FluidPrefixSum.usf",
	"ScanBlockSumsCS", SF_Compute);

/**
 * @brief Check if scan block sums shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FScanBlockSumsCS_RDG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify scan block sums shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FScanBlockSumsCS_RDG::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FAddBlockOffsetsCS_RDG,
	"/Plugin/KawaiiFluidSystem/Private/FluidPrefixSum.usf",
	"AddBlockOffsetsCS", SF_Compute);

/**
 * @brief Check if add block offsets shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FAddBlockOffsetsCS_RDG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify add block offsets shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FAddBlockOffsetsCS_RDG::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FCompactParticlesCS_RDG,
	"/Plugin/KawaiiFluidSystem/Private/FluidDespawnParticles.usf",
	"CompactParticlesCS", SF_Compute);

/**
 * @brief Check if compact particles shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FCompactParticlesCS_RDG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify compact particles shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FCompactParticlesCS_RDG::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FWriteTotalCountCS_RDG,
	"/Plugin/KawaiiFluidSystem/Private/FluidDespawnParticles.usf",
	"WriteTotalCountCS", SF_Compute);

/**
 * @brief Check if write total count shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FWriteTotalCountCS_RDG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}
//=============================================================================
// GPU Adhesion Shaders (Bone-based attachment)
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FAdhesionCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidAdhesion.usf",
	"AdhesionCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FUpdateAttachedPositionsCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidAdhesion.usf",
	"UpdateAttachedPositionsCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FClearDetachedFlagCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidAdhesion.usf",
	"ClearDetachedFlagCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FStackPressureCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidStackPressure.usf",
	"StackPressureCS", SF_Compute);

//=============================================================================
// Boundary Adhesion Shaders (Flex-style with Spatial Hash)
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FClearBoundaryHashCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundaryAdhesion.usf",
	"ClearBoundaryHashCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FBuildBoundaryHashCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundaryAdhesion.usf",
	"BuildBoundaryHashCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FBoundaryAdhesionCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundaryAdhesion.usf",
	"BoundaryAdhesionCS", SF_Compute);

//=============================================================================
// GPU Boundary Skinning Shader
// Transforms bone-local boundary particles to world space
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FBoundarySkinningCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundarySkinning.usf",
	"BoundarySkinningCS", SF_Compute);

//=============================================================================
// Z-Order (Morton Code) Sorting Shaders
// GPU-based spatial sorting for cache-coherent neighbor access
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FComputeMortonCodesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidMortonCode.usf",
	"ComputeMortonCodesCellBasedCS", SF_Compute);

/**
 * @brief Check if compute morton codes shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FComputeMortonCodesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify compute morton codes shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FComputeMortonCodesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);

	// Get grid resolution from permutation
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
	const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
	const int32 GridSize = GridResolutionPermutation::GetGridResolution(GridPreset);
	const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

	OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
	OutEnvironment.SetDefine(TEXT("MORTON_GRID_SIZE"), GridSize);
	OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
}

IMPLEMENT_GLOBAL_SHADER(FRadixSortHistogramCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidRadixSort.usf",
	"RadixSortHistogramCS", SF_Compute);

/**
 * @brief Check if radix sort histogram shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FRadixSortHistogramCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify radix sort histogram shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FRadixSortHistogramCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("RADIX_BITS"), GPU_RADIX_BITS);
	OutEnvironment.SetDefine(TEXT("RADIX_SIZE"), GPU_RADIX_SIZE);
}

IMPLEMENT_GLOBAL_SHADER(FRadixSortGlobalPrefixSumCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidRadixSort.usf",
	"RadixSortGlobalPrefixSumCS", SF_Compute);

/**
 * @brief Check if radix sort global prefix sum shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FRadixSortGlobalPrefixSumCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify radix sort global prefix sum shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FRadixSortGlobalPrefixSumCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("RADIX_BITS"), GPU_RADIX_BITS);
	OutEnvironment.SetDefine(TEXT("RADIX_SIZE"), GPU_RADIX_SIZE);
}

IMPLEMENT_GLOBAL_SHADER(FRadixSortBucketPrefixSumCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidRadixSort.usf",
	"RadixSortBucketPrefixSumCS", SF_Compute);

/**
 * @brief Check if radix sort bucket prefix sum shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FRadixSortBucketPrefixSumCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify radix sort bucket prefix sum shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FRadixSortBucketPrefixSumCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("RADIX_BITS"), GPU_RADIX_BITS);
	OutEnvironment.SetDefine(TEXT("RADIX_SIZE"), GPU_RADIX_SIZE);
}

IMPLEMENT_GLOBAL_SHADER(FRadixSortScatterCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidRadixSort.usf",
	"RadixSortScatterCS", SF_Compute);

/**
 * @brief Check if radix sort scatter shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FRadixSortScatterCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify radix sort scatter shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FRadixSortScatterCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
	OutEnvironment.SetDefine(TEXT("RADIX_BITS"), GPU_RADIX_BITS);
	OutEnvironment.SetDefine(TEXT("RADIX_SIZE"), GPU_RADIX_SIZE);
}

IMPLEMENT_GLOBAL_SHADER(FRadixSortSmallCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidRadixSort.usf",
	"RadixSortSmallCS", SF_Compute);

/**
 * @brief Check if radix sort small shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FRadixSortSmallCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify radix sort small shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FRadixSortSmallCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FReorderParticlesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidReorderParticles.usf",
	"ReorderParticlesCS", SF_Compute);

/**
 * @brief Check if reorder particles shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FReorderParticlesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify reorder particles shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FReorderParticlesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FBuildReverseMappingCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidReorderParticles.usf",
	"BuildReverseMappingCS", SF_Compute);

/**
 * @brief Check if build reverse mapping shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FBuildReverseMappingCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify build reverse mapping shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FBuildReverseMappingCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

//=============================================================================
// Cell Start/End Index Shaders
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FClearCellIndicesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidCellStartEnd.usf",
	"ClearCellIndicesCS", SF_Compute);

/**
 * @brief Check if clear cell indices shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FClearCellIndicesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify clear cell indices shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FClearCellIndicesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);

	// Get grid resolution from permutation
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
	const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
	const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

	OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
	OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
}

IMPLEMENT_GLOBAL_SHADER(FComputeCellStartEndCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidCellStartEnd.usf",
	"ComputeCellStartEndCS", SF_Compute);

/**
 * @brief Check if compute cell start end shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FComputeCellStartEndCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify compute cell start end shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FComputeCellStartEndCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);

	// Get grid resolution from permutation
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
	const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
	const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

	OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
	OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
}

//=============================================================================
// Boundary Particle Z-Order Sorting Shaders
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FComputeBoundaryMortonCodesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundaryZOrder.usf",
	"ComputeBoundaryMortonCodesCS", SF_Compute);

/**
 * @brief Check if compute boundary morton codes shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FComputeBoundaryMortonCodesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify compute boundary morton codes shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FComputeBoundaryMortonCodesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);

	// Get grid resolution from permutation
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
	const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
	const int32 GridSize = GridResolutionPermutation::GetGridResolution(GridPreset);
	const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

	OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
	OutEnvironment.SetDefine(TEXT("MORTON_GRID_SIZE"), GridSize);
	OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
}

IMPLEMENT_GLOBAL_SHADER(FClearBoundaryCellIndicesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundaryZOrder.usf",
	"ClearBoundaryCellIndicesCS", SF_Compute);

/**
 * @brief Check if clear boundary cell indices shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FClearBoundaryCellIndicesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify clear boundary cell indices shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FClearBoundaryCellIndicesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);

	// Get grid resolution from permutation
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
	const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
	const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

	OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
	OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
}

IMPLEMENT_GLOBAL_SHADER(FReorderBoundaryParticlesCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundaryZOrder.usf",
	"ReorderBoundaryParticlesCS", SF_Compute);

/**
 * @brief Check if reorder boundary particles shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FReorderBoundaryParticlesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify reorder boundary particles shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FReorderBoundaryParticlesCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FComputeBoundaryCellStartEndCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidBoundaryZOrder.usf",
	"ComputeBoundaryCellStartEndCS", SF_Compute);

/**
 * @brief Check if compute boundary cell start end shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FComputeBoundaryCellStartEndCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify compute boundary cell start end shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FComputeBoundaryCellStartEndCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);

	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
	const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
	const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

	OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
	OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
}

//=============================================================================
// Bone Delta Attachment Shaders (NEW simplified bone-following system)
// ApplyBoneTransform: Simulation start - transform attached particles with bones
// UpdateBoneDeltaAttachment: Simulation end - update attachment data, detach check
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FApplyBoneTransformCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidApplyBoneTransform.usf",
	"ApplyBoneTransformCS", SF_Compute);

/**
 * @brief Check if apply bone transform shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FApplyBoneTransformCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify apply bone transform shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FApplyBoneTransformCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FUpdateBoneDeltaAttachmentCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidUpdateBoneDeltaAttachment.usf",
	"UpdateBoneDeltaAttachmentCS", SF_Compute);

/**
 * @brief Check if update bone delta attachment shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FUpdateBoneDeltaAttachmentCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify update bone delta attachment shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FUpdateBoneDeltaAttachmentCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);

	// Get grid resolution from permutation for Morton code calculation
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	const int32 GridPreset = PermutationVector.Get<FGridResolutionDim>();
	const int32 AxisBits = GridResolutionPermutation::GetAxisBits(GridPreset);
	const int32 MaxCells = GridResolutionPermutation::GetMaxCells(GridPreset);

	OutEnvironment.SetDefine(TEXT("MORTON_GRID_AXIS_BITS"), AxisBits);
	OutEnvironment.SetDefine(TEXT("MAX_CELLS"), MaxCells);
}

//=============================================================================
// Pass Builder Implementation
//=============================================================================

// DEPRECATED: Legacy function - use GPUCollisionManager::AddPrimitiveCollisionPass instead
#if 0
void FGPUFluidSimulatorPassBuilder::AddPrimitiveCollisionPass(
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
	float CollisionThreshold)
{
	if (ParticleCount <= 0 || !ParticlesUAV)
	{
		return;
	}

	// Skip if no primitives
	if (SphereCount == 0 && CapsuleCount == 0 && BoxCount == 0 && ConvexCount == 0)
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FPrimitiveCollisionCS> ComputeShader(GlobalShaderMap);

	FPrimitiveCollisionCS::FParameters* PassParameters =
		GraphBuilder.AllocParameters<FPrimitiveCollisionCS::FParameters>();

	PassParameters->Particles = ParticlesUAV;
	PassParameters->ParticleCount = ParticleCount;
	PassParameters->ParticleRadius = ParticleRadius;
	PassParameters->CollisionThreshold = CollisionThreshold;

	PassParameters->CollisionSpheres = SpheresSRV;
	PassParameters->SphereCount = SphereCount;

	PassParameters->CollisionCapsules = CapsulesSRV;
	PassParameters->CapsuleCount = CapsuleCount;

	PassParameters->CollisionBoxes = BoxesSRV;
	PassParameters->BoxCount = BoxCount;

	PassParameters->CollisionConvexes = ConvexesSRV;
	PassParameters->ConvexCount = ConvexCount;

	PassParameters->ConvexPlanes = ConvexPlanesSRV;

	const int32 ThreadGroupSize = FPrimitiveCollisionCS::ThreadGroupSize;
	const int32 NumGroups = FMath::DivideAndRoundUp(ParticleCount, ThreadGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::PrimitiveCollision(%d particles, %d primitives)",
			ParticleCount, SphereCount + CapsuleCount + BoxCount + ConvexCount),
		ComputeShader,
		PassParameters,
		FIntVector(NumGroups, 1, 1));
}
#endif // DEPRECATED

/**
 * @brief Add RDG pass to extract particle data for rendering.
 * @param GraphBuilder RDG builder.
 * @param PhysicsParticlesSRV Read-only access to physics particle buffer.
 * @param RenderParticlesUAV Read-write access to output render particle buffer.
 * @param ParticleCountBufferSRV Read-only access to GPU particle count.
 * @param MaxParticleCount Maximum particle capacity.
 * @param ParticleRadius Radius for rendering.
 */
void FGPUFluidSimulatorPassBuilder::AddExtractRenderDataPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef PhysicsParticlesSRV,
	FRDGBufferUAVRef RenderParticlesUAV,
	FRDGBufferSRVRef ParticleCountBufferSRV,
	int32 MaxParticleCount,
	float ParticleRadius)
{
	if (MaxParticleCount <= 0 || !PhysicsParticlesSRV || !RenderParticlesUAV || !ParticleCountBufferSRV)
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FExtractRenderDataCS> ComputeShader(GlobalShaderMap);

	FExtractRenderDataCS::FParameters* PassParameters =
		GraphBuilder.AllocParameters<FExtractRenderDataCS::FParameters>();

	PassParameters->PhysicsParticles = PhysicsParticlesSRV;
	PassParameters->RenderParticles = RenderParticlesUAV;
	PassParameters->ParticleCountBuffer = ParticleCountBufferSRV;
	PassParameters->ParticleRadius = ParticleRadius;

	// Dispatch enough groups to cover max capacity; shader reads GPU count for bounds check
	const int32 ThreadGroupSize = FExtractRenderDataCS::ThreadGroupSize;
	const int32 NumGroups = FMath::DivideAndRoundUp(MaxParticleCount, ThreadGroupSize);

	if (NumGroups > 0)
	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::ExtractRenderData(max=%d)", MaxParticleCount),
			ComputeShader,
			PassParameters,
			FIntVector(NumGroups, 1, 1));
	}
}

/**
 * @brief Add RDG pass to extract render data and compute world-space bounds.
 * @param GraphBuilder RDG builder.
 * @param PhysicsParticlesSRV Read-only access to physics particle buffer.
 * @param RenderParticlesUAV Read-write access to output render particle buffer.
 * @param BoundsBufferUAV Read-write access to output bounds buffer.
 * @param ParticleCountBufferSRV Read-only access to GPU particle count.
 * @param ParticleRadius Radius for rendering.
 * @param BoundsMargin Extra margin to expand computed bounds.
 */
void FGPUFluidSimulatorPassBuilder::AddExtractRenderDataWithBoundsPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef PhysicsParticlesSRV,
	FRDGBufferUAVRef RenderParticlesUAV,
	FRDGBufferUAVRef BoundsBufferUAV,
	FRDGBufferSRVRef ParticleCountBufferSRV,
	float ParticleRadius,
	float BoundsMargin)
{
	if (!PhysicsParticlesSRV || !RenderParticlesUAV || !BoundsBufferUAV || !ParticleCountBufferSRV)
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FExtractRenderDataWithBoundsCS> ComputeShader(GlobalShaderMap);

	FExtractRenderDataWithBoundsCS::FParameters* PassParameters =
		GraphBuilder.AllocParameters<FExtractRenderDataWithBoundsCS::FParameters>();

	PassParameters->PhysicsParticles = PhysicsParticlesSRV;
	PassParameters->RenderParticles = RenderParticlesUAV;
	PassParameters->OutputBounds = BoundsBufferUAV;
	PassParameters->ParticleCountBuffer = ParticleCountBufferSRV;
	PassParameters->ParticleRadius = ParticleRadius;
	PassParameters->BoundsMargin = BoundsMargin;

	// Single group of 256 threads with grid-stride loop (reads GPU count internally)
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::ExtractRenderDataWithBounds"),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1));
}

/**
 * @brief Add RDG pass to extract render data in SoA format.
 * @param GraphBuilder RDG builder.
 * @param PhysicsParticlesSRV Read-only access to physics particle buffer.
 * @param RenderPositionsUAV Read-write access to output position buffer.
 * @param RenderVelocitiesUAV Read-write access to output velocity buffer.
 * @param ParticleCountBufferSRV Read-only access to GPU particle count.
 * @param MaxParticleCount Maximum particle capacity.
 * @param ParticleRadius Radius for rendering.
 */
void FGPUFluidSimulatorPassBuilder::AddExtractRenderDataSoAPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef PhysicsParticlesSRV,
	FRDGBufferUAVRef RenderPositionsUAV,
	FRDGBufferUAVRef RenderVelocitiesUAV,
	FRDGBufferSRVRef ParticleCountBufferSRV,
	int32 MaxParticleCount,
	float ParticleRadius)
{
	if (MaxParticleCount <= 0 || !PhysicsParticlesSRV || !RenderPositionsUAV || !RenderVelocitiesUAV || !ParticleCountBufferSRV)
	{
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FExtractRenderDataSoACS> ComputeShader(GlobalShaderMap);

	FExtractRenderDataSoACS::FParameters* PassParameters =
		GraphBuilder.AllocParameters<FExtractRenderDataSoACS::FParameters>();

	PassParameters->PhysicsParticles = PhysicsParticlesSRV;
	PassParameters->RenderPositions = RenderPositionsUAV;
	PassParameters->RenderVelocities = RenderVelocitiesUAV;
	PassParameters->ParticleCountBuffer = ParticleCountBufferSRV;
	PassParameters->ParticleRadius = ParticleRadius;

	// Dispatch enough groups to cover max capacity; shader reads GPU count for bounds check
	const int32 ThreadGroupSize = FExtractRenderDataSoACS::ThreadGroupSize;
	const int32 NumGroups = FMath::DivideAndRoundUp(MaxParticleCount, ThreadGroupSize);

	if (NumGroups > 0)
	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::ExtractRenderDataSoA(max=%d)", MaxParticleCount),
			ComputeShader,
			PassParameters,
			FIntVector(NumGroups, 1, 1));
	}
}

//=============================================================================
// SoA Conversion Shaders
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FSplitAoSToSoACS, "/Plugin/KawaiiFluidSystem/Private/FluidParticleSoA.usf", "SplitAoSToSoACS", SF_Compute);

/**
 * @brief Check if split AoS to SoA shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FSplitAoSToSoACS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify split AoS to SoA shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FSplitAoSToSoACS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), (int32)ThreadGroupSize);
}

IMPLEMENT_GLOBAL_SHADER(FMergeSoAToAoSCS, "/Plugin/KawaiiFluidSystem/Private/FluidParticleSoA.usf", "MergeSoAToAoSCS", SF_Compute);

/**
 * @brief Check if merge SoA to AoS shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FMergeSoAToAoSCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

/**
 * @brief Modify merge SoA to AoS shader compilation environment.
 * @param Parameters Shader permutation parameters.
 * @param OutEnvironment Shader compiler environment to modify.
 */
void FMergeSoAToAoSCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), (int32)ThreadGroupSize);
}

//=============================================================================
// Indirect Dispatch Particle Count Shaders
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FWriteAliveCountAfterCompactionCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidUpdateParticleCount.usf",
	"WriteAliveCountAfterCompactionCS", SF_Compute);

/**
 * @brief Check if write alive count after compaction shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FWriteAliveCountAfterCompactionCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

IMPLEMENT_GLOBAL_SHADER(FUpdateCountAfterSpawnCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidUpdateParticleCount.usf",
	"UpdateCountAfterSpawnCS", SF_Compute);

/**
 * @brief Check if update count after spawn shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FUpdateCountAfterSpawnCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

IMPLEMENT_GLOBAL_SHADER(FCopyCountToSpawnCounterCS,
	"/Plugin/KawaiiFluidSystem/Private/FluidCopyParticleCount.usf",
	"CopyCountToSpawnCounterCS", SF_Compute);

/**
 * @brief Check if copy count to spawn counter shader permutation should be compiled.
 * @param Parameters Shader permutation parameters.
 * @return True if permutation is supported.
 */
bool FCopyCountToSpawnCounterCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}
