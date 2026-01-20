# KawaiiFluid Architecture Refactoring Plan

## 개요

KawaiiFluid 시스템을 **Volume (Solver)** 과 **Emitter (Spawn)** 로 분리하여 더 직관적이고 유연한 구조로 리팩토링합니다.

---

## 1. 현재 구조 vs 신규 구조

### 현재 구조
```
UKawaiiFluidComponent (모든 것을 담당)
├── SpawnSettings (스폰 설정)
├── UKawaiiFluidSimulationModule (파티클 데이터)
│   └── UKawaiiFluidSimulationContext (Solver)
├── RenderModule (렌더링)
└── TargetSimulationVolume (선택적 외부 Volume 참조)

AKawaiiFluidSimulationVolume (공간 경계만)
└── UKawaiiFluidSimulationVolumeComponent (Z-Order 공간 정의)
```

### 신규 구조
```
AKawaiiFluidVolume (Solver 단위)
├── UKawaiiFluidVolumeComponent (공간 경계 + 설정)
├── UKawaiiFluidPresetDataAsset* Preset (물리/렌더링 파라미터)
├── UKawaiiFluidSimulationModule (파티클 데이터)
├── UKawaiiFluidSimulationContext (Solver, GPU Simulator)
├── UKawaiiFluidRenderModule (렌더링)
└── TArray<AKawaiiFluidEmitter*> RegisteredEmitters

AKawaiiFluidEmitter (스폰 전용)
├── UKawaiiFluidEmitterComponent (스폰 로직)
├── AKawaiiFluidVolume* TargetVolume (참조)
├── FFluidSpawnSettings SpawnSettings
└── SourceID (파티클 추적용)

UKawaiiFluidComponent (Deprecated - 레거시 호환용)
└── 기존 동작 유지
```

---

## 2. 네이밍 변경

| 기존 | 신규 | 비고 |
|------|------|------|
| `AKawaiiFluidSimulationVolume` | `AKawaiiFluidVolume` | CoreRedirects 적용 |
| `UKawaiiFluidSimulationVolumeComponent` | `UKawaiiFluidVolumeComponent` | CoreRedirects 적용 |
| (신규) | `AKawaiiFluidEmitter` | 새로 생성 |
| (신규) | `UKawaiiFluidEmitterComponent` | 새로 생성 |
| `UKawaiiFluidComponent` | (유지, Deprecated) | 레거시 호환 |

---

## 3. 클래스별 책임

### AKawaiiFluidVolume
```cpp
UCLASS()
class KAWAIIFLUIDRUNTIME_API AKawaiiFluidVolume : public AActor
{
    // === Components ===
    UPROPERTY(VisibleAnywhere)
    UKawaiiFluidVolumeComponent* VolumeComponent;

    // === Configuration ===
    UPROPERTY(EditAnywhere, Category = "Fluid")
    UKawaiiFluidPresetDataAsset* Preset;

    // === Simulation ===
    UPROPERTY()
    UKawaiiFluidSimulationModule* SimulationModule;

    UPROPERTY()
    UKawaiiFluidSimulationContext* SimulationContext;

    // === Rendering ===
    UPROPERTY(EditAnywhere, Category = "Rendering")
    UKawaiiFluidRenderModule* RenderModule;

    // === Emitter Management ===
    TArray<TWeakObjectPtr<AKawaiiFluidEmitter>> RegisteredEmitters;

    // === API ===
    void RegisterEmitter(AKawaiiFluidEmitter* Emitter);
    void UnregisterEmitter(AKawaiiFluidEmitter* Emitter);
    void QueueSpawnRequest(const FGPUSpawnRequest& Request);
    void Simulate(float DeltaTime);

    // === Getters ===
    UKawaiiFluidPresetDataAsset* GetPreset() const { return Preset; }
    float GetParticleSpacing() const;
    FVector GetVolumeSize() const;
};
```

### AKawaiiFluidEmitter
```cpp
UCLASS()
class KAWAIIFLUIDRUNTIME_API AKawaiiFluidEmitter : public AActor
{
    // === Components ===
    UPROPERTY(VisibleAnywhere)
    UKawaiiFluidEmitterComponent* EmitterComponent;

    // === Target Volume ===
    UPROPERTY(EditAnywhere, Category = "Fluid")
    AKawaiiFluidVolume* TargetVolume;

    // === Spawn Settings ===
    UPROPERTY(EditAnywhere, Category = "Spawn")
    FFluidSpawnSettings SpawnSettings;

    // === Identification ===
    UPROPERTY()
    int32 SourceID;

    // === API ===
    void SetTargetVolume(AKawaiiFluidVolume* Volume);
    float GetParticleSpacing() const; // Volume의 Preset에서 가져옴

    // === Spawn Execution ===
    void ExecuteAutoSpawn();      // ShapeVolume 모드
    void ProcessContinuousSpawn(float DeltaTime); // Emitter 모드
};
```

