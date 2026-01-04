// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ShaderPermutation.h"
#include "RenderGraphDefinitions.h"

//=============================================================================
// Spatial Hash Constants (must match shader defines)
//=============================================================================

static constexpr uint32 SPATIAL_HASH_SIZE = 65536;          // 2^16 cells
static constexpr uint32 SPATIAL_HASH_THREAD_GROUP_SIZE = 256;

//=============================================================================
// Shader Permutation: 물리 시뮬레이션용 vs 렌더링용
//=============================================================================

class FUsePhysicsParticleDim : SHADER_PERMUTATION_BOOL("USE_PHYSICS_PARTICLE");

//=============================================================================
// Pass 0: Clear Cell Counts
//=============================================================================

class FClearCellCountsCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FClearCellCountsCS);
    SHADER_USE_PARAMETER_STRUCT(FClearCellCountsCS, FGlobalShader);

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
    }
};

//=============================================================================
// Pass 1: Count Particles Per Cell
// Permutation으로 물리용(FGPUFluidParticle) / 렌더링용(FKawaiiRenderParticle) 분기
//=============================================================================

class FCountParticlesCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FCountParticlesCS);
    SHADER_USE_PARAMETER_STRUCT(FCountParticlesCS, FGlobalShader);

    // Permutation 정의
    using FPermutationDomain = TShaderPermutationDomain<FUsePhysicsParticleDim>;

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        // Input - Permutation에 따라 하나만 바인딩됨
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUFluidParticle>, PhysicsParticles)   // 물리용 (64 bytes)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FKawaiiRenderParticle>, RenderParticles) // 렌더링용 (32 bytes)
        SHADER_PARAMETER(int32, ParticleCount)
        SHADER_PARAMETER(float, CellSize)

        // Output
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounts)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleCellHashes)
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

        // Permutation에서 USE_PHYSICS_PARTICLE 값 가져와서 설정
        FPermutationDomain PermutationVector(Parameters.PermutationId);
        const bool bUsePhysicsParticle = PermutationVector.Get<FUsePhysicsParticleDim>();
        OutEnvironment.SetDefine(TEXT("USE_PHYSICS_PARTICLE"), bUsePhysicsParticle ? 1 : 0);
    }
};

//=============================================================================
// Pass 2: Prefix Sum (Sequential version)
//=============================================================================

class FPrefixSumCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FPrefixSumCS);
    SHADER_USE_PARAMETER_STRUCT(FPrefixSumCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellCounts)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellStartIndices)
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
    }
};

//=============================================================================
// Pass 2.5: Clear Write Offsets
//=============================================================================

class FClearWriteOffsetsCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FClearWriteOffsetsCS);
    SHADER_USE_PARAMETER_STRUCT(FClearWriteOffsetsCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellWriteOffsets)
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
    }
};

//=============================================================================
// Pass 3: Scatter Particles
// SRV 변수명이 HLSL과 일치해야 함 (ParticleCellHashesSRV, CellStartIndicesSRV)
//=============================================================================

class FScatterParticlesCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FScatterParticlesCS);
    SHADER_USE_PARAMETER_STRUCT(FScatterParticlesCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        // Input
        SHADER_PARAMETER(int32, ParticleCount)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleCellHashesSRV)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellStartIndicesSRV)

        // Output
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CellWriteOffsets)
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
    }
};

//=============================================================================
// Pass 4: Sort Particles Within Cells
// 셀 내 파티클을 인덱스 순서로 정렬 (떨림 방지)
//=============================================================================

class FSortCellParticlesCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FSortCellParticlesCS);
    SHADER_USE_PARAMETER_STRUCT(FSortCellParticlesCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCountsSRV)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellStartIndicesSRV)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, ParticleIndicesRW)
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
    }
};

//=============================================================================
// Spatial Hash GPU Resources (Multi-pass version)
//=============================================================================

/**
 * GPU resources for spatial hash acceleration structure
 *
 * 버퍼 구조:
 *   CellCounts[65536]       : 셀별 파티클 개수
 *   CellStartIndices[65536] : 셀별 시작 인덱스 (Prefix Sum 결과)
 *   ParticleIndices[N]      : 셀 순서로 정렬된 파티클 ID
 */
