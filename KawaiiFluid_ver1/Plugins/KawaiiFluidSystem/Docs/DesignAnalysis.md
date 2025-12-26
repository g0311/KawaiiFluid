# KawaiiFluid 시스템 설계 분석

## 1. 아키텍처 개요

### 1.1 설계 철학
새 시스템은 **SOLID 원칙**을 따르는 Component-Subsystem 아키텍처입니다:

```
┌─────────────────────────────────────────────────────────────────┐
│                    UKawaiiFluidSimulatorSubsystem               │
│                      (Orchestration Layer)                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ - AllComponents[]     - GlobalColliders[]               │   │
│  │ - ContextCache        - SharedSpatialHash               │   │
│  │ - MergedParticleBuffer                                   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│  Component A    │ │  Component B    │ │  Component C    │
│  (Data Owner)   │ │  (Data Owner)   │ │  (Data Owner)   │
│ ┌─────────────┐ │ │ ┌─────────────┐ │ │ ┌─────────────┐ │
│ │ Particles[] │ │ │ │ Particles[] │ │ │ │ Particles[] │ │
│ │ Colliders[] │ │ │ │ Colliders[] │ │ │ │ Colliders[] │ │
│ │ SpatialHash │ │ │ │ SpatialHash │ │ │ │ SpatialHash │ │
│ │ Preset Ref  │ │ │ │ Preset Ref  │ │ │ │ Preset Ref  │ │
│ └─────────────┘ │ │ └─────────────┘ │ │ └─────────────┘ │
└─────────────────┘ └─────────────────┘ └─────────────────┘
              │               │               │
              └───────────────┼───────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                 UKawaiiFluidSimulationContext                   │
│                    (Stateless Logic Layer)                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ - DensityConstraint (XPBD)                              │   │
│  │ - ViscositySolver (XSPH)                                │   │
│  │ - AdhesionSolver                                        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│  Simulate(Particles, Preset, Params, SpatialHash, DeltaTime)   │
│           ↓ Pure Function (No Side Effects)                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                  UKawaiiFluidPresetDataAsset                    │
│                     (Configuration Layer)                        │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ RestDensity, Compliance, SmoothingRadius, Viscosity...  │   │
│  │ ContextClass: TSubclassOf<UKawaiiFluidSimulationContext>│   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 SOLID 원칙 적용

| 원칙 | 적용 |
|------|------|
| **S**ingle Responsibility | Component=데이터 소유, Context=시뮬레이션 로직, Subsystem=오케스트레이션 |
| **O**pen/Closed | Context 상속으로 새 유체 타입 추가 (Lava, Sand 등) |
| **L**iskov Substitution | 모든 Context 서브클래스가 동일 인터페이스 |
| **I**nterface Segregation | IKawaiiFluidRenderable로 렌더링 분리 |
| **D**ependency Inversion | Component는 Preset(추상)에 의존, 구체 파라미터에 직접 의존 X |

---

## 2. 클래스별 책임 분석

### 2.1 UKawaiiFluidSimulatorSubsystem

**역할**: 오케스트레이터 (지휘자)

**책임**:
1. 모든 SimulationComponent 등록/해제 관리
2. 글로벌 Collider/InteractionComponent 관리
3. 배치 시뮬레이션 오케스트레이션
4. Context 캐싱 (Preset별 재사용)
5. 쿼리 API 제공

**핵심 데이터 구조**:
```cpp
TArray<UKawaiiFluidSimulationComponent*> AllComponents;  // 모든 컴포넌트
TArray<UFluidCollider*> GlobalColliders;                  // 글로벌 콜라이더
TMap<TSubclassOf<Context>, Context*> ContextCache;        // Context 캐시
TSharedPtr<FSpatialHash> SharedSpatialHash;               // 배치용 공유 해시
TArray<FFluidParticle> MergedParticleBuffer;              // 배치용 병합 버퍼
TArray<FKawaiiFluidBatchInfo> BatchInfos;                 // 분할 정보
```

**시뮬레이션 모드**:
```cpp
// 1. Independent Mode (bIndependentSimulation = true)
//    - 각 Component가 독립 시뮬레이션
//    - 자체 SpatialHash 사용
//    - 다른 Component 파티클과 상호작용 X
SimulateIndependentComponents(DeltaTime);

