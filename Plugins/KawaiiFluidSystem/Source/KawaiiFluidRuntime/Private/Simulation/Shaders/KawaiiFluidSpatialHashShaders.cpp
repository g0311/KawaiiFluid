// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Simulation/Shaders/KawaiiFluidSpatialHashShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

//=============================================================================
// Shader Implementations
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FClearCellDataCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "ClearCellDataCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FBuildSpatialHashSimpleCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "BuildSpatialHashSimpleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSortCellParticlesCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "SortCellParticlesCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FClearCellDataMultipassCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "ClearCellDataMultipassCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FCountParticlesPerCellCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "CountParticlesPerCellCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPrefixSumCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "PrefixSumCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FScatterParticlesCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "ScatterParticlesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FFinalizeCellDataCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "FinalizeCellDataCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSortBucketParticlesCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "SortBucketParticlesCS", SF_Compute);

//=============================================================================
// FSpatialHashBuilder Implementation
//=============================================================================

/**
 * @brief Create GPU buffers for the simplified spatial hash structure.
 * @param GraphBuilder RDG builder for resource creation.
 * @param CellSize Dimension of the spatial cells in cm.
 * @return Struct containing initialized RDG buffer references.
 */
FSpatialHashGPUResources FSpatialHashBuilder::CreateResources(
    FRDGBuilder& GraphBuilder,
    float CellSize)
{
    FSpatialHashGPUResources Resources;
    Resources.CellSize = CellSize;

    // Cell counts buffer: uint per cell (count only, startIndex is implicit)
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SPATIAL_HASH_SIZE);
        Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
        Resources.CellCountsBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.CellCounts"));
        Resources.CellCountsSRV = GraphBuilder.CreateSRV(Resources.CellCountsBuffer);
        Resources.CellCountsUAV = GraphBuilder.CreateUAV(Resources.CellCountsBuffer);
    }

    // Particle indices buffer: fixed layout (HASH_SIZE * MAX_PER_CELL)
    {
        uint32 TotalSlots = SPATIAL_HASH_SIZE * MAX_PARTICLES_PER_CELL;
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TotalSlots);
        Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
        Resources.ParticleIndicesBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.ParticleIndices"));
        Resources.ParticleIndicesSRV = GraphBuilder.CreateSRV(Resources.ParticleIndicesBuffer);
        Resources.ParticleIndicesUAV = GraphBuilder.CreateUAV(Resources.ParticleIndicesBuffer);
    }

    return Resources;
}

/**
 * @brief Dispatch a compute shader pass to clear the cell counts buffer.
 */
void FSpatialHashBuilder::ClearBuffers(
    FRDGBuilder& GraphBuilder,
    FSpatialHashGPUResources& Resources)
{
    FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    if (!ShaderMap)
    {
        return;
    }

    TShaderMapRef<FClearCellDataCS> ComputeShader(ShaderMap);
    if (!ComputeShader.IsValid())
    {
        return;
    }

    FClearCellDataCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearCellDataCS::FParameters>();
    PassParameters->CellCounts = Resources.CellCountsUAV;

    uint32 NumGroups = FMath::DivideAndRoundUp(SPATIAL_HASH_SIZE, SPATIAL_HASH_THREAD_GROUP_SIZE);

    GraphBuilder.AddPass(
        RDG_EVENT_NAME("SpatialHash::Clear"),
        PassParameters,
        ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
        [ComputeShader, PassParameters, NumGroups](FRHIComputeCommandList& RHICmdList)
        {
            FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, FIntVector(NumGroups, 1, 1));
        });
}

/**
 * @brief Build the spatial hash by assigning particle indices to grid cells.
 * 
 * @param GraphBuilder RDG builder.
 * @param ParticlePositionsSRV SRV of particle world-space positions.
 * @param ParticleCount Total number of particles to process.
 * @param ParticleRadius Radius used for cell boundary checks.
 * @param Resources Target hash resources to populate.
 * @param IndirectArgsBuffer Optional indirect arguments for dynamic particle counts.
 * @return True if the build pass was successfully registered.
 */
