# KawaiiFluid SoA (Structure of Arrays) 최적화 진행 문서

## 개요

**목표**: AoS(Array of Structures) → SoA(Structure of Arrays) 전환으로 GPU 메모리 대역폭 60%+ 절감

**현재 상태**: 렌더링 파이프라인 SoA 완료, 물리 시뮬레이션 SoA 미착수

---

## Phase 1: 렌더링 SoA (완료)

### 변경 내용

FKawaiiRenderParticle (32B) 대신 Position 버퍼 (12B)만 사용하여 SDF 평가 시 메모리 대역폭 62% 절감

```
기존 AoS (32 bytes/particle):
┌─────────────────────────────────────────────────┐
│ Position(12) │ Velocity(12) │ Radius(4) │ Pad(4) │
└─────────────────────────────────────────────────┘
  ↓ SDF에서는 Position만 필요 → 20B 낭비

SoA 버퍼 (12 bytes/particle for SDF):
├── PositionBuffer   float3[] (12B × N)  ← SDF 핫패스
├── VelocityBuffer   float3[] (12B × N)  ← 모션블러용 (선택적)
```

### 수정된 파일

#### 기존 작업
| 파일 | 변경 내용 |
|------|----------|
| `Public/Rendering/KawaiiFluidRenderResource.h` | SoA 버퍼 멤버/접근자 추가 |
| `Private/Rendering/KawaiiFluidRenderResource.cpp` | SoA 버퍼 생성/관리 로직 |
| `Shaders/Private/FluidExtractRenderData.usf` | `ExtractRenderDataSoACS` 엔트리포인트 추가 |
| `Public/GPU/GPUFluidSimulatorShaders.h` | `FExtractRenderDataSoACS` 셰이더 클래스 |
| `Private/GPU/GPUFluidSimulatorShaders.cpp` | `AddExtractRenderDataSoAPass` 구현 |
| `Shaders/Private/FluidSDFCommon.ush` | SoA 버전 SDF 함수들 추가 |
| `Public/Rendering/Shaders/FluidRayMarchShaders.h` | SoA 버퍼 파라미터, `FUseSoABuffersDim` 퍼뮤테이션 |
| `Shaders/Private/FluidRayMarching.usf` | `#if USE_SOA_BUFFERS` 조건부 SoA 사용 |
| `Public/Rendering/MetaballRenderingData.h` | `FRayMarchingPipelineData`에 SoA 멤버 추가 |
| `Private/Rendering/Pipeline/KawaiiMetaballRayMarchPipeline.cpp` | SoA 버퍼 등록/바인딩 |
| `Private/Rendering/Shading/KawaiiRayMarchShadingImpl.cpp` | SoA 버퍼 바인딩, 퍼뮤테이션 설정 |

#### 2026-01-07 추가 작업
| 파일 | 변경 내용 |
|------|----------|
| `Shaders/Private/FluidBoundsReduction.usf` | `#if USE_SOA_BUFFERS` SoA 지원 추가 |
| `Shaders/Private/SDFBake.usf` | `#if USE_SOA_BUFFERS` SoA 지원 추가 |
| `Shaders/Private/SDFBakeWithGPUBounds.usf` | `#if USE_SOA_BUFFERS` SoA 지원 추가 |
| `Shaders/Private/FluidRayMarchGBuffer.usf` | `#if USE_SOA_BUFFERS` SoA 지원 추가 |
| `Public/Rendering/Shaders/BoundsReductionShaders.h` | `RenderPositions` 파라미터, `FBoundsReductionUseSoADim` 퍼뮤테이션 |
| `Public/Rendering/Shaders/SDFBakeShaders.h` | `RenderPositions` 파라미터, `FSDFBakeUseSoADim` 퍼뮤테이션 |
| `Public/Rendering/Shaders/FluidRayMarchGBufferShaders.h` | `RenderPositions` 파라미터, `FUseSoABuffersGBufferDim` 퍼뮤테이션 |
| `Public/Rendering/SDFVolumeManager.h` | 모든 함수에 `PositionBufferSRV` 옵션 파라미터 추가 |
| `Private/Rendering/SDFVolumeManager.cpp` | SoA 버퍼 바인딩 및 퍼뮤테이션 설정 |
| `Private/Rendering/Shading/KawaiiRayMarchShadingImpl.cpp` | GBuffer 패스 SoA 지원 추가 |
| `Private/Rendering/KawaiiFluidRenderResource.cpp` | `UpdateParticleData()`에서 SoA Position 버퍼 업로드 (CPU 모드) |
| `Private/Rendering/Pipeline/KawaiiMetaballRayMarchPipeline.cpp` | CPU 모드에서 SoA 버퍼 생성 및 사용 |

