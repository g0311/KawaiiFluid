# KawaiiFluid GPU 시뮬레이션 워크플로우 분석

## 개요

이 문서는 KawaiiFluidComponent가 GPU 시뮬레이션과 렌더링을 어떻게 수행하는지 분석한 결과입니다.
AKawaiiFluidVolume 기반 새 아키텍처 구현 시 참고용으로 작성되었습니다.

---

## 1. 컴포넌트 초기화 흐름 (KawaiiFluidComponent::BeginPlay)

```
BeginPlay()
├─ SimulationModule->Initialize(Preset)
│  ├─ OwnedVolumeComponent 생성 (외부 Volume 없을 때)
│  ├─ 내부 bounds 설정
│  └─ Preset의 SmoothingRadius로 grid resolution 계산
├─ RegisterToSubsystem()
│  └─ Subsystem->RegisterModule(SimulationModule)  ★ GPU 설정의 핵심
├─ RenderingModule->Initialize(World, Component, SimulationModule, Preset)
└─ ExecuteAutoSpawn() (SpawnSettings.SpawnType == ShapeVolume인 경우)
   └─ 초기 파티클 스폰
```

---

## 2. Subsystem RegisterModule - GPU 설정의 핵심

**파일:** `KawaiiFluidSimulatorSubsystem.cpp` (148-221행)

```cpp
void UKawaiiFluidSimulatorSubsystem::RegisterModule(UKawaiiFluidSimulationModule* Module)
{
    // 1. Module을 AllModules 배열에 추가
    AllModules.Add(Module);

    // 2. SourceID 할당 (GPU 파티클 카운터 추적용)
    const int32 NewSourceID = AllocateSourceID();
    Module->SetSourceID(NewSourceID);

    // 3. Preset 가져오기
    UKawaiiFluidPresetDataAsset* Preset = Module->GetPreset();

    if (Preset)
    {
        // 4. VolumeComponent 가져오기 (Z-Order 공간 bounds용)
        UKawaiiFluidVolumeComponent* TargetVolume = Module->GetTargetVolumeComponent();

        // 5. Context 생성/가져오기 ★
        UKawaiiFluidSimulationContext* Context = GetOrCreateContext(TargetVolume, Preset);

        if (Context)
        {
            // 6. Module에 Context 참조 설정
            Module->SetSimulationContext(Context);

            // 7. Context에 Preset 캐시
            if (!Context->GetCachedPreset())
            {
                Context->SetCachedPreset(Preset);
            }

            // 8. GPU 시뮬레이터 초기화 ★★★
            if (!Context->IsGPUSimulatorReady())
            {
                Context->InitializeGPUSimulator(Preset->MaxParticles);
            }

            // 9. Module에 GPU 시뮬레이터 연결 ★★★
            if (Context->IsGPUSimulatorReady())
            {
                Module->SetGPUSimulator(Context->GetGPUSimulator());
                Module->SetGPUSimulationActive(true);
                Module->UploadCPUParticlesToGPU();
            }

            // 10. RenderResource 초기화
            if (!Context->HasValidRenderResource())
            {
                Context->InitializeRenderResource();
            }

            // 11. MetaballRenderer 연결 (KawaiiFluidComponent 전용)
            if (UKawaiiFluidComponent* OwnerComp = Cast<UKawaiiFluidComponent>(Module->GetOuter()))
            {
                // ... MetaballRenderer 설정
            }
        }
    }
}
```

### 핵심 포인트

1. **SourceID 할당**: 각 Module에 고유 ID 부여 (GPU 파티클 카운터용)
2. **Context 생성**: (VolumeComponent + Preset) 조합으로 캐시됨
3. **GPU 시뮬레이터 초기화**: `Context->InitializeGPUSimulator(MaxParticles)`
4. **Module ↔ GPU 연결**: `Module->SetGPUSimulator()`, `Module->SetGPUSimulationActive(true)`

---

## 3. Context의 GPU 시뮬레이터 초기화

**파일:** `KawaiiFluidSimulationContext.cpp`

```cpp
void UKawaiiFluidSimulationContext::InitializeGPUSimulator(int32 MaxParticles)
{
    if (!GPUSimulator)
    {
        GPUSimulator = new FGPUFluidSimulator();
    }

    // GPU 리소스 초기화
    GPUSimulator->Initialize(MaxParticles);

    // RHI 리소스 생성:
    // - ParticleBuffer (FGPUFluidParticle[MaxParticles])
    // - PositionBuffer (FVector4[MaxParticles])
    // - CellCounts, ParticleIndices (Spatial Hash)
    // - CellStart, CellEnd (Z-Order)
    // - AnisotropyBuffers (타원체 렌더링용)
}
```

---

## 4. Subsystem Tick - 시뮬레이션 실행

