# Ray Marching SDF 렌더링 및 최적화 Q&A

## 세션 날짜: 2025-12-31

---

## Q1: Ray Marching + SDF 슬라임 렌더링은 포스트 프로세싱 Tonemap 과정에서 발동되는가?

**A: 예, 맞습니다.**

Ray Marching + SDF 슬라임 렌더링은 **Tonemap 포스트프로세싱 패스**에서 실행됩니다.

### 핵심 코드 위치

`FluidSceneViewExtension.cpp`에서 Tonemap 패스에 구독:

```cpp
void FFluidSceneViewExtension::SubscribeToPostProcessingPass(
    EPostProcessingPass Pass, ...)
{
    if (Pass == EPostProcessingPass::Tonemap)  // 여기서 훅
    {
        InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateLambda(...));
    }
}
```

### 렌더링 순서

```
Unreal Deferred Rendering
    ↓
BasePass (GBuffer)
    ↓
Lighting Pass
    ↓
Translucency Pass
    ↓
Post Processing
    ├─ MotionBlur
    ├─ Tonemap ◀━━ Ray Marching SDF 렌더링 삽입 지점
    ├─ FXAA / TAA
    └─ UI
```

### 이유

- **Scene Color 완성 후** - 모든 오브젝트가 렌더링된 후 유체를 블렌딩
- **굴절 구현** - 완성된 Scene Color 텍스처를 샘플링하여 굴절된 배경색 계산 가능
- **Depth Occlusion** - Scene Depth 텍스처와 비교하여 유체가 오브젝트 뒤에 있을 때 가려짐 처리

---

## Q2: 메타볼 형태로 먼저 렌더링하고 PP에서 보정하는 개념인가?

**A: 아니요, 다릅니다.**

### Ray Marching 모드

**"먼저 렌더링 → PP 보정"이 아니라, PP에서 직접 Ray Marching으로 메타볼을 렌더링합니다.**

```
┌─────────────────────────────────────────────────────┐
│  Tonemap Pass (PP)                                  │
│                                                     │
│  1. 모든 픽셀에서 카메라 레이 생성                   │
│  2. 레이를 따라 SDF 실시간 평가 (Sphere Tracing)    │
│  3. 메타볼 표면 발견 시 Shading 계산                │
│  4. Scene Color에 Alpha Blend                       │
│                                                     │
│  ※ 사전 렌더링 없이 PP에서 모든 것을 처리           │
└─────────────────────────────────────────────────────┘
```

### Custom 모드 (SSFR)는 다름

```
1. Depth Pass     → 파티클을 스플랫으로 렌더링 (깊이 맵)
2. Smoothing Pass → Bilateral Filter로 스무딩 (보정)
3. Normal Pass    → 스무딩된 깊이에서 법선 복원
4. Thickness Pass → 두께 계산
5. Composite Pass → 최종 합성
```

이건 "먼저 대충 렌더링 → 여러 패스로 보정"하는 방식.

| 모드 | 방식 |
|------|------|
| **Ray Marching** | PP에서 **직접** SDF 계산하여 메타볼 렌더링 (사전 렌더링 없음) |
| **Custom (SSFR)** | 스플랫 렌더링 → 스무딩 → 법선 복원 (다중 패스 보정) |

---

## Q3: 입자의 정보를 PP에서 어떻게 처리하는가?

**A: 파티클 위치 데이터를 GPU 버퍼로 업로드하고 셰이더에서 접근합니다.**

### 파티클 데이터 흐름

```
┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│   Game Thread    │     │  Render Thread   │     │       GPU        │
│   (시뮬레이션)    │     │    (캐시)        │     │    (셰이더)      │
├──────────────────┤     ├──────────────────┤     ├──────────────────┤
│                  │     │                  │     │                  │
│  FFluidParticle  │────▶│  FVector3f[]     │────▶│ StructuredBuffer │
│  - Position      │     │  (위치만 추출)   │     │  <float3>        │
│  - Velocity      │     │                  │     │                  │
│  - etc...        │     │                  │     │  ParticlePositions│
└──────────────────┘     └──────────────────┘     └──────────────────┘
      물리 계산            UpdateGPUResources       셰이더에서 접근
```

