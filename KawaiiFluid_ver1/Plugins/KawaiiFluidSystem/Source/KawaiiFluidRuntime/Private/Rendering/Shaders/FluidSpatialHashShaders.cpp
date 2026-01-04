// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Shaders/FluidSpatialHashShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

//=============================================================================
// Shader Implementations
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FClearCellCountsCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "ClearCellCountsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FCountParticlesCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "CountParticlesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPrefixSumCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "PrefixSumCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FClearWriteOffsetsCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "ClearWriteOffsetsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FScatterParticlesCS, "/Plugin/KawaiiFluidSystem/Private/FluidSpatialHashBuild.usf", "ScatterParticlesCS", SF_Compute);

//=============================================================================
// FSpatialHashBuilder Implementation
//=============================================================================

void FSpatialHashBuilder::CreateBuffers(
    FRDGBuilder& GraphBuilder,
    int32 ParticleCount,
    float CellSize,
    FSpatialHashGPUResources& OutResources)
{
    OutResources.CellSize = CellSize;
    OutResources.ParticleCount = ParticleCount;

    // CellCounts 버퍼: 셀별 파티클 개수
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SPATIAL_HASH_SIZE);
        Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
        OutResources.CellCountsBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.CellCounts"));
        OutResources.CellCountsSRV = GraphBuilder.CreateSRV(OutResources.CellCountsBuffer);
        OutResources.CellCountsUAV = GraphBuilder.CreateUAV(OutResources.CellCountsBuffer);
    }

    // CellStartIndices 버퍼: 셀별 시작 인덱스 (Prefix Sum 결과)
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SPATIAL_HASH_SIZE);
        Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
        OutResources.CellStartIndicesBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.CellStartIndices"));
        OutResources.CellStartIndicesSRV = GraphBuilder.CreateSRV(OutResources.CellStartIndicesBuffer);
        OutResources.CellStartIndicesUAV = GraphBuilder.CreateUAV(OutResources.CellStartIndicesBuffer);
    }

    // ParticleIndices 버퍼: 정렬된 파티클 ID
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), FMath::Max(ParticleCount, 1));
        Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
        OutResources.ParticleIndicesBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.ParticleIndices"));
        OutResources.ParticleIndicesSRV = GraphBuilder.CreateSRV(OutResources.ParticleIndicesBuffer);
        OutResources.ParticleIndicesUAV = GraphBuilder.CreateUAV(OutResources.ParticleIndicesBuffer);
    }
}

bool FSpatialHashBuilder::BuildSpatialHash(
    FRDGBuilder& GraphBuilder,
    FRDGBufferSRVRef RenderParticlesSRV,
    int32 ParticleCount,
    float CellSize,
    FSpatialHashGPUResources& OutResources)
{
    // 렌더링용: USE_PHYSICS_PARTICLE = false
    return BuildSpatialHashInternal(GraphBuilder, RenderParticlesSRV, ParticleCount, CellSize, false, OutResources);
}

bool FSpatialHashBuilder::BuildSpatialHashForPhysics(
    FRDGBuilder& GraphBuilder,
    FRDGBufferSRVRef PhysicsParticlesSRV,
    int32 ParticleCount,
    float CellSize,
    FSpatialHashGPUResources& OutResources)
{
    // 물리용: USE_PHYSICS_PARTICLE = true
    return BuildSpatialHashInternal(GraphBuilder, PhysicsParticlesSRV, ParticleCount, CellSize, true, OutResources);
}