### UKawaiiFluidVolumeComponent
```cpp
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidVolumeComponent : public USceneComponent
{
    // === Volume Size (기존 유지) ===
    UPROPERTY(EditAnywhere, Category = "Volume")
    bool bUniformSize = true;

    UPROPERTY(EditAnywhere, Category = "Volume")
    float UniformVolumeSize;

    UPROPERTY(EditAnywhere, Category = "Volume")
    FVector VolumeSize;

    // === Wall Physics ===
    UPROPERTY(EditAnywhere, Category = "Volume")
    float WallBounce = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Volume")
    float WallFriction = 0.1f;

    // === Z-Order Grid (Auto-calculated) ===
    UPROPERTY(VisibleAnywhere, Category = "Internal")
    EGridResolutionPreset GridResolutionPreset;

    UPROPERTY(VisibleAnywhere, Category = "Internal")
    float CellSize;

    // ... 기존 VolumeComponent 로직 유지
};
```

### UKawaiiFluidEmitterComponent
```cpp
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidEmitterComponent : public USceneComponent
{
    // Owner Emitter 참조
    AKawaiiFluidEmitter* GetOwnerEmitter() const;

    // Spawn 실행 로직 (KawaiiFluidComponent에서 이동)
    void ExecuteAutoSpawn();
    void ProcessContinuousSpawn(float DeltaTime);

    // 스폰 헬퍼 함수들
    void SpawnParticlesSphere(...);
    void SpawnParticlesBox(...);
    void SpawnParticlesCylinder(...);
    void SpawnParticleDirectional(...);
    void SpawnParticleDirectionalHexLayer(...);
};
```

---

## 4. Batching 구조

### Simulation Batching (Volume 단위)
```cpp
// UKawaiiFluidSubsystem::Tick()
void UKawaiiFluidSubsystem::Tick(float DeltaTime)
{
    // Volume 단위로 시뮬레이션 (1 Volume = 1 GPU Dispatch)
    for (AKawaiiFluidVolume* Volume : RegisteredVolumes)
    {
        Volume->Simulate(DeltaTime);
    }
}
```

### Rendering Batching (Volume 단위)
```cpp
// AKawaiiFluidVolume이 자체 RenderModule 소유
// 해당 Volume의 모든 파티클 한번에 렌더링
// Draw Call: Volume당 1-2회
```

### 데이터 흐름
```
┌─────────────────────────────────────────────────────────┐
│                  AKawaiiFluidVolume                     │
│  ┌─────────┐  ┌────────────┐  ┌──────────────────────┐ │
│  │ Preset  │  │  Module    │  │   SimulationContext  │ │
│  │(물리설정)│  │(파티클DB) │  │   (GPU Simulator)    │ │
│  └────┬────┘  └─────┬──────┘  └──────────┬───────────┘ │
│       │             │                     │             │
│       │      ┌──────┴──────┐      ┌──────┴──────┐      │
│       │      │ Spawn Queue │      │RenderModule │      │
│       │      └──────┬──────┘      └─────────────┘      │
│       │             │                                   │
└───────┼─────────────┼───────────────────────────────────┘
        │             ▲
        │     ┌───────┴───────┐
        │     │ SpawnRequests │
        │     └───────┬───────┘
        │             │
   ┌────┴────┐  ┌─────┴─────┐  ┌───────────┐
   │Emitter1 │  │ Emitter2  │  │ Emitter3  │
   │(Stream) │  │ (Spray)   │  │ (Shape)   │
   └─────────┘  └───────────┘  └───────────┘

   * 모든 Emitter는 Volume의 Preset 파라미터 사용
   * 한 Volume = 한 Z-Order 공간 = 한 GPU Dispatch
```

---

## 5. 레거시 호환성

