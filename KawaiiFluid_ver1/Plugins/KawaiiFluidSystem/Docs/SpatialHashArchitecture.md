# Spatial Hash System Architecture

## Overview

GPU 기반 Spatial Hash 시스템. 파티클 이웃 탐색을 O(N^2)에서 O(k)로 최적화.

```
SPATIAL_HASH_SIZE = 65536 (2^16)
MAX_PARTICLES_PER_CELL = 16
```

---

## 두 가지 버전

### 1. Simple Version (GPU 시뮬레이션용)

**용도**: 물리 시뮬레이션 (Density, Pressure, Viscosity 계산)

**특징**:
- 고정 레이아웃: `ParticleIndices[Hash * MAX_PARTICLES_PER_CELL + Slot]`
- 셀당 최대 16개 파티클 제한
- 단일 패스 빌드 (Clear → Build)
- Prefix Sum 불필요

**버퍼 구조**:
```
CellCounts[65536]           : uint (셀당 파티클 개수)
ParticleIndices[65536 * 16] : uint (고정 오프셋 = Hash * 16)
```

**셰이더**:
- `ClearCellDataCS` - CellCounts를 0으로 초기화
- `BuildSpatialHashSimpleCS` - 파티클을 해시 그리드에 삽입

**C++ 클래스**:
- `FClearCellDataCS`
- `FBuildSpatialHashSimpleCS`
- `FSpatialHashGPUResources` (리소스 구조체)
- `FSpatialHashBuilder::CreateAndBuildHash()` (빌드 함수)

---

### 2. Multipass Version (렌더링용)

**용도**: Ray Marching SDF 렌더링 (Hybrid 모드에서 정밀 평가)

**특징**:
- 동적 레이아웃: Prefix Sum으로 시작 인덱스 계산
- 셀당 파티클 제한 없음
- 6개 패스 빌드
- 정렬된 파티클 인덱스 (smin 결정론적 결과)

**버퍼 구조**:
```
CellData[65536]             : uint2 {startIndex, count}
ParticleIndices[N]          : uint (동적 크기 = 파티클 수)
CellCounters[65536]         : uint (임시 - 카운트/스캐터용)
ParticleCellHashes[N]       : uint (임시 - 파티클별 해시)
PrefixSumBuffer[65536]      : uint (임시 - 누적합)
```

**빌드 패스**:
```
Pass 1: ClearCellDataMultipassCS  - CellData, CellCounters 초기화
Pass 2: CountParticlesPerCellCS   - 셀별 파티클 카운트
Pass 3: PrefixSumCS               - 시작 인덱스 계산 (단일 스레드)
Pass 4: Clear CellCounters        - 스캐터 전 카운터 리셋
Pass 5: ScatterParticlesCS        - 파티클을 정렬된 위치에 저장
Pass 6: FinalizeCellDataCS        - CellData에 {startIndex, count} 기록
Pass 7: SortBucketParticlesCS     - 셀 내 파티클 정렬 (선택적)
```

**C++ 클래스**:
- `FClearCellDataMultipassCS`
- `FCountParticlesPerCellCS`
- `FPrefixSumCS`
- `FScatterParticlesCS`
- `FFinalizeCellDataCS`
- `FSortBucketParticlesCS`
- `FSpatialHashMultipassResources` (리소스 구조체)
- `FSpatialHashBuilder::CreateAndBuildHashMultipass()` (빌드 함수)

---

## 파일 구조

### 셰이더 파일
```
Shaders/Private/
├── FluidSpatialHash.ush           # 해시 함수 (WorldToCell, HashCell)
├── FluidSpatialHashBuild.usf      # 빌드 셰이더 (Simple + Multipass)
└── FluidExtractRenderPositions.usf # FKawaiiRenderParticle → float3 추출
```

### C++ 파일
```
Source/KawaiiFluidRuntime/
├── Public/Rendering/Shaders/
│   ├── FluidSpatialHashShaders.h      # 셰이더 클래스 선언
│   └── ExtractRenderPositionsShaders.h
└── Private/Rendering/Shaders/
    ├── FluidSpatialHashShaders.cpp    # 셰이더 구현
    └── ExtractRenderPositionsShaders.cpp
```