bool FSpatialHashBuilder::BuildHash(
    FRDGBuilder& GraphBuilder,
    FRDGBufferSRVRef ParticlePositionsSRV,
    int32 ParticleCount,
    float ParticleRadius,
    FSpatialHashGPUResources& Resources,
    FRDGBufferRef IndirectArgsBuffer)
{
    if (!Resources.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("FSpatialHashBuilder::BuildHash - Resources not valid"));
        return false;
    }

    if (ParticleCount <= 0 || !ParticlePositionsSRV)
    {
        UE_LOG(LogTemp, Warning, TEXT("FSpatialHashBuilder::BuildHash - Invalid inputs (ParticleCount: %d)"), ParticleCount);
        return false;
    }

    FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    if (!ShaderMap)
    {
        return false;
    }

    TShaderMapRef<FBuildSpatialHashSimpleCS> BuildShader(ShaderMap);
    TShaderMapRef<FClearCellDataCS> ClearShader(ShaderMap);

    if (!BuildShader.IsValid() || !ClearShader.IsValid())
    {
        return false;
    }

    RDG_EVENT_SCOPE(GraphBuilder, "SpatialHash::Build (Particles: %d)", ParticleCount);

    // Step 1: Clear buffers (CellCounts only)
    {
        FClearCellDataCS::FParameters* ClearParams = GraphBuilder.AllocParameters<FClearCellDataCS::FParameters>();
        ClearParams->CellCounts = Resources.CellCountsUAV;

        uint32 NumGroups = FMath::DivideAndRoundUp(SPATIAL_HASH_SIZE, SPATIAL_HASH_THREAD_GROUP_SIZE);

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("SpatialHash::Clear"),
            ClearParams,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [ClearShader, ClearParams, NumGroups](FRHIComputeCommandList& RHICmdList)
            {
                FComputeShaderUtils::Dispatch(RHICmdList, ClearShader, *ClearParams, FIntVector(NumGroups, 1, 1));
            });
    }

    // Step 2: Build hash
    {
        FBuildSpatialHashSimpleCS::FParameters* PassParameters =
            GraphBuilder.AllocParameters<FBuildSpatialHashSimpleCS::FParameters>();

        PassParameters->ParticlePositions = ParticlePositionsSRV;
        PassParameters->ParticleCount = ParticleCount;
        if (IndirectArgsBuffer)
        {
            PassParameters->ParticleCountBuffer = GraphBuilder.CreateSRV(IndirectArgsBuffer);
        }
        PassParameters->ParticleRadius = ParticleRadius;
        PassParameters->CellSize = Resources.CellSize;
        PassParameters->CellCounts = Resources.CellCountsUAV;
        PassParameters->ParticleIndices = Resources.ParticleIndicesUAV;

        uint32 NumGroups = FMath::DivideAndRoundUp(static_cast<uint32>(ParticleCount), SPATIAL_HASH_THREAD_GROUP_SIZE);

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("SpatialHash::BuildSimple"),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [BuildShader, PassParameters, NumGroups](FRHIComputeCommandList& RHICmdList)
            {
                FComputeShaderUtils::Dispatch(RHICmdList, BuildShader, *PassParameters, FIntVector(NumGroups, 1, 1));
            });
    }

    return true;
}

/**
 * @brief Atomically create resources and build the spatial hash grid.
 * 
 * RECOMMENDED: This is the safe way to use spatial hash - it validates conditions 
 * before creating RDG resources, preventing orphan buffer crashes.
 */
bool FSpatialHashBuilder::CreateAndBuildHash(
    FRDGBuilder& GraphBuilder,
    FRDGBufferSRVRef ParticlePositionsSRV,
    int32 ParticleCount,
    float ParticleRadius,
    float CellSize,
    FSpatialHashGPUResources& OutResources,
    FRDGBufferRef IndirectArgsBuffer)
{
    OutResources = FSpatialHashGPUResources();

    if (ParticleCount <= 0 || !ParticlePositionsSRV)
    {
        return false;
    }

    if (CellSize <= 0.0f)
    {
        return false;
    }

    FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    if (!ShaderMap)
    {
        return false;
    }

    TShaderMapRef<FBuildSpatialHashSimpleCS> BuildShader(ShaderMap);
    if (!BuildShader.IsValid())
    {
        return false;
    }

    OutResources.CellSize = CellSize;

    // Create buffers with initial zero data
    {
        TArray<uint32> InitialCellCounts;
        InitialCellCounts.SetNumZeroed(SPATIAL_HASH_SIZE);

        OutResources.CellCountsBuffer = CreateStructuredBuffer(
            GraphBuilder,
            TEXT("SpatialHash.CellCounts"),
            sizeof(uint32),
            SPATIAL_HASH_SIZE,
            InitialCellCounts.GetData(),
            InitialCellCounts.Num() * sizeof(uint32));

        OutResources.CellCountsSRV = GraphBuilder.CreateSRV(OutResources.CellCountsBuffer);
        OutResources.CellCountsUAV = GraphBuilder.CreateUAV(OutResources.CellCountsBuffer);
    }

    {
        uint32 TotalSlots = SPATIAL_HASH_SIZE * MAX_PARTICLES_PER_CELL;
        TArray<uint32> InitialIndices;
        InitialIndices.SetNumZeroed(TotalSlots);

        OutResources.ParticleIndicesBuffer = CreateStructuredBuffer(
            GraphBuilder,
            TEXT("SpatialHash.ParticleIndices"),
            sizeof(uint32),
            TotalSlots,
            InitialIndices.GetData(),
            InitialIndices.Num() * sizeof(uint32));

        OutResources.ParticleIndicesSRV = GraphBuilder.CreateSRV(OutResources.ParticleIndicesBuffer);
        OutResources.ParticleIndicesUAV = GraphBuilder.CreateUAV(OutResources.ParticleIndicesBuffer);
    }

    // Dispatch build pass
    {
        FBuildSpatialHashSimpleCS::FParameters* PassParameters =
            GraphBuilder.AllocParameters<FBuildSpatialHashSimpleCS::FParameters>();

        PassParameters->ParticlePositions = ParticlePositionsSRV;
        PassParameters->ParticleCount = ParticleCount;
        if (IndirectArgsBuffer)
        {
            PassParameters->ParticleCountBuffer = GraphBuilder.CreateSRV(IndirectArgsBuffer);
        }
        PassParameters->ParticleRadius = ParticleRadius;
        PassParameters->CellSize = CellSize;
        PassParameters->CellCounts = OutResources.CellCountsUAV;
        PassParameters->ParticleIndices = OutResources.ParticleIndicesUAV;

        uint32 NumGroups = FMath::DivideAndRoundUp(static_cast<uint32>(ParticleCount), SPATIAL_HASH_THREAD_GROUP_SIZE);

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("SpatialHash::Build"),
            BuildShader,
            PassParameters,
            FIntVector(NumGroups, 1, 1));
    }

    return true;
}