### CoreRedirects 설정
```ini
; Config/DefaultEngine.ini
[CoreRedirects]
+ClassRedirects=(OldName="/Script/KawaiiFluidRuntime.KawaiiFluidSimulationVolume",NewName="/Script/KawaiiFluidRuntime.KawaiiFluidVolume")
+ClassRedirects=(OldName="/Script/KawaiiFluidRuntime.KawaiiFluidSimulationVolumeComponent",NewName="/Script/KawaiiFluidRuntime.KawaiiFluidVolumeComponent")
```

### UKawaiiFluidComponent (Deprecated 유지)
```cpp
UCLASS(meta=(DeprecationMessage="Use AKawaiiFluidVolume + AKawaiiFluidEmitter instead"))
class UKawaiiFluidComponent : public USceneComponent
{
    // 기존 코드 그대로 유지
    // 동작에는 문제 없음
    // 에디터에서 Deprecated 경고만 표시
};
```

### 마이그레이션 도구 (선택적)
```cpp
// Editor Utility Widget으로 제공
UFUNCTION(CallInEditor, Category = "Migration")
void ConvertLegacyToVolumeEmitter()
{
    // 1. 기존 Component 설정 읽기
    // 2. Volume 생성 및 설정 복사
    // 3. Emitter 생성 및 SpawnSettings 복사
    // 4. 완료 로그
}
```

---

## 6. 파일 구조

### 신규 생성 파일
```
Plugins/KawaiiFluidSystem/Source/KawaiiFluidRuntime/
├── Public/
│   ├── Actors/
│   │   ├── KawaiiFluidVolume.h          (신규 - AKawaiiFluidVolume)
│   │   └── KawaiiFluidEmitter.h         (신규 - AKawaiiFluidEmitter)
│   └── Components/
│       ├── KawaiiFluidVolumeComponent.h (리네임 - 기존 SimulationVolumeComponent)
│       └── KawaiiFluidEmitterComponent.h (신규)
│
└── Private/
    ├── Actors/
    │   ├── KawaiiFluidVolume.cpp        (신규)
    │   └── KawaiiFluidEmitter.cpp       (신규)
    └── Components/
        ├── KawaiiFluidVolumeComponent.cpp (리네임)
        └── KawaiiFluidEmitterComponent.cpp (신규)
```

### 수정 파일
```
- KawaiiFluidSubsystem.h/cpp     (Volume 단위 시뮬레이션으로 변경)
- KawaiiFluidSimulationModule.h/cpp (Volume 소유로 이동)
- KawaiiFluidSimulationContext.h/cpp (Volume 소유로 이동)
- KawaiiFluidComponent.h/cpp     (Deprecated 마킹, 동작 유지)
- DefaultEngine.ini              (CoreRedirects 추가)
```

---

## 7. 구현 단계

### Phase 1: 기반 구조 (1주)

#### 1.1 네이밍 변경
- [ ] `AKawaiiFluidSimulationVolume` → `AKawaiiFluidVolume` 리네임
- [ ] `UKawaiiFluidSimulationVolumeComponent` → `UKawaiiFluidVolumeComponent` 리네임
- [ ] CoreRedirects 설정 추가
- [ ] 빌드 및 기존 레벨 테스트

#### 1.2 Volume 확장
- [ ] `AKawaiiFluidVolume`에 Preset 프로퍼티 추가
- [ ] `AKawaiiFluidVolume`에 SimulationModule 소유권 이동
- [ ] `AKawaiiFluidVolume`에 SimulationContext 소유권 이동
- [ ] `AKawaiiFluidVolume::Simulate()` 구현
- [ ] Subsystem에서 Volume 단위 시뮬레이션 호출

#### 1.3 Emitter 기본 생성
- [ ] `AKawaiiFluidEmitter` Actor 생성
- [ ] `UKawaiiFluidEmitterComponent` Component 생성
- [ ] TargetVolume 참조 시스템 구현
- [ ] Volume-Emitter 등록/해제 로직

### Phase 2: 스폰 기능 이전 (1주)

#### 2.1 SpawnSettings 이동
- [ ] `FFluidSpawnSettings`를 Emitter로 이동
- [ ] Emitter에서 Volume의 Preset 조회 API
- [ ] ParticleSpacing, MaxParticleCount 등 Volume에서 가져오기

#### 2.2 스폰 로직 이동
- [ ] `ExecuteAutoSpawn()` → EmitterComponent로 이동
- [ ] `ProcessContinuousSpawn()` → EmitterComponent로 이동
- [ ] 모든 Spawn 헬퍼 함수 이동
  - SpawnParticlesSphere*()
  - SpawnParticlesBox*()
  - SpawnParticlesCylinder*()
  - SpawnParticleDirectional*()
  - SpawnParticleDirectionalHexLayer*()