### 핵심 코드 변경

#### FluidSDFCommon.ush - SoA 함수 추가
```hlsl
// 기존 AoS 버전
float EvaluateMetaballSDFWithSpatialHash(
    float3 p,
    StructuredBuffer<FKawaiiRenderParticle> RenderParticles, // 32B
    ...
);

// 새로운 SoA 버전 (62% 대역폭 절감)
float EvaluateMetaballSDFWithSpatialHash_SoA(
    float3 p,
    StructuredBuffer<float3> RenderPositions, // 12B
    ...
);
```

#### FluidRayMarching.usf - 조건부 SoA 사용
```hlsl
#if USE_SOA_BUFFERS
    float hashSdf = EvaluateMetaballSDFWithSpatialHash_SoA(p, RenderPositions, ...);
    normal = CalculateNormalAnalyticWithSpatialHash_SoA(p, sdf, RenderPositions, ...);
#else
    float hashSdf = EvaluateMetaballSDFWithSpatialHash(p, RenderParticles, ...);
    normal = CalculateNormalAnalyticWithSpatialHash(p, sdf, RenderParticles, ...);
#endif
```

### 예상 성능 이득

| 시나리오 | AoS | SoA | 절감 |
|---------|-----|-----|------|
| SDF Ray March (10K particles) | 32B × K reads | 12B × K reads | **62%** |
| 캐시라인 효율 (128B) | 4 particles | 10 particles | **150%** |

---

## Phase 2: 물리 시뮬레이션 SoA (미착수)

### 목표

FGPUFluidParticle (64B) → 분리된 SoA 버퍼로 전환

```
현재 FGPUFluidParticle (64 bytes):
┌─────────────────────────────────────────────────────────────────┐
│ Position(12) │ Mass(4) │ PredPos(12) │ Density(4) │ Velocity(12) │ Lambda(4) │ ID(4) │ Cluster(4) │ Flags(4) │ Pad(4) │
└─────────────────────────────────────────────────────────────────┘

목표 SoA 버퍼:
├── PositionBuffer         float3[] (12B × N)  ← 핫패스
├── PredictedPositionBuffer float3[] (12B × N)  ← 핫패스
├── VelocityBuffer         float3[] (12B × N)  ← 핫패스
├── LambdaBuffer           float[]  (4B × N)   ← 핫패스
├── DensityBuffer          float[]  (4B × N)
├── MassBuffer             float[]  (4B × N)
├── FlagsBuffer            uint[]   (4B × N)
├── ParticleIDBuffer       int[]    (4B × N)   ← 콜드패스
└── ClusterIDBuffer        int[]    (4B × N)   ← 콜드패스
```

### 수정 예정 파일

#### 인프라 (Phase 2-1)
| 파일 | 변경 내용 |
|------|----------|
| `Public/GPU/GPUFluidParticle.h` | `FGPUFluidParticleSoA` 버퍼 구조체 추가 |
| `Private/GPU/GPUFluidSimulator.cpp` | SoA 버퍼 생성/해제 (ResizeBuffers) |
| `Shaders/Private/FluidGPUPhysics.ush` | SoA 접근 매크로 정의 |

#### 핵심 셰이더 (Phase 2-2)
| 파일 | 변경 내용 | 예상 절감 |
|------|----------|----------|
| `FluidPredictPositions.usf` | Position, Velocity, PredPos SoA | 50% |
| `FluidComputeDensity.usf` | PredPos SoA (27셀 이웃 접근) | **75%** |
| `FluidSolvePressure.usf` | PredPos, Lambda SoA | **69%** |
| `FluidApplyViscosity.usf` | PredPos, Velocity SoA | 56% |
| `FluidFinalizePositions.usf` | Position, PredPos, Velocity SoA | 50% |

#### 충돌 셰이더 (Phase 2-3)
| 파일 | 변경 내용 |
|------|----------|
| `FluidBoundsCollision.usf` | PredPos, Velocity SoA |
| `FluidDistanceFieldCollision.usf` | PredPos, Velocity SoA |
| `FluidPrimitiveCollision.usf` | PredPos, Velocity SoA |

#### 추출 셰이더 (Phase 2-4)
| 파일 | 변경 내용 |
|------|----------|
| `FluidExtractRenderData.usf` | Physics SoA → Render SoA 직접 복사 |
| `FluidExtractRenderDataWithBounds.usf` | 동일 |