/**
 * @brief Build a dynamic multi-pass spatial hash without particle count limits per cell.
 * 
 * Uses prefix sums to allocate bucket sizes dynamically based on actual particle distribution.
 */
bool FSpatialHashBuilder::CreateAndBuildHashMultipass(
    FRDGBuilder& GraphBuilder,
    FRDGBufferSRVRef ParticlePositionsSRV,
    int32 ParticleCount,
    float CellSize,
    FSpatialHashMultipassResources& OutResources,
    FRDGBufferRef IndirectArgsBuffer)
{
    OutResources.Reset();

    if (ParticleCount <= 0 || !ParticlePositionsSRV)
    {
        return false;
    }

    if (CellSize <= 0.0f)
    {
        return false;
    }

    FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    if (!ShaderMap)
    {
        return false;
    }

    TShaderMapRef<FClearCellDataMultipassCS> ClearShader(ShaderMap);
    TShaderMapRef<FCountParticlesPerCellCS> CountShader(ShaderMap);
    TShaderMapRef<FPrefixSumCS> PrefixSumShader(ShaderMap);
    TShaderMapRef<FScatterParticlesCS> ScatterShader(ShaderMap);
    TShaderMapRef<FFinalizeCellDataCS> FinalizeShader(ShaderMap);
    TShaderMapRef<FSortBucketParticlesCS> SortShader(ShaderMap);

    if (!ClearShader.IsValid() || !CountShader.IsValid() || !PrefixSumShader.IsValid() ||
        !ScatterShader.IsValid() || !FinalizeShader.IsValid() || !SortShader.IsValid())
    {
        return false;
    }

    OutResources.CellSize = CellSize;
    OutResources.ParticleCount = ParticleCount;

    RDG_EVENT_SCOPE(GraphBuilder, "SpatialHash::BuildMultipass (Particles: %d)", ParticleCount);

    // Create resources
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector2), SPATIAL_HASH_SIZE);
        Desc.Usage |= EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
        OutResources.CellDataBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.CellData"));
        OutResources.CellDataSRV = GraphBuilder.CreateSRV(OutResources.CellDataBuffer);
        OutResources.CellDataUAV = GraphBuilder.CreateUAV(OutResources.CellDataBuffer);
    }

    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ParticleCount);
        Desc.Usage |= EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
        OutResources.ParticleIndicesBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.ParticleIndices"));
        OutResources.ParticleIndicesSRV = GraphBuilder.CreateSRV(OutResources.ParticleIndicesBuffer);
        OutResources.ParticleIndicesUAV = GraphBuilder.CreateUAV(OutResources.ParticleIndicesBuffer);
    }

    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SPATIAL_HASH_SIZE);
        Desc.Usage |= EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
        OutResources.CellCountersBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.CellCounters"));
        OutResources.CellCountersUAV = GraphBuilder.CreateUAV(OutResources.CellCountersBuffer);
    }

    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ParticleCount);
        Desc.Usage |= EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
        OutResources.ParticleCellHashesBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.ParticleCellHashes"));
        OutResources.ParticleCellHashesSRV = GraphBuilder.CreateSRV(OutResources.ParticleCellHashesBuffer);
        OutResources.ParticleCellHashesUAV = GraphBuilder.CreateUAV(OutResources.ParticleCellHashesBuffer);
    }

    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SPATIAL_HASH_SIZE);
        Desc.Usage |= EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
        OutResources.PrefixSumBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.PrefixSum"));
        OutResources.PrefixSumSRV = GraphBuilder.CreateSRV(OutResources.PrefixSumBuffer);
        OutResources.PrefixSumUAV = GraphBuilder.CreateUAV(OutResources.PrefixSumBuffer);
    }

    uint32 NumCellGroups = FMath::DivideAndRoundUp(SPATIAL_HASH_SIZE, SPATIAL_HASH_THREAD_GROUP_SIZE);
    uint32 NumParticleGroups = FMath::DivideAndRoundUp(static_cast<uint32>(ParticleCount), SPATIAL_HASH_THREAD_GROUP_SIZE);

    // Pass 1: Clear
    {
        FClearCellDataMultipassCS::FParameters* ClearParams =
            GraphBuilder.AllocParameters<FClearCellDataMultipassCS::FParameters>();
        ClearParams->CellData = OutResources.CellDataUAV;
        ClearParams->CellCounters = OutResources.CellCountersUAV;

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("SpatialHash::Clear"),
            ClearShader,
            ClearParams,
            FIntVector(NumCellGroups, 1, 1));
    }

    // Pass 2: Count
    {
        FCountParticlesPerCellCS::FParameters* CountParams =
            GraphBuilder.AllocParameters<FCountParticlesPerCellCS::FParameters>();
        CountParams->ParticlePositions = ParticlePositionsSRV;
        CountParams->ParticleCount = ParticleCount;
        if (IndirectArgsBuffer)
        {
            CountParams->ParticleCountBuffer = GraphBuilder.CreateSRV(IndirectArgsBuffer);
        }
        CountParams->CellSize = CellSize;
        CountParams->CellCounters = OutResources.CellCountersUAV;
        CountParams->ParticleCellHashes = OutResources.ParticleCellHashesUAV;

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("SpatialHash::CountParticles"),
            CountShader,
            CountParams,
            FIntVector(NumParticleGroups, 1, 1));
    }

    // Pass 3: Prefix Sum
    {
        FPrefixSumCS::FParameters* PrefixParams =
            GraphBuilder.AllocParameters<FPrefixSumCS::FParameters>();
        PrefixParams->CellCounters = OutResources.CellCountersUAV;
        PrefixParams->PrefixSumBuffer = OutResources.PrefixSumUAV;

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("SpatialHash::PrefixSum"),
            PrefixSumShader,
            PrefixParams,
            FIntVector(1, 1, 1));
    }

    AddClearUAVPass(GraphBuilder, OutResources.CellCountersUAV, 0u);

    // Pass 4: Scatter
    {
        FScatterParticlesCS::FParameters* ScatterParams =
            GraphBuilder.AllocParameters<FScatterParticlesCS::FParameters>();
        ScatterParams->ParticleCellHashes = OutResources.ParticleCellHashesUAV;
        ScatterParams->PrefixSumBuffer = OutResources.PrefixSumUAV;
        ScatterParams->ParticleCount = ParticleCount;
        if (IndirectArgsBuffer)
        {
            ScatterParams->ParticleCountBuffer = GraphBuilder.CreateSRV(IndirectArgsBuffer);
        }
        ScatterParams->CellCounters = OutResources.CellCountersUAV;
        ScatterParams->ParticleIndices = OutResources.ParticleIndicesUAV;

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("SpatialHash::Scatter"),
            ScatterShader,
            ScatterParams,
            FIntVector(NumParticleGroups, 1, 1));
    }

    // Pass 5: Finalize
    {
        FFinalizeCellDataCS::FParameters* FinalizeParams =
            GraphBuilder.AllocParameters<FFinalizeCellDataCS::FParameters>();
        FinalizeParams->PrefixSumBuffer = OutResources.PrefixSumUAV;
        FinalizeParams->CellCounters = OutResources.CellCountersUAV;
        FinalizeParams->CellData = OutResources.CellDataUAV;

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("SpatialHash::Finalize"),
            FinalizeShader,
            FinalizeParams,
            FIntVector(NumCellGroups, 1, 1));
    }

    // Pass 6: Sort
    {
        FSortBucketParticlesCS::FParameters* SortParams =
            GraphBuilder.AllocParameters<FSortBucketParticlesCS::FParameters>();
        SortParams->CellData = OutResources.CellDataUAV;
        SortParams->ParticleIndices = OutResources.ParticleIndicesUAV;

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("SpatialHash::Sort"),
            SortShader,
            SortParams,
            FIntVector(NumCellGroups, 1, 1));
    }

    return true;
}
