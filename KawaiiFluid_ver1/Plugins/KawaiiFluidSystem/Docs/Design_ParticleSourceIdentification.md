# 파티클 소속 정보 시스템 설계서

## 개요

파티클이 어느 Preset/Component에서 스폰됐는지 식별하고, 충돌 시 해당 정보를 CPU로 전달하는 시스템.

---

## 1. 요구사항 정의

### 1.1 기능 요구사항

| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| FR-01 | 파티클이 어느 Preset에서 왔는지 식별 | 필수 |
| FR-02 | 파티클이 어느 Component에서 스폰됐는지 식별 | 필수 |
| FR-03 | 충돌 시 소속 정보를 CPU로 전달 | 필수 |
| FR-04 | 소속 정보 기반 이벤트 콜백 | 필수 |
| FR-05 | 런타임 컴포넌트 생성/삭제 대응 | 필수 |
| FR-06 | 통합 시뮬레이션 시 Preset별 상호작용 분기 | 선택 |

### 1.2 비기능 요구사항

| ID | 요구사항 | 제약 |
|----|----------|------|
| NFR-01 | GPU 구조체 크기 유지 | 64 bytes (변경 불가) |
| NFR-02 | 16-byte alignment 유지 | GPU 메모리 접근 최적화 |
| NFR-03 | 최대 Preset 수 | 확장 가능하게 설계 |
| NFR-04 | 최대 Component 수 | 확장 가능하게 설계 |
| NFR-05 | 스레드 안전성 | GameThread ↔ RenderThread 동기화 |

---

## 2. 데이터 구조 설계

### 2.1 소속 정보 저장 방식 비교

#### Option A: Flags 비트 패킹 (8-bit each, 최대 256개)

```
┌─────────────────────────────────────────────────────────────────┐
│                        uint32 Flags (32 bits)                   │
├─────────┬─────────┬─────────────────┬───────────────────────────┤
│ 31...24 │ 23...16 │     15...5      │          4...0            │
├─────────┼─────────┼─────────────────┼───────────────────────────┤
│ CompIdx │ PresetIdx│    Reserved    │       ParticleFlags       │
│ (8-bit) │ (8-bit) │    (11-bit)    │         (5-bit)           │
└─────────┴─────────┴─────────────────┴───────────────────────────┘
```

- **장점**: 구조체 크기 변경 없음
- **단점**: 최대 256개 제한

#### Option B: ClusterID 필드 활용 (16-bit each, 최대 65536개)

```
┌─────────────────────────────────────────────────────────────────┐
│                     int32 ClusterID (32 bits)                   │
│              → SourceID로 리네임하여 재활용                       │
├─────────────────────────────────┬───────────────────────────────┤
│            31...16              │            15...0             │
├─────────────────────────────────┼───────────────────────────────┤
│      ComponentIndex (16-bit)    │      PresetIndex (16-bit)     │
│         (0 ~ 65535)             │         (0 ~ 65535)           │
└─────────────────────────────────┴───────────────────────────────┘
```

- **장점**: 65536개까지 확장 가능, 구조체 크기 유지
- **단점**: ClusterID 기능 사용 시 충돌 (슬라임 클러스터링)

#### Option C: 구조체 확장 (64 → 80 bytes)

```cpp
struct FGPUFluidParticle  // 80 bytes
{
    // 기존 64 bytes ...

    // NEW: 소속 정보 (16 bytes)
    uint32 PresetIndex;       // 4 bytes
    uint32 ComponentIndex;    // 4 bytes
    uint32 SourceActorID;     // 4 bytes
    uint32 Reserved;          // 4 bytes
};
```

- **장점**: 무제한 확장, 명확한 구조
- **단점**: 메모리 25% 증가, 모든 셰이더 수정 필요

### 2.2 권장 방식: Option B (ClusterID 재활용)

ClusterID는 현재 슬라임 클러스터링에만 사용됨. 대부분의 유체는 사용 안 함.
→ SourceID로 리네임하고 Preset/Component 인덱스 저장에 활용.