// 2. Batched Mode (bIndependentSimulation = false)
//    - 같은 Preset 가진 Component들 병합
//    - 공유 SpatialHash 사용
//    - 파티클 간 상호작용 O
SimulateBatchedComponents(DeltaTime);
```

### 2.2 UKawaiiFluidSimulationComponent

**역할**: 데이터 소유자 + API 제공자

**책임**:
1. 파티클 배열 소유 및 라이프사이클 관리
2. 로컬 Collider/InteractionComponent 등록
3. 렌더링 리소스 관리 (IKawaiiFluidRenderable 구현)
4. Blueprint/Gameplay API 제공
5. 이벤트 델리게이트 (OnParticleHit)

**핵심 데이터 구조**:
```cpp
TArray<FFluidParticle> Particles;           // 파티클 배열 (핵심 데이터)
TObjectPtr<UKawaiiFluidPresetDataAsset> Preset;  // 설정 참조
TArray<UFluidCollider*> Colliders;          // 로컬 콜라이더
TArray<UFluidInteractionComponent*> InteractionComponents;
TSharedPtr<FSpatialHash> SpatialHash;       // 독립 모드용
TSharedPtr<FKawaiiFluidRenderResource> RenderResource;  // GPU 리소스
```

**API 카테고리**:
```cpp
// 스폰 API
SpawnParticles(Location, Count, Radius);
SpawnParticle(Position, Velocity);

// 힘 적용 API
ApplyExternalForce(Force);
ApplyForceToParticle(Index, Force);

// 쿼리 API
GetParticlesInRadius(Location, Radius);
GetParticlesInBox(Center, Extent);
GetParticleInfo(Index, OutPos, OutVel, OutDensity);

// 설정 API
RegisterCollider(Collider);
SetContinuousSpawnEnabled(bEnabled);
ClearAllParticles();
```

### 2.3 UKawaiiFluidSimulationContext

**역할**: Stateless 시뮬레이션 로직

**핵심 설계 원칙**:
- **순수 함수**: 모든 상태는 파라미터로 전달, 내부 상태 없음
- **재사용 가능**: 여러 Component가 같은 Context 공유
- **확장 가능**: 상속으로 커스텀 유체 동작 구현

**시뮬레이션 파이프라인**:
```cpp
void Simulate(Particles, Preset, Params, SpatialHash, DeltaTime, AccumulatedTime)
{
    // Accumulator 기반 Fixed Timestep
    AccumulatedTime += DeltaTime;

    // Collider 형상 캐싱 (프레임당 1회)
    CacheColliderShapes(Colliders);

    // 본 추적 파티클 위치 업데이트
    UpdateAttachedParticlePositions(Particles, InteractionComponents);

    // Substep 루프
    while (AccumulatedTime >= SubstepDT)
    {
        SimulateSubstep(Particles, Preset, Params, SpatialHash, SubstepDT);
        AccumulatedTime -= SubstepDT;
    }
}
```

**Substep 파이프라인**:
```
┌─────────────────────────────────────────────────────────────┐
│ 1. PredictPositions                                          │
│    - 중력 + 외부 힘 적용                                     │
│    - PredictedPosition = Position + Velocity * dt            │
│    - 부착된 파티클: 접선 방향 중력만 적용 (미끄러짐 효과)    │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. UpdateNeighbors                                           │
│    - SpatialHash 재구축 (순차 - HashMap 쓰기)                │
│    - 각 파티클 이웃 캐싱 (병렬 - 읽기만)                     │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. SolveDensityConstraints (XPBD)                            │
│    - Poly6 커널로 밀도 계산                                  │
│    - Lambda 계산: λ = -C / (∑|∇C|² + ε)                      │
│    - Spiky 그래디언트로 위치 보정: Δp = λ∇W                  │
│    - SIMD 최적화 (4개씩 병렬 처리)                           │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. HandleCollisions                                          │
│    - 등록된 FluidCollider들과 충돌 처리                      │
│    - MeshCollider, SphereCollider, BoxCollider 등            │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. HandleWorldCollision                                      │
│    - Cell 기반 Broad-Phase (OverlapBlockingTestByChannel)    │
│    - Narrow-Phase: SweepSingleByChannel (병렬)               │
│    - 충돌 이벤트 발생 (AsyncTask로 GameThread에서 실행)      │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 6. FinalizePositions                                         │
│    - Velocity = (PredictedPosition - Position) / dt          │
│    - Position = PredictedPosition                            │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 7. ApplyViscosity (XSPH)                                     │
│    - v_new = v + c * Σ(v_j - v_i) * W(r, h)                  │
│    - 이웃 속도 평균화로 점성 효과                            │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 8. ApplyAdhesion                                             │
│    - 표면 접착력 적용                                        │
│    - 임계값 이상 힘 받으면 분리                              │
└──────────────────────────────────────────────────────────────┘
```

### 2.4 UKawaiiFluidPresetDataAsset

**역할**: 설정 저장소

**파라미터 카테고리**:
```cpp
// 물리 파라미터
float RestDensity = 1200.0f;      // 목표 밀도 (kg/m³)
float ParticleMass = 1.0f;         // 파티클 질량
float SmoothingRadius = 20.0f;     // 커널 반경 (cm)
float Compliance = 0.01f;          // XPBD 유연성 (낮을수록 뻣뻣)
FVector Gravity = (0, 0, -980);    // 중력