struct FSpatialHashGPUResources
{
    // 셀별 파티클 개수
    FRDGBufferRef CellCountsBuffer = nullptr;
    FRDGBufferSRVRef CellCountsSRV = nullptr;
    FRDGBufferUAVRef CellCountsUAV = nullptr;

    // 셀별 시작 인덱스 (Prefix Sum 결과)
    FRDGBufferRef CellStartIndicesBuffer = nullptr;
    FRDGBufferSRVRef CellStartIndicesSRV = nullptr;
    FRDGBufferUAVRef CellStartIndicesUAV = nullptr;

    // 정렬된 파티클 인덱스
    FRDGBufferRef ParticleIndicesBuffer = nullptr;
    FRDGBufferSRVRef ParticleIndicesSRV = nullptr;
    FRDGBufferUAVRef ParticleIndicesUAV = nullptr;

    // Configuration
    float CellSize = 0.0f;
    int32 ParticleCount = 0;

    bool IsValid() const
    {
        return CellCountsBuffer != nullptr
            && CellStartIndicesBuffer != nullptr
            && ParticleIndicesBuffer != nullptr;
    }
};

//=============================================================================
// Spatial Hash Builder (Multi-pass version)
//=============================================================================

/**
 * Utility class for building spatial hash on GPU using multi-pass algorithm
 *
 * 빌드 순서:
 *   Pass 0: Clear - CellCounts 초기화
 *   Pass 1: Count - 각 셀의 파티클 개수 카운트
 *   Pass 2: PrefixSum - 누적합으로 시작 인덱스 계산
 *   Pass 2.5: ClearWriteOffsets - Scatter용 오프셋 초기화
 *   Pass 3: Scatter - 파티클을 정렬된 위치에 저장
 */
class KAWAIIFLUIDRUNTIME_API FSpatialHashBuilder
{
public:
    /**
     * Build spatial hash for RENDERING (FKawaiiRenderParticle, 32 bytes)
     *
     * @param GraphBuilder RDG builder
     * @param RenderParticlesSRV 렌더 파티클 버퍼 (FKawaiiRenderParticle)
     * @param ParticleCount 파티클 개수
     * @param CellSize 셀 크기 (월드 유닛)
     * @param OutResources 출력 리소스 구조체
     * @return 성공 여부
     */
    static bool BuildSpatialHash(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef RenderParticlesSRV,
        int32 ParticleCount,
        float CellSize,
        FSpatialHashGPUResources& OutResources);

    /**
     * Build spatial hash for PHYSICS (FGPUFluidParticle, 64 bytes)
     *
     * @param GraphBuilder RDG builder
     * @param PhysicsParticlesSRV 물리 파티클 버퍼 (FGPUFluidParticle)
     * @param ParticleCount 파티클 개수
     * @param CellSize 셀 크기 (월드 유닛)
     * @param OutResources 출력 리소스 구조체
     * @return 성공 여부
     */
    static bool BuildSpatialHashForPhysics(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef PhysicsParticlesSRV,
        int32 ParticleCount,
        float CellSize,
        FSpatialHashGPUResources& OutResources);

    /**
     * 기존 API 호환성 함수 (BuildSpatialHash와 동일)
     * @deprecated Use BuildSpatialHash instead
     */
    static bool CreateAndBuildHash(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef RenderParticlesSRV,
        int32 ParticleCount,
        float ParticleRadius,  // 사용하지 않음, 호환성 유지용
        float CellSize,
        FSpatialHashGPUResources& OutResources)
    {
        // ParticleRadius는 무시 (새 API에서는 사용하지 않음)
        return BuildSpatialHash(GraphBuilder, RenderParticlesSRV, ParticleCount, CellSize, OutResources);
    }

private:
    // 내부 버퍼 생성
    static void CreateBuffers(
        FRDGBuilder& GraphBuilder,
        int32 ParticleCount,
        float CellSize,
        FSpatialHashGPUResources& OutResources);

    // 내부 구현 (Permutation 분기)
    static bool BuildSpatialHashInternal(
        FRDGBuilder& GraphBuilder,
        FRDGBufferSRVRef ParticlesSRV,
        int32 ParticleCount,
        float CellSize,
        bool bUsePhysicsParticle,
        FSpatialHashGPUResources& OutResources);
};