슬라임 클러스터링이 필요한 경우:
- 별도 버퍼(ClusterBuffer)로 분리하거나
- Reserved 비트 활용

---

## 3. 상세 설계

### 3.1 FGPUFluidParticle 수정

```cpp
struct FGPUFluidParticle
{
    FVector3f Position;           // 12 bytes
    float Mass;                   // 4 bytes  (total: 16)

    FVector3f PredictedPosition;  // 12 bytes
    float Density;                // 4 bytes  (total: 32)

    FVector3f Velocity;           // 12 bytes
    float Lambda;                 // 4 bytes  (total: 48)

    int32 ParticleID;             // 4 bytes
    int32 SourceID;               // 4 bytes  ← ClusterID에서 리네임
    uint32 Flags;                 // 4 bytes
    uint32 NeighborCount;         // 4 bytes  (total: 64)
};

// SourceID 레이아웃
// [31...16] ComponentIndex (0-65535)
// [15...0]  PresetIndex (0-65535)
```

### 3.2 C++ 상수 및 헬퍼

```cpp
namespace EGPUParticleSource
{
    constexpr int32 PresetIndexMask      = 0x0000FFFF;
    constexpr int32 PresetIndexShift     = 0;
    constexpr int32 ComponentIndexMask   = 0xFFFF0000;
    constexpr int32 ComponentIndexShift  = 16;

    constexpr uint16 InvalidPresetIndex    = 0xFFFF;
    constexpr uint16 InvalidComponentIndex = 0xFFFF;
    constexpr int32  InvalidSourceID       = -1;  // 0xFFFFFFFF
}

FORCEINLINE uint16 GetPresetIndex(int32 SourceID)
{
    if (SourceID == EGPUParticleSource::InvalidSourceID)
        return EGPUParticleSource::InvalidPresetIndex;
    return static_cast<uint16>(SourceID & EGPUParticleSource::PresetIndexMask);
}

FORCEINLINE uint16 GetComponentIndex(int32 SourceID)
{
    if (SourceID == EGPUParticleSource::InvalidSourceID)
        return EGPUParticleSource::InvalidComponentIndex;
    return static_cast<uint16>((SourceID & EGPUParticleSource::ComponentIndexMask)
                                >> EGPUParticleSource::ComponentIndexShift);
}

FORCEINLINE int32 MakeSourceID(uint16 PresetIndex, uint16 ComponentIndex)
{
    return static_cast<int32>(PresetIndex)
         | (static_cast<int32>(ComponentIndex) << EGPUParticleSource::ComponentIndexShift);
}

FORCEINLINE bool HasValidSource(int32 SourceID)
{
    return SourceID != EGPUParticleSource::InvalidSourceID;
}
```

### 3.3 HLSL 상수 및 헬퍼

```hlsl
#define GPU_PARTICLE_PRESET_INDEX_MASK      0x0000FFFF
#define GPU_PARTICLE_PRESET_INDEX_SHIFT     0
#define GPU_PARTICLE_COMPONENT_INDEX_MASK   0xFFFF0000
#define GPU_PARTICLE_COMPONENT_INDEX_SHIFT  16

#define GPU_PARTICLE_INVALID_SOURCE_ID      0xFFFFFFFF

uint GetPresetIndex(int SourceID)
{
    if (SourceID == GPU_PARTICLE_INVALID_SOURCE_ID) return 0xFFFF;
    return uint(SourceID) & GPU_PARTICLE_PRESET_INDEX_MASK;
}

uint GetComponentIndex(int SourceID)
{
    if (SourceID == GPU_PARTICLE_INVALID_SOURCE_ID) return 0xFFFF;
    return (uint(SourceID) & GPU_PARTICLE_COMPONENT_INDEX_MASK) >> GPU_PARTICLE_COMPONENT_INDEX_SHIFT;
}

int MakeSourceID(uint PresetIndex, uint ComponentIndex)
{
    return int(PresetIndex | (ComponentIndex << GPU_PARTICLE_COMPONENT_INDEX_SHIFT));
}

bool HasValidSource(int SourceID)
{
    return SourceID != GPU_PARTICLE_INVALID_SOURCE_ID;
}
```

