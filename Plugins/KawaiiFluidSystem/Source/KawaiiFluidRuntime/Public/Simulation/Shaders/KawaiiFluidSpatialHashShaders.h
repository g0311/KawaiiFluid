// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphDefinitions.h"

static constexpr uint32 SPATIAL_HASH_SIZE = 65536;
static constexpr uint32 MAX_PARTICLES_PER_CELL = 16;
static constexpr uint32 SPATIAL_HASH_THREAD_GROUP_SIZE = 256;

/**
 * @class FClearCellDataCS
 * @brief Compute shader to reset cell particle counts.
 */
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

/**
 * @class FBuildSpatialHashSimpleCS
 * @brief Compute shader for traditional hash-table based spatial partitioning.
 */
class FBuildSpatialHashSimpleCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FBuildSpatialHashSimpleCS);
    SHADER_USE_PARAMETER_STRUCT(FBuildSpatialHashSimpleCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector3f>, ParticlePositions)
        SHADER_PARAMETER(int32, ParticleCount)
        SHADER_PARAMETER(float, ParticleRadius)
        SHADER_PARAMETER(float, CellSize)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounts)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleIndices)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
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

/**
 * @class FSortCellParticlesCS
 * @brief Compute shader to sort particles within each hash cell.
 */
class FSortCellParticlesCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FSortCellParticlesCS);
    SHADER_USE_PARAMETER_STRUCT(FSortCellParticlesCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
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

/**
 * @class FClearCellDataMultipassCS
 * @brief Resets buffers for the multi-pass spatial hash build.
 */
class FClearCellDataMultipassCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FClearCellDataMultipassCS);
    SHADER_USE_PARAMETER_STRUCT(FClearCellDataMultipassCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector2>, CellData)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounters)
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
        OutEnvironment.SetDefine(TEXT("USE_MULTIPASS_BUILD"), 1);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
    }
};

/**
 * @class FCountParticlesPerCellCS
 * @brief Pass 1: Count number of particles per hash cell.
 */
class FCountParticlesPerCellCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FCountParticlesPerCellCS);
    SHADER_USE_PARAMETER_STRUCT(FCountParticlesPerCellCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector3f>, ParticlePositions)
        SHADER_PARAMETER(int32, ParticleCount)
        SHADER_PARAMETER(float, CellSize)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounters)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleCellHashes)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
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
        OutEnvironment.SetDefine(TEXT("USE_MULTIPASS_BUILD"), 1);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
    }
};

/**
 * @class FPrefixSumCS
 * @brief Pass 2: Calculate prefix sums to determine starting indices for each cell.
 */
class FPrefixSumCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FPrefixSumCS);
    SHADER_USE_PARAMETER_STRUCT(FPrefixSumCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounters)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PrefixSumBuffer)
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
        OutEnvironment.SetDefine(TEXT("USE_MULTIPASS_BUILD"), 1);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
    }
};

/**
 * @class FScatterParticlesCS
 * @brief Pass 3: Scatter particle indices into their corresponding cells based on prefix sums.
 */
class FScatterParticlesCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FScatterParticlesCS);
    SHADER_USE_PARAMETER_STRUCT(FScatterParticlesCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleCellHashes)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PrefixSumBuffer)
        SHADER_PARAMETER(int32, ParticleCount)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounters)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleIndices)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCountBuffer)
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
        OutEnvironment.SetDefine(TEXT("USE_MULTIPASS_BUILD"), 1);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
    }
};

/**
 * @class FFinalizeCellDataCS
 * @brief Pass 4: Store finalized starting indices and counts for each cell.
 */
class FFinalizeCellDataCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFinalizeCellDataCS);
    SHADER_USE_PARAMETER_STRUCT(FFinalizeCellDataCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PrefixSumBuffer)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounters)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector2>, CellData)
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
        OutEnvironment.SetDefine(TEXT("USE_MULTIPASS_BUILD"), 1);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
    }
};

/**
 * @class FSortBucketParticlesCS
 * @brief Final Pass: Sort particles within each dynamic bucket for consistency.
 */
class FSortBucketParticlesCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FSortBucketParticlesCS);
    SHADER_USE_PARAMETER_STRUCT(FSortBucketParticlesCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector2>, CellData)
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
        OutEnvironment.SetDefine(TEXT("USE_MULTIPASS_BUILD"), 1);
        OutEnvironment.SetDefine(TEXT("SPATIAL_HASH_SIZE"), SPATIAL_HASH_SIZE);
    }
};

/**
 * @struct FSpatialHashGPUResources
 * @brief GPU resources for the simplified spatial hash acceleration structure.
 * 
 * @param CellCountsBuffer Buffer storing the number of particles per cell.
 * @param ParticleIndicesBuffer Buffer storing particle indices sorted by cell.
 * @param CellSize Spatial partitioning cell size in cm.
 */