### 예상 총 성능 이득

| 셰이더 | AoS (B/파티클) | SoA (B/파티클) | 절감 |
|--------|---------------|---------------|------|
| ComputeDensity (27셀) | 1,728 | 648 | **63%** |
| SolvePressure (27셀) | 1,728 | 540 | **69%** |
| ApplyViscosity (27셀) | 1,728 | 756 | **56%** |

**10K 파티클 기준**: 52MB/substep → 20MB/substep (**61% 절감**)

---

---

## TODO: 파이프라인 통합

### 문제점

현재 GPU 시뮬레이션과 CPU 시뮬레이션이 렌더링 파이프라인에서 다르게 동작함:

```
GPU 시뮬레이션 모드:
├── UpdateFromGPUBuffer() 호출
├── AddExtractRenderDataSoAPass()로 GPU에서 SoA 추출
├── PooledPositionBuffer를 Pipeline에서 직접 RegisterExternalBuffer()
└── SoA 버퍼 재사용 (GPU→GPU, 업로드 없음)

CPU 시뮬레이션 모드:
├── UpdateParticleData() 호출
├── CPU에서 GPU로 AoS 업로드 (LockBuffer/Memcpy)
├── CPU에서 GPU로 SoA 업로드 (LockBuffer/Memcpy) ← 별도로 함
├── Pipeline에서 GetCachedParticles()로 다시 CPU에서 읽음 ← 비효율
├── 새 RDG 버퍼 만들어서 또 업로드 ← 중복!
└── SoA도 새로 만들어서 업로드 ← 중복!
```

### 문제 요약

1. **CPU 모드 이중 업로드**: `UpdateParticleData()`에서 GPU에 업로드했는데, Pipeline에서 CPU 캐시 읽어서 또 업로드
2. **비일관된 버퍼 소스**: GPU 모드는 `PooledBuffer` 사용, CPU 모드는 매 프레임 새 버퍼 생성
3. **불필요한 코드 경로**: 두 모드가 같은 결과인데 다른 코드 경로

### 해결 방향

CPU 모드도 GPU 모드처럼 `PooledBuffer` 직접 사용하도록 통합:

```
통합 렌더링 파이프라인:
├── RenderResource->GetPooledPositionBuffer() 사용 (GPU/CPU 공통)
├── RegisterExternalBuffer()로 RDG에 등록
└── 새 버퍼 생성/업로드 불필요
```

### 수정 필요 파일

| 파일 | 변경 내용 |
|------|----------|
| `KawaiiMetaballRayMarchPipeline.cpp` | CPU 모드에서 `PooledBuffer` 직접 사용 |
| `KawaiiFluidRenderResource.cpp` | `UpdateParticleData()`의 GPU 업로드 로직 정리 |

### 우선순위

- **낮음**: 현재도 동작함. 성능 최적화 목적으로 나중에 진행

---

## 주의사항

### Double Buffering 필요 (SolvePressure)
```hlsl
// SolvePressure에서 PredPosition 읽기/쓰기 동시 필요
// 해결책: 별도 출력 버퍼 또는 반복 후 스왑
StructuredBuffer<float3> PredPositions_In;
RWStructuredBuffer<float3> PredPositions_Out;
```

### float3 정렬
- 현재: float3 (12B) 사용
- GPU에 따라 float4 (16B) 패딩이 더 빠를 수 있음
- 프로파일링 후 결정

### 버퍼 동기화
- RDG 의존성 그래프로 순서 보장
- 다중 버퍼 접근 시 리소스 상태 전환 주의

---

## 테스트 체크리스트

### Phase 1 (렌더링 SoA) - 현재
- [ ] 컴파일 성공
- [ ] 10K 파티클 Dam Break 시뮬레이션 정상 렌더링
- [ ] SoA 모드 vs AoS 모드 시각적 동일성 확인
- [ ] GPU 프레임타임 비교 (RenderDoc/ProfileGPU)

### Phase 2 (물리 SoA) - 예정
- [ ] 1000프레임 시뮬레이션 위치 드리프트 < 1cm
- [ ] 물리 정확도 유지 확인
- [ ] 전체 substep 대역폭 측정

---

## 참고 자료

- Inigo Quilez SDF: https://iquilezles.org/articles/smin/
- UE5 RDG: Engine/Source/Runtime/RenderCore/Public/RenderGraphBuilder.h
- GPU 메모리 대역폭 최적화: 캐시라인(128B) 기준 접근 패턴 설계

---

*마지막 업데이트: 2026-01-07*
*작성자: Claude Code*