---

## 4. 충돌 피드백 구조체

### 4.1 FGPUCollisionFeedback 확장 (48 → 64 bytes)

```cpp
struct FGPUCollisionFeedback
{
    // Row 1 (16 bytes)
    int32 ParticleIndex;      // 충돌한 파티클 인덱스
    int32 ColliderIndex;      // 충돌한 콜라이더 인덱스
    int32 ColliderType;       // 콜라이더 타입
    float Density;            // 파티클 밀도

    // Row 2 (16 bytes)
    FVector3f ImpactNormal;   // 충돌 노말
    float Penetration;        // 관통 깊이

    // Row 3 (16 bytes)
    FVector3f ParticleVelocity; // 파티클 속도
    int32 ColliderOwnerID;      // 콜라이더 소유자 ID

    // Row 4 (16 bytes) - NEW
    int32 ParticleSourceID;     // 파티클 SourceID (Preset + Component)
    int32 ParticleActorID;      // 파티클 소속 액터 ID (선택)
    int32 Reserved1;
    int32 Reserved2;
};

static_assert(sizeof(FGPUCollisionFeedback) == 64, "Must be 64 bytes");
```

---

## 5. 소스 등록 시스템

### 5.1 UFluidSourceRegistry

```cpp
UCLASS()
class KAWAIIFLUIDRUNTIME_API UFluidSourceRegistry : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxPresets = 65536;
    static constexpr int32 MaxComponents = 65536;
    static constexpr uint16 InvalidIndex = 0xFFFF;

    // Preset 등록/조회
    uint16 RegisterPreset(UKawaiiFluidPresetDataAsset* Preset);
    void UnregisterPreset(UKawaiiFluidPresetDataAsset* Preset);
    UKawaiiFluidPresetDataAsset* GetPreset(uint16 Index) const;
    uint16 FindPresetIndex(UKawaiiFluidPresetDataAsset* Preset) const;

    // Component 등록/조회
    uint16 RegisterComponent(UKawaiiFluidComponent* Component);
    void UnregisterComponent(UKawaiiFluidComponent* Component);
    UKawaiiFluidComponent* GetComponent(uint16 Index) const;
    uint16 FindComponentIndex(UKawaiiFluidComponent* Component) const;

private:
    struct FSlot
    {
        TWeakObjectPtr<UObject> Object;
        int32 RefCount = 0;
        bool IsOccupied() const { return RefCount > 0 && Object.IsValid(); }
    };

    TArray<FSlot> PresetSlots;
    TArray<FSlot> ComponentSlots;
    TArray<uint16> FreePresetIndices;
    TArray<uint16> FreeComponentIndices;
    mutable FCriticalSection CriticalSection;
};
```

---

## 6. 스폰 요청 구조체

### 6.1 FGPUSpawnRequest 확장 (32 → 48 bytes)

```cpp
struct FGPUSpawnRequest
{
    FVector3f Position;       // 12 bytes
    float Radius;             // 4 bytes
    FVector3f Velocity;       // 12 bytes
    float Mass;               // 4 bytes  (total: 32)

    // NEW
    int32 SourceID;           // 4 bytes  (Preset + Component)
    int32 ActorID;            // 4 bytes
    int32 Reserved1;          // 4 bytes
    int32 Reserved2;          // 4 bytes  (total: 48)
};

static_assert(sizeof(FGPUSpawnRequest) == 48, "Must be 48 bytes");
```

---

## 7. 셰이더 수정

### 7.1 FluidSpawnParticles.usf