---

## 해시 함수

```hlsl
// FluidSpatialHash.ush

int3 WorldToCell(float3 WorldPos, float CellSize)
{
    return int3(floor(WorldPos / CellSize));
}

uint HashCell(int3 CellCoord)
{
    const uint p1 = 73856093u;
    const uint p2 = 19349663u;
    const uint p3 = 83492791u;

    uint h = (uint(CellCoord.x) * p1) ^ (uint(CellCoord.y) * p2) ^ (uint(CellCoord.z) * p3);
    return h & (SPATIAL_HASH_SIZE - 1);  // 65535 마스크
}
```

---

## 사용 흐름

### GPU 시뮬레이션 (Simple)
```cpp
// GPUFluidSimulator.cpp
FSpatialHashGPUResources HashResources;
FSpatialHashBuilder::CreateAndBuildHash(
    GraphBuilder,
    PositionsSRV,      // StructuredBuffer<float3>
    ParticleCount,
    ParticleRadius,
    CellSize,
    HashResources
);

// 이후 Density/Pressure/Viscosity 패스에서 사용
// HashResources.CellCountsSRV, HashResources.ParticleIndicesSRV
```

### 렌더링 (Multipass)
```cpp
// KawaiiMetaballRayMarchPipeline.cpp

// 1. FKawaiiRenderParticle에서 float3 추출
FExtractRenderPositionsPassBuilder::AddExtractPositionsPass(
    GraphBuilder,
    ParticleBufferSRV,  // StructuredBuffer<FKawaiiRenderParticle>
    PositionUAV,        // RWStructuredBuffer<float3>
    ParticleCount
);

// 2. Multipass Hash 빌드
FSpatialHashMultipassResources HashResources;
FSpatialHashBuilder::CreateAndBuildHashMultipass(
    GraphBuilder,
    PositionSRV,        // StructuredBuffer<float3>
    ParticleCount,
    CellSize,
    HashResources
);

// 3. Ray Marching 셰이더에서 사용
// HashResources.CellDataSRV, HashResources.ParticleIndicesSRV
```

---

## 알려진 이슈

### 1. PrefixSum 단일 스레드
현재 PrefixSumCS가 단일 스레드로 65536번 루프를 돈다.
- 원인: 병렬 Prefix Sum의 cross-block 누적 버그
- 영향: Multipass 빌드 성능 저하
- TODO: 3-pass 병렬 Prefix Sum 구현

### 2. CPU 버퍼 초기화 (CreateAndBuildHash)
Simple 버전에서 매 프레임 CPU에서 버퍼를 0으로 초기화하고 업로드.
- 65536 * 4 = 256KB (CellCounts)
- 65536 * 16 * 4 = 4MB (ParticleIndices)
- TODO: GPU Clear 패스 사용으로 변경

### 3. 렌더링 중간중간 파티클 누락
Spatial Hash 사용 시 일부 파티클이 렌더링 안 되는 현상.
- 원인 분석 필요
- 가능성: Prefix Sum 버그, 해시 충돌, 버퍼 동기화

---

## 상수 정의

```cpp
// FluidSpatialHashShaders.h
static constexpr uint32 SPATIAL_HASH_SIZE = 65536;
static constexpr uint32 MAX_PARTICLES_PER_CELL = 16;
static constexpr uint32 SPATIAL_HASH_THREAD_GROUP_SIZE = 256;
```

```hlsl
// FluidSpatialHash.ush
#define SPATIAL_HASH_SIZE 65536
#define MAX_PARTICLES_PER_CELL 16
```

---

## Permutation Defines

| Define | 값 | 용도 |
|--------|---|------|
| `USE_MULTIPASS_BUILD` | 1 | Multipass 셰이더 컴파일 활성화 |
| `SPATIAL_HASH_SIZE` | 65536 | 해시 테이블 크기 |
| `MAX_PARTICLES_PER_CELL` | 16 | Simple 버전 셀당 최대 파티클 |