// 점성 파라미터
float ViscosityCoefficient = 0.5f; // XSPH 점성 (0=물, 0.5=슬라임, 0.8=꿀)

// 접착 파라미터
float AdhesionStrength = 0.5f;     // 접착력
float AdhesionRadius = 25.0f;      // 접착 반경
float DetachThreshold = 500.0f;    // 분리 임계값

// 렌더링 파라미터
float ParticleRadius = 5.0f;       // 렌더링 반경
FLinearColor Color;                // 유체 색상

// 확장성
TSubclassOf<UKawaiiFluidSimulationContext> ContextClass;  // 커스텀 Context
```

---

## 3. 데이터 흐름 분석

### 3.1 시뮬레이션 데이터 흐름

```
┌─────────────────────────────────────────────────────────────────┐
│                         매 프레임                                │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Subsystem::Tick(DeltaTime)                                       │
│   ├── EventCountThisFrame.store(0)  // 이벤트 카운터 리셋        │
│   │                                                              │
│   ├── SimulateIndependentComponents(DeltaTime)                   │
│   │     └── 각 Independent Component에 대해:                     │
│   │           Context->Simulate(                                 │
│   │               Component->Particles,    // In/Out             │
│   │               Component->Preset,       // Read-only          │
│   │               Component->BuildSimulationParams(),            │
│   │               *Component->SpatialHash, // In/Out             │
│   │               DeltaTime,                                     │
│   │               Component->AccumulatedTime                     │
│   │           )                                                  │
│   │                                                              │
│   └── SimulateBatchedComponents(DeltaTime)                       │
│         └── 각 Preset 그룹에 대해:                               │
│               1. MergeParticles(Components)                      │
│                    - 각 Component의 Particles를 MergedBuffer로   │
│                    - BatchInfos에 (Component, StartIndex, Count) │
│               2. Context->Simulate(MergedBuffer, ...)            │
│               3. SplitParticles(Components)                      │
│                    - MergedBuffer에서 각 Component로 복사        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Component::TickComponent(DeltaTime)                              │
│   ├── UpdateRenderData()        // GPU 리소스 업데이트           │
│   └── UpdateDebugInstances()    // 디버그 메시 업데이트          │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 배치 시뮬레이션 상세