**파일:** `KawaiiFluidSimulatorSubsystem.cpp` (100-142행)

```cpp
void UKawaiiFluidSimulatorSubsystem::Tick(float DeltaTime)
{
    // AllModules가 있어야 시뮬레이션 실행됨!
    if (AllModules.Num() > 0)
    {
        SimulateIndependentFluidComponents(DeltaTime);
        SimulateBatchedFluidComponents(DeltaTime);

        // Collision feedback 처리
        // ...
    }
}
```

### SimulateIndependentFluidComponents 조건

```cpp
void UKawaiiFluidSimulatorSubsystem::SimulateIndependentFluidComponents(float DeltaTime)
{
    for (UKawaiiFluidSimulationModule* Module : AllModules)
    {
        // 시뮬레이션 스킵 조건들:
        if (!Module) continue;
        if (!Module->IsSimulationEnabled()) continue;
        if (Module->GetParticleCount() == 0) continue;  // ★ 파티클이 0이면 스킵!
        if (!Module->IsIndependentSimulation()) continue;

        // Context 가져오기
        UKawaiiFluidSimulationContext* Context = GetOrCreateContext(TargetVolume, Preset);

        // GPU 시뮬레이터 설정 (아직 안됐으면)
        if (!Context->IsGPUSimulatorReady())
        {
            Context->InitializeGPUSimulator(Preset->MaxParticles);
        }

        if (Context->IsGPUSimulatorReady())
        {
            Module->SetGPUSimulator(Context->GetGPUSimulator());
            Module->SetGPUSimulationActive(true);
        }

        // 시뮬레이션 실행!
        Context->Simulate(Particles, Preset, Params, SpatialHash, DeltaTime, AccumulatedTime);
    }
}
```

---

## 5. 파티클 스폰 흐름

### SpawnParticle 함수

```cpp
int32 UKawaiiFluidSimulationModule::SpawnParticle(FVector Position, FVector Velocity)
{
    // ★ CachedGPUSimulator가 없으면 바로 반환! (-1)
    if (!CachedGPUSimulator)
    {
        return -1;
    }

    // GPU 스폰 요청 생성
    FGPUSpawnRequest Request;
    Request.Position = FVector3f(Position);
    Request.Velocity = FVector3f(Velocity);
    Request.SourceID = CachedSourceID;
    Request.Mass = Preset->ParticleMass;
    Request.Radius = Preset->ParticleRadius;

    // GPU에 스폰 요청 전송
    CachedGPUSimulator->QueueSpawnRequest(Request);

    return NextParticleID++;
}
```

### GetParticleCount 함수

```cpp
virtual int32 GetParticleCount() const override
{
    if (bGPUSimulationActive && CachedGPUSimulator)
    {
        // GPU 파티클 수 + 대기 중인 스폰 요청 수
        return CachedGPUSimulator->GetParticleCount() +
               CachedGPUSimulator->GetPendingSpawnCount();
    }
    return Particles.Num();  // CPU fallback
}
```

---

## 6. GPU 시뮬레이션 패스

```
Context::SimulateGPU()
├─ GPU 시뮬레이터 초기화 확인
└─ 메인 루프 (서브스텝 2-3회):
   └─ GPUSimulator->SimulateSubstep(Params)
      ├─ Phase 1: 파티클 버퍼 준비
      │  └─ CPU 파티클 → GPU 업로드 (필요시)
      ├─ Phase 2: 공간 구조 빌드
      │  ├─ Z-Order Morton 정렬
      │  └─ CellStart/CellEnd 테이블 생성
      ├─ Phase 3: 물리 솔버
      │  ├─ 위치 예측 (Position += Velocity * dt)
      │  └─ 압력 솔버 루프 (5-10회 반복)
      │     ├─ 이웃 업데이트
      │     ├─ 밀도 계산
      │     └─ XPBD 압력 제약 해결
      ├─ Phase 4: 충돌 & 부착
      │  ├─ 볼륨 경계 충돌
      │  ├─ DF 충돌 (활성화시)
      │  └─ 프리미티브 충돌
      └─ Phase 5: 후처리
         ├─ 속도 계산 (Velocity = ΔPosition / dt)
         ├─ 점성 적용 (XSPH)
         └─ 비등방성 계산 (타원체 렌더링용)
```

---

## 7. Preset 물리 파라미터 → GPU 전달 경로

```
UKawaiiFluidPresetDataAsset (에디터 설정)
├─ SmoothingRadius    → CellSize = SmoothingRadius * 0.5
├─ RestDensity        → TargetDensity
├─ Compliance         → Lambda 계산에 사용
├─ Viscosity          → XSPH 스무딩 계수
├─ CohesionCoefficient → 표면 장력
├─ SolverIterations   → 압력 루프 횟수
├─ Gravity            → ExternalForce
└─ MaxParticles       → GPU 버퍼 크기

데이터 경로:
Preset → Module->BuildSimulationParams()
      → FKawaiiFluidSimulationParams (CPU 구조체)
      → FGPUFluidSimulationParams (GPU 셰이더 파라미터)
      → Compute Shader 전역 변수
```