### 단계별 처리

#### 1. 시뮬레이션 (Game Thread)
물리 엔진이 파티클 위치 계산

#### 2. 캐시 (KawaiiFluidSSFRRenderer.cpp)
```cpp
void UKawaiiFluidSSFRRenderer::UpdateGPUResources(...)
{
    for (파티클들)
    {
        RenderParticlesCache.Add(파티클.Position);  // 위치만 추출
    }
}
```

#### 3. GPU 업로드 (FluidSceneViewExtension.cpp)
```cpp
// StructuredBuffer 생성
FRDGBufferRef ParticleBuffer = GraphBuilder.CreateBuffer(
    FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), ParticleCount));

// CPU → GPU 복사
GraphBuilder.QueueBufferUpload(ParticleBuffer, AllParticlePositions.GetData(), ...);

// SRV 생성 (셰이더에서 읽을 수 있게)
FRDGBufferSRVRef ParticleBufferSRV = GraphBuilder.CreateSRV(ParticleBuffer);
```

#### 4. 셰이더에서 접근 (FluidRayMarching.usf)
```hlsl
StructuredBuffer<float3> ParticlePositions;  // GPU 버퍼
int ParticleCount;
float ParticleRadius;

float EvaluateMetaballSDF(float3 p)
{
    float sdf = sdSphere(p, ParticlePositions[0], ParticleRadius);

    for (int i = 1; i < ParticleCount; ++i)
    {
        float d = sdSphere(p, ParticlePositions[i], ParticleRadius);
        sdf = smin(sdf, d, Smoothness);  // 부드럽게 블렌딩
    }
    return sdf;
}
```

---

## Q4: 600개 파티클을 매 프레임마다 보내고 있는가?

**A: 맞습니다. 하지만 데이터 전송은 문제가 아닙니다.**

### 데이터 전송량

```
600 파티클 × 12 bytes (FVector3f) = 7.2 KB / 프레임
```

PCIe 대역폭 대비 무시할 수준.

### 진짜 문제는 셰이더 연산량

```
매 픽셀마다 600개 파티클 전부 순회

1920 × 1080 픽셀 × 128 스텝 × 600 파티클
= 약 1,592억 번의 거리 계산 / 프레임
```

```hlsl
// 이게 매 픽셀, 매 스텝마다 실행됨
for (int i = 0; i < 600; ++i)  // ◀━━ 여기가 병목
{
    float d = sdSphere(p, ParticlePositions[i], radius);
    sdf = smin(sdf, d, smoothness);
}
```

| 항목 | 부담 |
|------|------|
| 데이터 전송 (7KB) | 거의 없음 ✅ |
| 셰이더 연산 (O(픽셀×스텝×파티클)) | **매우 큼** ❌ |

---

## Q5: 화면이 클수록 픽셀마다 600번 계산하니까 문제인가?

**A: 맞습니다.**

### 해상도별 연산량 (600 파티클 기준)

| 해상도 | 픽셀 수 | × 128 스텝 × 600 파티클 |
|--------|---------|------------------------|
| 1280×720 (HD) | 92만 | **710억** 연산 |
| 1920×1080 (FHD) | 207만 | **1,592억** 연산 |
| 2560×1440 (QHD) | 368만 | **2,831억** 연산 |
| 3840×2160 (4K) | 829만 | **6,367억** 연산 |

### 구조

```
화면의 모든 픽셀
       ↓
    각 픽셀마다
       ↓
   Ray March 루프 (최대 128번)
       ↓
    매 스텝마다
       ↓
   600개 파티클 전부 순회  ◀━━ 병목
```

해상도 2배 → 픽셀 4배 → 연산량 4배

---

## Q6: 최적화 방법은?

### 1. 공간 분할 (가장 효과적)
```
현재: 모든 픽셀에서 600개 전부 순회 → O(N)
개선: 그리드/옥트리로 근처 파티클만 검사 → O(1) ~ O(log N)
```

### 2. 저해상도 렌더링 + 업스케일
```
1/2 해상도로 Ray Marching → Bilateral Upscale
픽셀 수 1/4 → 연산량 1/4
```