```
┌─────────────────────────────────────────────────────────────────┐
│ 배치 시뮬레이션 (같은 Preset을 가진 Component들)                 │
└─────────────────────────────────────────────────────────────────┘

[Component A]        [Component B]        [Component C]
 Particles[0..99]     Particles[0..49]     Particles[0..29]
      │                    │                    │
      └────────────────────┼────────────────────┘
                           │ MergeParticles()
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ MergedParticleBuffer[0..179]                                     │
│  ├── [0..99]   ← Component A                                    │
│  ├── [100..149] ← Component B                                   │
│  └── [150..179] ← Component C                                   │
│                                                                  │
│ BatchInfos:                                                      │
│  ├── {A, StartIndex=0,   Count=100}                             │
│  ├── {B, StartIndex=100, Count=50}                              │
│  └── {C, StartIndex=150, Count=30}                              │
└─────────────────────────────────────────────────────────────────┘
                           │
                           │ Context->Simulate()
                           │ (모든 180개 파티클이 서로 상호작용!)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ MergedParticleBuffer[0..179] (시뮬레이션 후)                     │
└─────────────────────────────────────────────────────────────────┘
                           │ SplitParticles()
                           ▼
[Component A]        [Component B]        [Component C]
 Particles[0..99]     Particles[0..49]     Particles[0..29]
 (업데이트됨)          (업데이트됨)          (업데이트됨)
```

---

## 4. 스레딩 모델

### 4.1 병렬화 전략

```cpp
// 1. ParallelFor 사용 위치
PredictPositions()      // 각 파티클 독립 - 병렬
UpdateNeighbors()       // 이웃 검색 - 병렬 (읽기만)
SolveDensityConstraints() // SIMD + ParallelFor
FinalizePositions()     // 각 파티클 독립 - 병렬
HandleWorldCollision()  // Cell별 병렬 + 파티클별 병렬

// 2. 순차 처리가 필요한 부분
SpatialHash::BuildFromPositions()  // HashMap 쓰기 - 순차
HandleCollisions()                  // Collider당 순차 (내부는 병렬 가능)
```

### 4.2 스레드 안전성

```cpp
// 문제: 충돌 이벤트는 병렬 루프 내에서 발생
// 해결: Atomic counter + AsyncTask

// Subsystem에서 관리
std::atomic<int32> EventCountThisFrame{0};

// 병렬 루프 내에서
if (bCanEmitEvent)
{
    // Atomic 증가로 이벤트 수 제한
    int32 CurrentCount = EventCountPtr->fetch_add(1, std::memory_order_relaxed);
    if (CurrentCount < MaxEventsPerFrame)
    {
        // GameThread에서 콜백 실행 (스레드 안전)
        AsyncTask(ENamedThreads::GameThread, [Callback, Event]()
        {
            Callback.Execute(Event);
        });
    }
}
```

### 4.3 Physics Scene 접근

```cpp
// 문제: World collision은 Physics Scene 접근 필요
// 해결: FPhysicsCommand::ExecuteRead로 읽기 잠금

FPhysScene* PhysScene = World->GetPhysicsScene();
FPhysicsCommand::ExecuteRead(PhysScene, [&]()
{
    // 이 블록 내에서는 Physics Scene 읽기 안전
    ParallelFor(CollisionParticleIndices.Num(), [&](int32 j)
    {
        World->SweepSingleByChannel(...);
    });
});
```

---

## 5. 메모리 관리

### 5.1 소유권 모델

