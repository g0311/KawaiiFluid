// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphDefinitions.h"

//=============================================================================
// Spatial Hash Constants (must match shader defines)
//=============================================================================

static constexpr uint32 SPATIAL_HASH_SIZE = 65536;          // 2^16 cells
static constexpr uint32 MAX_PARTICLES_PER_CELL = 16;
static constexpr uint32 SPATIAL_HASH_THREAD_GROUP_SIZE = 256;

//=============================================================================
// Clear Cell Data Compute Shader
//=============================================================================

class FClearCellDataCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FClearCellDataCS);
    SHADER_USE_PARAMETER_STRUCT(FClearCellDataCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounts)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& Parameters,
        FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
        OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), MAX_PARTICLES_PER_CELL);
    }
};

//=============================================================================
// Build Spatial Hash (Simple version) Compute Shader
//=============================================================================

class FBuildSpatialHashSimpleCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FBuildSpatialHashSimpleCS);
    SHADER_USE_PARAMETER_STRUCT(FBuildSpatialHashSimpleCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        // Input
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector3f>, ParticlePositions)
        SHADER_PARAMETER(int32, ParticleCount)
        SHADER_PARAMETER(float, ParticleRadius)
        SHADER_PARAMETER(float, CellSize)

        // Output
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounts)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleIndices)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& Parameters,
        FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
        OutEnvironment.SetDefine(TEXT("MAX_PARTICLES_PER_CELL"), MAX_PARTICLES_PER_CELL);
    }
};

//=============================================================================
// Spatial Hash GPU Resources
//=============================================================================

/**
 * GPU resources for spatial hash acceleration structure
 * Simplified: CellCounts stores only count per cell (uint)
 * StartIndex is implicit: Hash * MAX_PARTICLES_PER_CELL
 */
struct FSpatialHashGPUResources
{
    // Cell counts: count per cell (startIndex is implicit)
    FRDGBufferRef CellCountsBuffer = nullptr;
    FRDGBufferSRVRef CellCountsSRV = nullptr;
    FRDGBufferUAVRef CellCountsUAV = nullptr;

    // Particle indices sorted by cell
    FRDGBufferRef ParticleIndicesBuffer = nullptr;
    FRDGBufferSRVRef ParticleIndicesSRV = nullptr;
    FRDGBufferUAVRef ParticleIndicesUAV = nullptr;

    // Configuration
    float CellSize = 0.0f;

    bool IsValid() const
    {
        return CellCountsBuffer != nullptr && ParticleIndicesBuffer != nullptr;
    }
};

//=============================================================================
// Spatial Hash Builder
//=============================================================================

/**
 * Utility class for building spatial hash on GPU
 */
class KAWAIIFLUIDRUNTIME_API FSpatialHashBuilder
{
public:
    /**
     * Create GPU buffers for spatial hash
     * WARNING: Only call this if you will IMMEDIATELY add producer passes!
     * Orphan buffers (without producers) cause RDG crashes.
     */
    static FSpatialHashGPUResources CreateResources(
        FRDGBuilder& GraphBuilder,
        float CellSize);

    /**
     * Build spatial hash from particle positions
     * @return true if build succeeded, false if shaders not available
     * WARNING: Resources must already be created, and this function MUST succeed
     * to avoid orphan buffers.
     */
    static bool BuildHash(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef ParticlePositionsSRV,
        int32 ParticleCount,
        float ParticleRadius,
        FSpatialHashGPUResources& Resources);

    /**
     * RECOMMENDED: Atomically create resources and build hash.
     * This is the safe way to use spatial hash - it validates ALL conditions
     * BEFORE creating any RDG resources, preventing orphan buffer crashes.
     *
     * @param OutResources Output - will be populated only on success
     * @return true if build succeeded, false if conditions not met (no resources created)
     */
    static bool CreateAndBuildHash(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef ParticlePositionsSRV,
        int32 ParticleCount,
        float ParticleRadius,
        float CellSize,
        FSpatialHashGPUResources& OutResources);

private:
    static void ClearBuffers(
        FRDGBuilder& GraphBuilder,
        FSpatialHashGPUResources& Resources);
};