#### 2.3 Spawn Request 시스템
- [ ] `AKawaiiFluidVolume::QueueSpawnRequest()` 구현
- [ ] Emitter → Volume GPU spawn queue 연결
- [ ] SourceID 기반 파티클 추적

### Phase 3: 렌더링 통합 (3-5일)

#### 3.1 Volume 렌더링
- [ ] RenderModule을 Volume 소유로 이동
- [ ] Volume 단위 Draw Call 통합
- [ ] ISM/Metaball 렌더러 Volume 연결

#### 3.2 파티클 리사이클링
- [ ] Per-SourceID 파티클 카운트 (Volume에서 관리)
- [ ] `RemoveOldestParticles()` Emitter별 처리
- [ ] MaxParticleCount 로직 업데이트

### Phase 4: 마무리 (3-5일)

#### 4.1 레거시 처리
- [ ] `UKawaiiFluidComponent` Deprecated 마킹
- [ ] 에디터 경고 메시지 추가
- [ ] 마이그레이션 도구 (선택적)

#### 4.2 에디터 UX
- [ ] Volume 경계 시각화
- [ ] Emitter 아이콘/Gizmo
- [ ] Volume 내 Emitter 배치 시 자동 연결 (선택적)

#### 4.3 테스트 및 문서화
- [ ] 단위 테스트
- [ ] 통합 테스트 (다중 Emitter, 다중 Volume)
- [ ] 성능 테스트
- [ ] 사용 가이드 문서

---

## 8. 예상 일정

| Phase | 작업 내용 | 예상 기간 |
|-------|----------|----------|
| Phase 1 | 기반 구조 (네이밍, Volume 확장, Emitter 기본) | 1주 |
| Phase 2 | 스폰 기능 이전 | 1주 |
| Phase 3 | 렌더링 통합 | 3-5일 |
| Phase 4 | 마무리 (레거시, UX, 테스트) | 3-5일 |
| **총합** | | **약 3-4주** |

---

## 9. 위험 요소 및 대응

| 위험 | 영향 | 대응 |
|------|------|------|
| CoreRedirects 실패 | 기존 레벨 깨짐 | 철저한 테스트, 백업 |
| GPU 버퍼 관리 복잡 | 메모리 이슈 | Volume당 버퍼 풀 관리 |
| 다중 Emitter 동기화 | 스폰 타이밍 이슈 | Spawn Queue 통합 |
| 성능 저하 | 프레임 드랍 | 프로파일링, 최적화 |

---

## 10. 성공 기준

- [ ] 기존 레벨의 Legacy Actor 정상 동작
- [ ] 새 Volume + Emitter 시스템 정상 동작
- [ ] Volume 단위 Batching (1 Volume = 1 GPU Dispatch)
- [ ] 다중 Emitter가 같은 Volume에서 정상 시뮬레이션
- [ ] 성능 저하 없음 (기존 대비)
- [ ] 에디터에서 직관적인 배치/설정

---

## 부록: API 사용 예시

### Blueprint에서 사용
```
1. Level에 AKawaiiFluidVolume 배치
2. Volume의 Preset 설정 (Water, Lava 등)
3. Volume 크기 조정
4. AKawaiiFluidEmitter를 Volume 내부에 배치
5. Emitter의 TargetVolume을 해당 Volume으로 설정
6. Emitter의 SpawnSettings 설정 (Stream, Spray 등)
7. Play!
```

### C++에서 사용
```cpp
// Volume 생성
AKawaiiFluidVolume* Volume = World->SpawnActor<AKawaiiFluidVolume>();
Volume->SetPreset(WaterPreset);
Volume->SetVolumeSize(FVector(1000, 1000, 500));

// Emitter 생성
AKawaiiFluidEmitter* Emitter = World->SpawnActor<AKawaiiFluidEmitter>();
Emitter->SetTargetVolume(Volume);
Emitter->SetActorLocation(FVector(0, 0, 400));
Emitter->SpawnSettings.SpawnType = EFluidSpawnType::Emitter;
Emitter->SpawnSettings.EmitterType = EFluidEmitterType::Stream;
Emitter->SpawnSettings.FlowSpeed = 200.0f;
```