```
UKawaiiFluidSimulatorSubsystem (World Lifetime)
    │
    ├── TObjectPtr<UKawaiiFluidSimulationContext> DefaultContext  // UObject - GC 관리
    ├── TMap<Class, Context*> ContextCache                        // UObject - GC 관리
    ├── TSharedPtr<FSpatialHash> SharedSpatialHash                // 공유 포인터
    ├── TArray<FFluidParticle> MergedParticleBuffer               // 값 배열
    └── TArray<FKawaiiFluidBatchInfo> BatchInfos                  // 값 배열 (raw ptr 포함)

UKawaiiFluidSimulationComponent (Actor Lifetime)
    │
    ├── TArray<FFluidParticle> Particles                          // 값 배열 (핵심 데이터)
    ├── TObjectPtr<UKawaiiFluidPresetDataAsset> Preset            // UObject 참조
    ├── TSharedPtr<FSpatialHash> SpatialHash                      // 공유 포인터
    ├── TSharedPtr<FKawaiiFluidRenderResource> RenderResource     // 공유 포인터
    └── TObjectPtr<UInstancedStaticMeshComponent> DebugMeshComponent // UObject - GC
```

### 5.2 배치 정보의 수명

```cpp
// FKawaiiFluidBatchInfo는 raw pointer 사용
struct FKawaiiFluidBatchInfo
{
    UKawaiiFluidSimulationComponent* Component;  // raw pointer!
    int32 StartIndex;
    int32 ParticleCount;
};

// 안전한 이유:
// 1. BatchInfos는 단일 프레임 내에서만 사용
// 2. MergeParticles() → Simulate() → SplitParticles() 순차 실행
// 3. 프레임 끝에서 BatchInfos.Reset()으로 클리어
```

---

## 6. 확장성 설계

### 6.1 커스텀 Context 생성

```cpp
// 예시: 용암 시뮬레이션
UCLASS()
class ULavaSimulationContext : public UKawaiiFluidSimulationContext
{
    GENERATED_BODY()

protected:
    // 온도 기반 점성 오버라이드
    virtual void ApplyViscosity(
        TArray<FFluidParticle>& Particles,
        const UKawaiiFluidPresetDataAsset* Preset) override
    {
        for (FFluidParticle& P : Particles)
        {
            // 온도가 낮으면 점성 증가 (굳어감)
            float TempFactor = FMath::Clamp(P.Temperature / MaxTemp, 0.1f, 1.0f);
            float DynamicViscosity = Preset->ViscosityCoefficient / TempFactor;
            // ...
        }
    }

    // 추가 단계: 온도 감소
    virtual void SimulateSubstep(...) override
    {
        Super::SimulateSubstep(...);
        CoolDown(Particles, SubstepDT);  // 새 단계 추가
    }
};
```

### 6.2 Preset에서 Context 지정

```cpp
// DataAsset에서 설정
UPROPERTY(EditAnywhere, Category = "Extension")
TSubclassOf<UKawaiiFluidSimulationContext> ContextClass;

// Subsystem에서 캐싱 및 사용
UKawaiiFluidSimulationContext* Subsystem::GetOrCreateContext(const Preset* P)
{
    if (!P || !P->ContextClass)
        return DefaultContext;

    if (auto* Found = ContextCache.Find(P->ContextClass))
        return *Found;

    auto* NewContext = NewObject<UKawaiiFluidSimulationContext>(this, P->ContextClass);
    NewContext->InitializeSolvers(P);
    ContextCache.Add(P->ContextClass, NewContext);
    return NewContext;
}
```

---

## 7. 성능 고려사항

### 7.1 프로파일링 포인트

```cpp
// 각 단계별 STAT 매크로
DECLARE_CYCLE_STAT(TEXT("Context Simulate"), STAT_ContextSimulate, ...);
DECLARE_CYCLE_STAT(TEXT("Context PredictPositions"), STAT_ContextPredictPositions, ...);
DECLARE_CYCLE_STAT(TEXT("Context UpdateNeighbors"), STAT_ContextUpdateNeighbors, ...);
DECLARE_CYCLE_STAT(TEXT("Context SolveDensity"), STAT_ContextSolveDensity, ...);
// ... 등

// Unreal Insights에서 확인 가능:
// stat KawaiiFluidContext
// stat KawaiiFluidSubsystem
```

### 7.2 병목 지점