bool FSpatialHashBuilder::BuildSpatialHashInternal(
    FRDGBuilder& GraphBuilder,
    FRDGBufferSRVRef ParticlesSRV,
    int32 ParticleCount,
    float CellSize,
    bool bUsePhysicsParticle,
    FSpatialHashGPUResources& OutResources)
{
    // 입력 유효성 검사
    if (ParticleCount <= 0 || !ParticlesSRV || CellSize <= 0.0f)
    {
        OutResources = FSpatialHashGPUResources();
        return false;
    }

    // 쉐이더 맵 조회
    FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    if (!ShaderMap)
    {
        UE_LOG(LogTemp, Warning, TEXT("FSpatialHashBuilder::BuildSpatialHash - ShaderMap is null"));
        return false;
    }

    // 쉐이더 Permutation 설정
    FCountParticlesCS::FPermutationDomain CountPermutation;
    CountPermutation.Set<FUsePhysicsParticleDim>(bUsePhysicsParticle);

    UE_LOG(LogTemp, Warning, TEXT("FSpatialHashBuilder: bUsePhysicsParticle=%d, ParticleCount=%d, CellSize=%.2f"),
        bUsePhysicsParticle ? 1 : 0, ParticleCount, CellSize);

    // 쉐이더 유효성 검사
    TShaderMapRef<FClearCellCountsCS> ClearCellCountsShader(ShaderMap);
    TShaderMapRef<FCountParticlesCS> CountParticlesShader(ShaderMap, CountPermutation);
    TShaderMapRef<FPrefixSumCS> PrefixSumShader(ShaderMap);
    TShaderMapRef<FClearWriteOffsetsCS> ClearWriteOffsetsShader(ShaderMap);
    TShaderMapRef<FScatterParticlesCS> ScatterParticlesShader(ShaderMap);

    if (!ClearCellCountsShader.IsValid() || !CountParticlesShader.IsValid() ||
        !PrefixSumShader.IsValid() || !ClearWriteOffsetsShader.IsValid() ||
        !ScatterParticlesShader.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("FSpatialHashBuilder::BuildSpatialHash - One or more shaders not valid"));
        return false;
    }

    RDG_EVENT_SCOPE(GraphBuilder, "SpatialHash::Build (Particles: %d, Physics: %d)", ParticleCount, bUsePhysicsParticle ? 1 : 0);

    // 버퍼 생성
    CreateBuffers(GraphBuilder, ParticleCount, CellSize, OutResources);

    // 임시 버퍼: ParticleCellHashes (각 파티클의 셀 해시)
    FRDGBufferRef ParticleCellHashesBuffer;
    FRDGBufferSRVRef ParticleCellHashesSRV;
    FRDGBufferUAVRef ParticleCellHashesUAV;
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ParticleCount);
        Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
        ParticleCellHashesBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.ParticleCellHashes"));
        ParticleCellHashesSRV = GraphBuilder.CreateSRV(ParticleCellHashesBuffer);
        ParticleCellHashesUAV = GraphBuilder.CreateUAV(ParticleCellHashesBuffer);
    }

    // 임시 버퍼: CellWriteOffsets (Scatter용 atomic 카운터)
    FRDGBufferRef CellWriteOffsetsBuffer;
    FRDGBufferUAVRef CellWriteOffsetsUAV;
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SPATIAL_HASH_SIZE);
        Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::StructuredBuffer;
        CellWriteOffsetsBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("SpatialHash.CellWriteOffsets"));
        CellWriteOffsetsUAV = GraphBuilder.CreateUAV(CellWriteOffsetsBuffer);
    }

    uint32 NumCellGroups = FMath::DivideAndRoundUp(SPATIAL_HASH_SIZE, SPATIAL_HASH_THREAD_GROUP_SIZE);
    uint32 NumParticleGroups = FMath::DivideAndRoundUp((uint32)ParticleCount, SPATIAL_HASH_THREAD_GROUP_SIZE);

    //=========================================================================
    // Pass 0: Clear Cell Counts
    //=========================================================================
    {
        FClearCellCountsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearCellCountsCS::FParameters>();
        PassParameters->CellCounts = OutResources.CellCountsUAV;

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("SpatialHash::ClearCellCounts"),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [ClearCellCountsShader, PassParameters, NumCellGroups](FRHIComputeCommandList& RHICmdList)
            {
                FComputeShaderUtils::Dispatch(RHICmdList, ClearCellCountsShader, *PassParameters, FIntVector(NumCellGroups, 1, 1));
            });
    }

    //=========================================================================
    // Pass 1: Count Particles Per Cell
    // Permutation에 따라 PhysicsParticles 또는 RenderParticles 사용
    //=========================================================================
    {
        FCountParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCountParticlesCS::FParameters>();

        // Permutation에 따라 적절한 버퍼만 바인딩
        if (bUsePhysicsParticle)
        {
            PassParameters->PhysicsParticles = ParticlesSRV;  // FGPUFluidParticle (64 bytes)
            PassParameters->RenderParticles = nullptr;
        }
        else
        {
            PassParameters->PhysicsParticles = nullptr;
            PassParameters->RenderParticles = ParticlesSRV;   // FKawaiiRenderParticle (32 bytes)
        }

        PassParameters->ParticleCount = ParticleCount;
        PassParameters->CellSize = CellSize;
        PassParameters->CellCounts = OutResources.CellCountsUAV;
        PassParameters->ParticleCellHashes = ParticleCellHashesUAV;

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("SpatialHash::CountParticles"),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [CountParticlesShader, PassParameters, NumParticleGroups](FRHIComputeCommandList& RHICmdList)
            {
                FComputeShaderUtils::Dispatch(RHICmdList, CountParticlesShader, *PassParameters, FIntVector(NumParticleGroups, 1, 1));
            });
    }

    //=========================================================================
    // Pass 2: Prefix Sum (Sequential)
    // 단일 스레드로 65536개 셀에 대해 순차 처리
    //=========================================================================
    {
        FPrefixSumCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPrefixSumCS::FParameters>();
        PassParameters->CellCounts = OutResources.CellCountsUAV;
        PassParameters->CellStartIndices = OutResources.CellStartIndicesUAV;

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("SpatialHash::PrefixSum"),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [PrefixSumShader, PassParameters](FRHIComputeCommandList& RHICmdList)
            {
                // 단일 스레드 그룹 디스패치 (numthreads(1,1,1) 쉐이더)
                FComputeShaderUtils::Dispatch(RHICmdList, PrefixSumShader, *PassParameters, FIntVector(1, 1, 1));
            });
    }

    //=========================================================================
    // Pass 2.5: Clear Write Offsets
    //=========================================================================
    {
        FClearWriteOffsetsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearWriteOffsetsCS::FParameters>();
        PassParameters->CellWriteOffsets = CellWriteOffsetsUAV;

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("SpatialHash::ClearWriteOffsets"),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [ClearWriteOffsetsShader, PassParameters, NumCellGroups](FRHIComputeCommandList& RHICmdList)
            {
                FComputeShaderUtils::Dispatch(RHICmdList, ClearWriteOffsetsShader, *PassParameters, FIntVector(NumCellGroups, 1, 1));
            });
    }

    //=========================================================================
    // Pass 3: Scatter Particles
    // SRV 변수명: ParticleCellHashesSRV, CellStartIndicesSRV
    //=========================================================================
    {
        FScatterParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScatterParticlesCS::FParameters>();
        PassParameters->ParticleCount = ParticleCount;
        PassParameters->ParticleCellHashesSRV = ParticleCellHashesSRV;
        PassParameters->CellStartIndicesSRV = OutResources.CellStartIndicesSRV;
        PassParameters->CellWriteOffsets = CellWriteOffsetsUAV;
        PassParameters->ParticleIndices = OutResources.ParticleIndicesUAV;

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("SpatialHash::ScatterParticles"),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [ScatterParticlesShader, PassParameters, NumParticleGroups](FRHIComputeCommandList& RHICmdList)
            {
                FComputeShaderUtils::Dispatch(RHICmdList, ScatterParticlesShader, *PassParameters, FIntVector(NumParticleGroups, 1, 1));
            });
    }

    return true;
}