struct FSpatialHashGPUResources
{
    FRDGBufferRef CellCountsBuffer = nullptr;
    FRDGBufferSRVRef CellCountsSRV = nullptr;
    FRDGBufferUAVRef CellCountsUAV = nullptr;

    FRDGBufferRef ParticleIndicesBuffer = nullptr;
    FRDGBufferSRVRef ParticleIndicesSRV = nullptr;
    FRDGBufferUAVRef ParticleIndicesUAV = nullptr;

    float CellSize = 0.0f;

    bool IsValid() const
    {
        return CellCountsBuffer != nullptr && ParticleIndicesBuffer != nullptr;
    }
};

/**
 * @struct FSpatialHashMultipassResources
 * @brief GPU resources for the dynamic multi-pass spatial hash.
 * 
 * @param CellDataBuffer Buffer storing {startIndex, count} pairs for each cell.
 * @param ParticleIndicesBuffer Buffer storing all particle indices sorted by cell.
 * @param CellCountersBuffer Temporary atomic counters for cell processing.
 * @param ParticleCellHashesBuffer Temporary buffer storing hash values for each particle.
 * @param PrefixSumBuffer Temporary buffer for prefix sum calculation.
 * @param CellSize Spatial partitioning cell size in cm.
 * @param ParticleCount Total number of particles managed.
 */
struct FSpatialHashMultipassResources
{
    FRDGBufferRef CellDataBuffer = nullptr;
    FRDGBufferSRVRef CellDataSRV = nullptr;
    FRDGBufferUAVRef CellDataUAV = nullptr;

    FRDGBufferRef ParticleIndicesBuffer = nullptr;
    FRDGBufferSRVRef ParticleIndicesSRV = nullptr;
    FRDGBufferUAVRef ParticleIndicesUAV = nullptr;

    FRDGBufferRef CellCountersBuffer = nullptr;
    FRDGBufferUAVRef CellCountersUAV = nullptr;

    FRDGBufferRef ParticleCellHashesBuffer = nullptr;
    FRDGBufferSRVRef ParticleCellHashesSRV = nullptr;
    FRDGBufferUAVRef ParticleCellHashesUAV = nullptr;

    FRDGBufferRef PrefixSumBuffer = nullptr;
    FRDGBufferSRVRef PrefixSumSRV = nullptr;
    FRDGBufferUAVRef PrefixSumUAV = nullptr;

    float CellSize = 0.0f;
    int32 ParticleCount = 0;

    bool IsValid() const
    {
        return CellDataBuffer != nullptr && ParticleIndicesBuffer != nullptr;
    }

    void Reset()
    {
        CellDataBuffer = nullptr;
        CellDataSRV = nullptr;
        CellDataUAV = nullptr;
        ParticleIndicesBuffer = nullptr;
        ParticleIndicesSRV = nullptr;
        ParticleIndicesUAV = nullptr;
        CellCountersBuffer = nullptr;
        CellCountersUAV = nullptr;
        ParticleCellHashesBuffer = nullptr;
        ParticleCellHashesSRV = nullptr;
        ParticleCellHashesUAV = nullptr;
        PrefixSumBuffer = nullptr;
        PrefixSumSRV = nullptr;
        PrefixSumUAV = nullptr;
        CellSize = 0.0f;
        ParticleCount = 0;
    }
};

/**
 * @class FSpatialHashBuilder
 * @brief Utility class for building and managing spatial hash structures on the GPU using RDG.
 */
class KAWAIIFLUIDRUNTIME_API FSpatialHashBuilder
{
public:
    static FSpatialHashGPUResources CreateResources(
        FRDGBuilder& GraphBuilder,
        float CellSize);

    static bool BuildHash(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef ParticlePositionsSRV,
        int32 ParticleCount,
        float ParticleRadius,
        FSpatialHashGPUResources& Resources,
        FRDGBufferRef IndirectArgsBuffer = nullptr);

    static bool CreateAndBuildHash(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef ParticlePositionsSRV,
        int32 ParticleCount,
        float ParticleRadius,
        float CellSize,
        FSpatialHashGPUResources& OutResources,
        FRDGBufferRef IndirectArgsBuffer = nullptr);

    static bool CreateAndBuildHashMultipass(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef ParticlePositionsSRV,
        int32 ParticleCount,
        float CellSize,
        FSpatialHashMultipassResources& OutResources,
        FRDGBufferRef IndirectArgsBuffer = nullptr);

private:
    static void ClearBuffers(
        FRDGBuilder& GraphBuilder,
        FSpatialHashGPUResources& Resources);

    static void SortCellParticles(
        FRDGBuilder& GraphBuilder,
        FSpatialHashGPUResources& Resources);
};