```hlsl
[numthreads(64, 1, 1)]
void SpawnParticlesCS(uint3 DispatchThreadId : SV_DispatchThreadID)
{
    uint requestIdx = DispatchThreadId.x;
    if (requestIdx >= (uint)SpawnRequestCount)
        return;

    FGPUSpawnRequest request = SpawnRequests[requestIdx];

    // Atomic allocate
    uint newIdx;
    InterlockedAdd(ParticleCounter[0], 1, newIdx);
    if (newIdx >= (uint)MaxParticleCount)
    {
        InterlockedAdd(ParticleCounter[0], -1, newIdx);
        return;
    }

    FGPUFluidParticle particle = (FGPUFluidParticle)0;
    particle.Position = request.Position;
    particle.PredictedPosition = request.Position;
    particle.Velocity = request.Velocity;
    particle.Mass = request.Mass > 0 ? request.Mass : DefaultMass;
    particle.ParticleID = NextParticleID + requestIdx;
    particle.SourceID = request.SourceID;  // NEW

    Particles[newIdx] = particle;
}
```

### 7.2 FluidPrimitiveCollision.usf

```hlsl
void RecordCollisionFeedback(
    uint particleIdx,
    int colliderIdx,
    int colliderType,
    FGPUFluidParticle particle,
    float3 normal,
    float penetration,
    int ownerID)
{
    if (bEnableCollisionFeedback == 0)
        return;

    uint feedbackIdx;
    InterlockedAdd(CollisionCounter[0], 1, feedbackIdx);

    if ((int)feedbackIdx < MaxCollisionFeedback)
    {
        FGPUCollisionFeedback feedback;
        feedback.ParticleIndex = particleIdx;
        feedback.ColliderIndex = colliderIdx;
        feedback.ColliderType = colliderType;
        feedback.Density = particle.Density;
        feedback.ImpactNormal = normal;
        feedback.Penetration = penetration;
        feedback.ParticleVelocity = particle.Velocity;
        feedback.ColliderOwnerID = ownerID;
        feedback.ParticleSourceID = particle.SourceID;  // NEW
        feedback.ParticleActorID = 0;
        feedback.Reserved1 = 0;
        feedback.Reserved2 = 0;

        CollisionFeedback[feedbackIdx] = feedback;
    }
}
```

---

## 8. 이벤트 시스템

### 8.1 델리게이트

```cpp
USTRUCT(BlueprintType)
struct FFluidCollisionEventData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UKawaiiFluidPresetDataAsset> SourcePreset;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UKawaiiFluidComponent> SourceComponent;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AActor> HitActor;

    UPROPERTY(BlueprintReadOnly)
    FVector ImpactNormal;

    UPROPERTY(BlueprintReadOnly)
    FVector AverageVelocity;

    UPROPERTY(BlueprintReadOnly)
    float TotalPenetration;

    UPROPERTY(BlueprintReadOnly)
    int32 ParticleCount;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnFluidCollisionEvent,
    const FFluidCollisionEventData&, EventData);
```

---

## 9. 구현 순서

| Phase | 태스크 | 파일 |
|-------|--------|------|
| 1 | ClusterID → SourceID 리네임 | GPUFluidParticle.h |
| 2 | C++ 헬퍼 함수 추가 | GPUFluidParticle.h |
| 3 | HLSL 구조체/헬퍼 동기화 | FluidGPUPhysics.ush |
| 4 | FGPUCollisionFeedback 확장 | GPUFluidParticle.h, FluidGPUPhysics.ush |
| 5 | UFluidSourceRegistry 생성 | FluidSourceRegistry.h/cpp |
| 6 | FGPUSpawnRequest 확장 | GPUFluidParticle.h |
| 7 | FluidSpawnParticles.usf 수정 | FluidSpawnParticles.usf |
| 8 | FluidPrimitiveCollision.usf 수정 | FluidPrimitiveCollision.usf |
| 9 | Component 연동 | KawaiiFluidComponent.cpp |
| 10 | 이벤트 프로세서 구현 | FluidCollisionEventProcessor.h/cpp |
| 11 | 빌드 & 테스트 | - |

---

## 10. 확장성

현재 설계:
- **최대 Preset**: 65,536개 (16-bit)
- **최대 Component**: 65,536개 (16-bit)

추가 확장이 필요한 경우:
- Option C (구조체 80 bytes 확장)로 전환
- 또는 별도 SourceInfo 버퍼 도입 (파티클당 추가 데이터)

---

*Last Updated: 2026-01-11*