### 3. Bounding Volume 조기 기각
```hlsl
// 슬라임 영역 밖이면 바로 스킵
if (!RayIntersectsAABB(ray, slimeBounds))
    discard;
```

### 4. 적응형 스텝 수
```hlsl
// 거리에 따라 스텝 수 조절
int steps = (distance < 500) ? 128 : 64;
```

### 5. 3D 텍스처에 SDF 베이크 (가장 빠름)
```
Compute Shader로 SDF 볼륨 텍스처 생성 (64³ ~ 128³)
Ray Marching에서는 텍스처 샘플링만 → O(1)
```

### 6. 표면 파티클만 렌더링
```cpp
bRenderSurfaceOnly = true;  // 내부 파티클 제외
```

### 효과 비교

| 방법 | 구현 난이도 | 효과 |
|------|------------|------|
| 공간 분할 그리드 | 중 | ⭐⭐⭐⭐ |
| 저해상도 + 업스케일 | 하 | ⭐⭐⭐ |
| Bounding Volume | 하 | ⭐⭐ |
| 3D SDF 텍스처 | 상 | ⭐⭐⭐⭐⭐ |
| 표면 파티클만 | 하 | ⭐⭐ |

---

## Q7: bRenderSurfaceOnly는 이미 적용되어 있는가?

**A: 옵션은 존재하지만 기본값이 꺼져있습니다.**

```cpp
// KawaiiFluidRendererSettings.h
bool bRenderSurfaceOnly = false;  // ◀━━ 기본값 false
```

```cpp
// KawaiiFluidSSFRRenderer.cpp
if (!bRenderSurfaceOnly || Particles[i].bIsSurfaceParticle)
{
    // 렌더링에 추가
}
```

### 현재 상태

| 설정 | 동작 |
|------|------|
| `false` (기본값) | 600개 **전부** 렌더링 |
| `true` | 표면 파티클만 렌더링 (내부 제외) |

### 활성화 방법

```cpp
SSFRSettings.bRenderSurfaceOnly = true;
```

---

## Q8: 3D 텍스처에 SDF 베이크 방법 자세히 설명

### 2D 텍스처 vs 3D 텍스처

```
2D 텍스처 (일반 이미지)
┌─────────────┐
│ ■ ■ ■ ■ ■ │  → 가로(X) × 세로(Y)
│ ■ ■ ■ ■ ■ │  → 예: 1024 × 1024 픽셀
│ ■ ■ ■ ■ ■ │  → UV 좌표로 샘플링
└─────────────┘

3D 텍스처 (볼륨)
    ┌─────────────┐
   ╱│            ╱│
  ╱ │           ╱ │   → 가로(X) × 세로(Y) × 깊이(Z)
 ┌─────────────┐  │   → 예: 64 × 64 × 64 복셀
 │  │          │  │   → UVW 좌표로 샘플링
 │  └──────────│──┘
 │ ╱           │ ╱    → 3차원 공간의 각 점에 값 저장
 │╱            │╱
 └─────────────┘
```

### 비유

```
2D 텍스처 = 종이 한 장 (그림)
3D 텍스처 = 젤리 큐브 (내부에도 데이터 있음)

┌─────────────┐
│  젤리 큐브   │  ← 64×64×64 = 262,144개의 작은 칸
│  각 칸마다   │  ← 각 칸에 SDF 거리값 저장
│  숫자 저장   │
└─────────────┘
```

### 구현 단계

#### 1단계: 3D 텍스처 생성
```cpp
FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create3D(
    TEXT("SDFVolumeTexture"),
    64, 64, 64,           // 해상도 (64³ = 262,144 복셀)
    PF_R16F               // 16bit float (SDF 거리값)
);
Desc.SetFlags(TexCreate_ShaderResource | TexCreate_UAV);
SDFVolumeTexture = RHICreateTexture(Desc);
```