---

## 8. 렌더링 데이터 흐름

```
RenderingModule->UpdateRenderers()
├─ SimulationModule에서 데이터 가져오기
│  ├─ GetParticleCount()
│  ├─ GetGPUSimulator() → PersistentParticleBuffer
│  └─ GetSimulationContext() → RenderResource
├─ ISM 렌더러 업데이트
│  └─ 파티클당 1개 인스턴스 트랜스폼 설정
└─ Metaball 렌더러 업데이트
   └─ GPU 버퍼 직접 참조 (Readback 불필요)
```

---

## 9. AKawaiiFluidVolume과의 비교 분석

### KawaiiFluidComponent (정상 작동)

```
BeginPlay()
├─ SimulationModule = NewObject<>()
├─ SimulationModule->Initialize(Preset)
├─ Subsystem->RegisterModule(SimulationModule)  ★
│  └─ GPU 시뮬레이터 설정 완료
├─ RenderingModule->Initialize(World, this, SimulationModule, Preset)
└─ AutoSpawn → SimulationModule->SpawnParticle()
   └─ CachedGPUSimulator 있음 → 성공!
```

### AKawaiiFluidVolume (현재 문제)

```
BeginPlay()
├─ InitializeSimulation()
│  ├─ SimulationModule = NewObject<>(this)
│  ├─ SimulationModule->Initialize(Preset)
│  ├─ SimulationModule->SetTargetSimulationVolume(this)
│  ├─ VolumeComponent->RegisterModule(SimulationModule)
│  └─ Subsystem->RegisterModule(SimulationModule)  ★
│     └─ GPU 시뮬레이터 설정 완료
├─ InitializeRendering()
│  └─ RenderingModule->Initialize(World, VolumeComponent, SimulationModule, Preset)
└─ RegisterToSubsystem()

Tick()
├─ ProcessPendingSpawnRequests()  ← 추가됨
│  └─ SimulationModule->SpawnParticle()
│     └─ CachedGPUSimulator 있어야 함
└─ RenderingModule->UpdateRenderers()
```

---

## 10. 잠재적 문제점 분석

### 문제 1: RenderingModule 초기화 시점

KawaiiFluidComponent는 RenderingModule 초기화 시 **자신(this)**을 SceneComponent로 전달:
```cpp
RenderingModule->Initialize(World, this, SimulationModule, Preset);
```

AKawaiiFluidVolume은 **VolumeComponent**를 전달:
```cpp
RenderingModule->Initialize(World, VolumeComponent, SimulationModule, Preset);
```

→ 이것은 정상임 (VolumeComponent도 SceneComponent)

### 문제 2: Module의 Outer가 다름

Subsystem::RegisterModule에서 MetaballRenderer 연결 코드:
```cpp
if (UKawaiiFluidComponent* OwnerComp = Cast<UKawaiiFluidComponent>(Module->GetOuter()))
{
    if (UKawaiiFluidRenderingModule* RenderingMod = OwnerComp->GetRenderingModule())
    {
        if (UKawaiiFluidMetaballRenderer* MR = RenderingMod->GetMetaballRenderer())
        {
            MR->SetSimulationContext(Context);
        }
    }
}
```

- KawaiiFluidComponent: `Module->GetOuter()` = UKawaiiFluidComponent ✓
- AKawaiiFluidVolume: `Module->GetOuter()` = AKawaiiFluidVolume (AActor) ✗

→ Cast 실패로 MetaballRenderer가 Context에 연결되지 않음!

### 문제 3: Spawn 시점과 GPU 초기화 순서

Emitter가 스폰할 때:
1. EmitterComponent::Tick() → QueueSpawnRequest() → Volume->QueueSpawnRequests()
2. Volume::Tick() → ProcessPendingSpawnRequests() → SimulationModule->SpawnParticle()
3. SpawnParticle()에서 `CachedGPUSimulator`가 nullptr이면 -1 반환

InitializeSimulation()에서 Subsystem->RegisterModule() 호출로 GPU가 설정되어야 함.
하지만 **실제로 GPU 시뮬레이터가 제대로 초기화되었는지 확인 필요**.

---

## 11. 검증해야 할 사항

1. **Context->InitializeGPUSimulator() 호출 여부**
   - 로그: "GPU simulation initialized at registration" 출력되는지

2. **Module->SetGPUSimulator() 호출 여부**
   - CachedGPUSimulator가 nullptr이 아닌지