| 단계 | 복잡도 | 병목 원인 | 최적화 |
|------|--------|-----------|--------|
| BuildSpatialHash | O(N) | HashMap 쓰기 경쟁 | 순차 처리 (불가피) |
| UpdateNeighbors | O(N*K) | 이웃 검색 | ParallelFor |
| SolveDensity | O(N*K) | 커널 계산 | SIMD + ParallelFor |
| WorldCollision | O(N) | Physics Query | Cell Broad-Phase + ParallelFor |

### 7.3 배치 vs 독립 모드

```
[독립 모드 (bIndependentSimulation = true)]
장점:
- 완전한 격리 (다른 Component 영향 X)
- 더 작은 SpatialHash
- 병렬화 가능성 높음

단점:
- Component 간 상호작용 불가
- Context 호출 오버헤드 (Component 수만큼)

[배치 모드 (bIndependentSimulation = false)]
장점:
- Component 간 파티클 상호작용
- Context 호출 1회 (Preset당)
- 통합 SpatialHash로 이웃 검색 효율

단점:
- Merge/Split 오버헤드
- 메모리 복사 비용
```

---

## 8. 알려진 이슈 및 개선점

### 8.1 현재 이슈

1. **밀도 계산 파라미터 민감도**
   - RestDensity, Compliance, SmoothingRadius 조합에 민감
   - 잘못된 값 → 파티클 폭발 또는 뭉침

2. **Blueprint 이벤트 제한**
   - OnParticleHit은 Component에 있지만 Subsystem에서 시뮬레이션
   - 배치 모드에서 이벤트 라우팅 복잡

3. **Per-Actor 쿨다운 미완성**
   - 파티클별 쿨다운은 구현됨
   - Actor별 쿨다운은 부분 구현

### 8.2 향후 개선 방향

1. **GPU 시뮬레이션**
   - Compute Shader로 DensityConstraint 이전
   - 10만+ 파티클 지원

2. **LOD 시스템**
   - 거리 기반 파티클 병합
   - 카메라 거리에 따른 시뮬레이션 품질 조절

3. **네트워크 동기화**
   - 결정적 시뮬레이션 (Deterministic)
   - 파티클 상태 압축 및 동기화

4. **ISPC 최적화**
   - Intel SPMD Program Compiler
   - SIMD보다 더 높은 병렬화

---

## 9. 사용 예시

### 9.1 기본 사용

```cpp
// Blueprint에서 Component 추가 후
// 1. Preset DataAsset 생성 (Content Browser)
// 2. Component에 Preset 할당
// 3. bSpawnOnBeginPlay = true 설정

// 또는 코드에서:
auto* FluidComp = NewObject<UKawaiiFluidSimulationComponent>(MyActor);
FluidComp->Preset = LoadObject<UKawaiiFluidPresetDataAsset>(...);
FluidComp->RegisterComponent();
FluidComp->SpawnParticles(GetActorLocation(), 100, 50.0f);
```

### 9.2 이벤트 처리

```cpp
// Blueprint에서 바인딩 가능
UPROPERTY(BlueprintAssignable)
FOnFluidParticleHitComponent OnParticleHit;

// C++에서:
FluidComp->OnParticleHit.AddDynamic(this, &AMyActor::HandleFluidHit);

void AMyActor::HandleFluidHit(int32 Index, AActor* HitActor,
    FVector Location, FVector Normal, float Speed)
{
    // 충돌 처리
    if (Speed > 100.0f)
    {
        SpawnSplashEffect(Location, Normal);
    }
}
```

### 9.3 다중 유체 상호작용

```cpp
// 같은 Preset을 사용하면 자동으로 배치됨
// Component A (Preset: Slime)
// Component B (Preset: Slime)  ← A와 B의 파티클이 상호작용!
// Component C (Preset: Water)  ← 별도 시뮬레이션

// 독립 시뮬레이션 강제
FluidComp->bIndependentSimulation = true;  // 다른 Component와 상호작용 X
```