#### 2단계: Compute Shader로 SDF 베이크
```hlsl
RWTexture3D<float> SDFVolume;              // 출력 (쓰기)
StructuredBuffer<float3> ParticlePositions; // 입력
int ParticleCount;
float ParticleRadius;
float Smoothness;

float3 VolumeMin;      // 볼륨 월드 좌표 최소
float3 VolumeMax;      // 볼륨 월드 좌표 최대
int3 VolumeResolution; // (64, 64, 64)

[numthreads(8, 8, 8)]
void MainCS(uint3 DispatchThreadId : SV_DispatchThreadID)
{
    // 1. 복셀 좌표 → 월드 좌표 변환
    float3 uvw = (float3(DispatchThreadId) + 0.5) / float3(VolumeResolution);
    float3 worldPos = lerp(VolumeMin, VolumeMax, uvw);

    // 2. 이 위치에서 SDF 계산 (파티클 전부 순회)
    float sdf = sdSphere(worldPos, ParticlePositions[0], ParticleRadius);

    for (int i = 1; i < ParticleCount; ++i)
    {
        float d = sdSphere(worldPos, ParticlePositions[i], ParticleRadius);
        sdf = smin(sdf, d, Smoothness);
    }

    // 3. 텍스처에 저장
    SDFVolume[DispatchThreadId] = sdf;
}
```

#### 3단계: Ray Marching에서 샘플링
```hlsl
Texture3D<float> SDFVolume;      // 베이크된 SDF
SamplerState SDFSampler;         // Trilinear 샘플러

float SampleSDF(float3 worldPos)
{
    float3 uvw = (worldPos - VolumeMin) / (VolumeMax - VolumeMin);

    if (any(uvw < 0) || any(uvw > 1))
        return 1000.0;

    return SDFVolume.SampleLevel(SDFSampler, uvw, 0);  // O(1)
}

for (int i = 0; i < MaxSteps; ++i)
{
    float3 p = ro + rd * t;
    float sdf = SampleSDF(p);  // 파티클 순회 없음!

    if (sdf < threshold) { /* hit */ }
    t += sdf;
}
```

### 연산량 비교

| 방식 | 연산 위치 | 복잡도 |
|------|----------|--------|
| **현재** | 픽셀 셰이더 | 207만 픽셀 × 128 스텝 × 600 파티클 |
| **베이크** | Compute (1회) | 64³ 복셀 × 600 파티클 = **1.57억** |
| | 픽셀 셰이더 | 207만 픽셀 × 128 스텝 × **1 샘플링** |

```
현재:    1,592억 연산
베이크:  1.57억 (Compute) + 2.65억 (샘플링) ≈ 4억 연산

약 400배 빠름
```

---

## Q9: 복셀 위치/범위, 저장값, 사용 방법?

### 1. 복셀 위치와 범위

슬라임 파티클들이 이런 위치에 있다고 가정:
```
파티클 A: (100, 100, 100)
파티클 B: (120, 100, 100)
파티클 C: (110, 120, 100)
```

바운딩 박스 계산:
```
최소 좌표: (100, 100, 100)
최대 좌표: (120, 120, 100)

+ 여유 마진 20cm 추가

최종 볼륨 범위:
  Min: (80, 80, 80)
  Max: (140, 140, 120)
```

4×4×4로 나눈다면:
```
60cm ÷ 4 = 15cm 간격

각 칸 = 15cm × 15cm × 10cm 크기의 복셀
```

### 2. 각 복셀에 뭘 저장하나

복셀 [2,1]의 중심 위치 (117.5, 102.5, 85)에서:

```
파티클 A까지 SDF = 거리 - 반지름 = 23.2 - 10 = +13.2
파티클 B까지 SDF = 15.4 - 10 = +5.4
파티클 C까지 SDF = 24.2 - 10 = +14.2

Smooth Minimum 블렌딩 → 최종값: +4.8

복셀 [2,1]에 저장: +4.8
의미: "이 위치에서 슬라임 표면까지 약 4.8cm (바깥쪽)"
```

| 값 | 의미 |
|----|------|
| 양수 | 슬라임 바깥 |
| 0 | 슬라임 표면 |
| 음수 | 슬라임 내부 |

### 3. 이 값을 어떻게 쓰나

```
레이 진행 방향 →

위치:    A         B         C         D
SDF값:  +20       +8        +2        +0.01
         │         │         │         │
         ▼         ▼         ▼         ▼
      "멀다"    "좀 가깝"  "거의 다 옴" "표면이다!"
      20cm 전진  8cm 전진   2cm 전진    렌더링!
```