3. **Subsystem::Tick()에서 시뮬레이션 실행 조건**
   - AllModules.Num() > 0 인지
   - Module->GetParticleCount() > 0 인지

4. **MetaballRenderer의 Context 연결**
   - AKawaiiFluidVolume용 코드 추가 필요

---

## 12. 권장 수정 사항

### 수정 1: Volume용 MetaballRenderer 연결 추가

Subsystem::RegisterModule()에 추가:
```cpp
// 기존 코드 (KawaiiFluidComponent용)
if (UKawaiiFluidComponent* OwnerComp = Cast<UKawaiiFluidComponent>(Module->GetOuter()))
{
    // ...
}
// 추가: AKawaiiFluidVolume용
else if (AKawaiiFluidVolume* OwnerVolume = Cast<AKawaiiFluidVolume>(Module->GetOuter()))
{
    if (UKawaiiFluidRenderingModule* RenderingMod = OwnerVolume->GetRenderingModule())
    {
        if (UKawaiiFluidMetaballRenderer* MR = RenderingMod->GetMetaballRenderer())
        {
            MR->SetSimulationContext(Context);
        }
    }
}
```

### 수정 2: 디버그 로그 추가

Volume의 InitializeSimulation()에 검증 로그 추가:
```cpp
if (SimulationModule->GetGPUSimulator())
{
    UE_LOG(LogTemp, Log, TEXT("Volume: GPU Simulator ready, Active=%d"),
           SimulationModule->IsGPUSimulationActive());
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Volume: GPU Simulator NOT ready!"));
}
```

### 수정 3: FluidRendererSubsystem 등록 추가 ★

KawaiiFluidComponent::RegisterToSubsystem()에서는 두 가지 등록을 수행:
```cpp
Subsystem->RegisterModule(SimulationModule);
RendererSubsystem->RegisterRenderingModule(RenderingModule);  // ← 이것이 누락됨!
```

AKawaiiFluidVolume에 추가 필요:
```cpp
// InitializeRendering()에 추가
if (UFluidRendererSubsystem* RendererSubsystem = World->GetSubsystem<UFluidRendererSubsystem>())
{
    RendererSubsystem->RegisterRenderingModule(RenderingModule);
}
```

### 수정 4: 초기화 순서 문제 (MetaballRenderer → Context 연결 시점) ★★

**문제**: `Subsystem::RegisterModule()`이 `InitializeRendering()` 전에 호출됨
- RegisterModule에서 MetaballRenderer → Context 연결 시도
- 하지만 MetaballRenderer는 `RenderingModule->Initialize()` 시점에 생성됨
- 결과: MetaballRenderer가 nullptr이라 연결 실패

**해결**: `InitializeRendering()` 후에 수동으로 연결:
```cpp
// InitializeRendering()에서 MetaballRenderer 생성 후
if (UKawaiiFluidMetaballRenderer* MetaballRenderer = RenderingModule->GetMetaballRenderer())
{
    MetaballRenderer->ApplySettings(MetaballSettings);

    // 여기서 수동 연결!
    if (SimulationContext)
    {
        MetaballRenderer->SetSimulationContext(SimulationContext);
    }
}
```

---

## 13. 적용된 수정 사항 (2026-01-21)

| 수정 | 파일 | 설명 |
|------|------|------|
| Volume::Tick 스폰 처리 | KawaiiFluidVolume.cpp | `ProcessPendingSpawnRequests()` 호출 추가 |
| Volume용 MetaballRenderer 연결 | KawaiiFluidSimulatorSubsystem.cpp | AKawaiiFluidVolume Cast 분기 추가 |
| FluidRendererSubsystem 등록 | KawaiiFluidVolume.cpp | `InitializeRendering()`에 추가 |
| MetaballRenderer → Context 연결 | KawaiiFluidVolume.cpp | `InitializeRendering()`에서 수동 연결 |
| 디버그 로그 | KawaiiFluidVolume.cpp | GPU 초기화 상태 및 스폰 결과 로그 |

---

## 요약

KawaiiFluidComponent의 GPU 시뮬레이션 핵심 흐름:

1. **BeginPlay** → SimulationModule 생성 및 초기화
2. **Subsystem::RegisterModule()** → Context 생성 → GPU 시뮬레이터 초기화 → Module에 연결
3. **Subsystem::Tick()** → AllModules 순회 → Context->Simulate() 호출
4. **파티클 스폰** → CachedGPUSimulator->QueueSpawnRequest()

AKawaiiFluidVolume이 작동하지 않는 주요 원인:
- Subsystem의 MetaballRenderer 연결 코드가 KawaiiFluidComponent 전용
- GPU 시뮬레이터 초기화 성공 여부 확인 필요