---

## Q10: 슬라임이 움직이면 다시 계산해야 하지 않나?

**A: 맞습니다. 매 프레임 다시 계산합니다. 그래도 훨씬 빠릅니다.**

### 연산 위치가 다름

#### 현재 방식 (픽셀 셰이더에서 계산)
```
매 픽셀 × 매 스텝 × 매 파티클
= 207만 × 128 × 600
= 1,592억 연산
```

#### 베이크 방식 (Compute Shader에서 미리 계산)
```
Compute: 64³ 복셀 × 600 파티클 = 1.57억 연산 (1회)
픽셀:    207만 × 128 × 1 샘플링 = 2.65억 연산

총 = 약 4억 연산
```

### 왜 차이가 나냐면

```
현재 방식:
  픽셀 100만개가 각자 600개 파티클 순회
  = 똑같은 계산을 100만 번 중복

베이크 방식:
  Compute가 26만 복셀만 1번 계산
  픽셀들은 그 결과를 그냥 읽기만 함
  = 중복 계산 없음
```

**핵심: 파티클 순회를 "한 번만" 하고, 결과를 공유하는 것**

---

## Q11: 이제 픽셀에서 파티클 순회 대신 텍스처 샘플링만 하는 건가?

**A: 맞습니다!**

### 코드 비교

#### 현재
```hlsl
for (int step = 0; step < 128; step++)
{
    float3 pos = rayOrigin + rayDir * t;

    // 600번 반복 ❌
    float sdf = 99999;
    for (int i = 0; i < 600; i++)
    {
        float d = distance(pos, Particles[i]) - radius;
        sdf = smin(sdf, d);
    }

    t += sdf;
}
```

#### 베이크 후
```hlsl
for (int step = 0; step < 128; step++)
{
    float3 pos = rayOrigin + rayDir * t;

    // 텍스처 샘플링 1번 ✅
    float sdf = SDFTexture.Sample(pos);

    t += sdf;
}
```

| | 현재 | 베이크 |
|--|------|--------|
| 매 스텝마다 | 600번 계산 | 1번 샘플링 |
| 연산 | 거리 계산 + smin | 텍스처 읽기 |
| 속도 | 느림 | 빠름 |

---

## 전체 흐름 요약

```
┌─────────────────────────────────────────────────────────────────┐
│  COMPUTE SHADER (매 프레임 1회)                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. 파티클 600개 위치 데이터 받음                               │
│                    ↓                                            │
│  2. 최소/최대 좌표 계산 → 바운딩 박스 결정                      │
│                    ↓                                            │
│  3. 바운딩 박스를 N×N×N (예: 64³)으로 나눔                      │
│                    ↓                                            │
│  4. 각 복셀마다:                                                │
│     - 복셀 중심 좌표 계산                                       │
│     - 600개 파티클 검사                                         │
│     - 가장 가까운 표면까지 거리(SDF) 저장                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
                              ↓
                    3D 텍스처 완성 (64³개의 SDF 값)
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  PIXEL SHADER (Ray Marching)                                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  매 픽셀에서:                                                   │
│  - 레이 발사                                                    │
│  - 매 스텝마다 3D 텍스처에서 SDF 값 읽기 (파티클 순회 없음!)   │
│  - SDF ≈ 0 이면 표면 → 렌더링                                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 연산량 비교

| 단계 | 현재 | 베이크 |
|------|------|--------|
| Compute | 없음 | 64³ × 600 = **1.57억** |
| Pixel | 207만 × 128 × 600 = **1,592억** | 207만 × 128 × 1 = **2.65억** |
| **총합** | **1,592억** | **4억** |

---

## 파일 참조

- `FluidSceneViewExtension.cpp` - 렌더링 파이프라인 훅
- `FluidRayMarching.usf` - Ray Marching 셰이더
- `FluidSDFCommon.ush` - SDF 함수들
- `KawaiiFluidSSFRRenderer.cpp` - GPU 리소스 관리
- `KawaiiFluidRendererSettings.h` - bRenderSurfaceOnly 등 설정
