# Fluid Upscale System Documentation

## 개요

KawaiiFluid의 Upscale 시스템은 성능 최적화를 위해 저해상도로 렌더링된 유체를 원본 해상도로 업스케일하는 기능을 제공합니다. 단순한 바이리니어 업스케일이 아닌 **Depth-Aware Bilinear Upscaling**을 사용하여 유체 경계면에서 발생하는 할로(halo) 아티팩트를 방지합니다.

## 파일 구조

```
KawaiiFluidSystem/
├── Shaders/Private/
│   └── FluidUpscale.usf          # 업스케일 셰이더 (HLSL)
├── Source/KawaiiFluidRuntime/
│   ├── Public/Rendering/Shaders/
│   │   └── FluidRayMarchShaders.h    # 셰이더 파라미터 구조체
│   └── Private/Rendering/Shading/
│       └── KawaiiRayMarchShadingImpl.cpp  # 업스케일 패스 구현
```

## 아키텍처

### 2-Pass 렌더링 파이프라인

```
┌─────────────────────────────────────────────────────────────┐
│                    Pass 1: Ray Marching                     │
│                   (Scaled Resolution)                       │
├─────────────────────────────────────────────────────────────┤
│  Input:                                                     │
│    - Particle Buffer (SRV)                                  │
│    - SDF Volume Texture (optional)                          │
│    - Scene Depth/Color Textures                             │
│                                                             │
│  Output:                                                    │
│    - IntermediateTarget (Scaled Color, PF_FloatRGBA)        │
│    - ScaledDepthTexture (Scaled Depth, PF_R32_FLOAT)        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Pass 2: Upscale                          │
│                   (Full Resolution)                         │
├─────────────────────────────────────────────────────────────┤
│  Input:                                                     │
│    - IntermediateTarget (Scaled Color)                      │
│    - ScaledDepthTexture (Scaled Depth)                      │
│    - SceneDepthTexture (Full-res, optional)                 │
│                                                             │
│  Output:                                                    │
│    - Scene Texture (Alpha Blended)                          │
└─────────────────────────────────────────────────────────────┘
```

## 셰이더 구현 (FluidUpscale.usf)

### 입력 파라미터

| 파라미터 | 타입 | 설명 |
|---------|------|------|
| `InputTexture` | Texture2D<float4> | 스케일된 해상도의 유체 컬러 |
| `InputSampler` | SamplerState | Bilinear 샘플러 |
| `FluidDepthTexture` | Texture2D<float> | 스케일된 해상도의 유체 깊이 (DeviceZ) |
| `DepthSampler` | SamplerState | Point 샘플러 |
| `InputSize` | float2 | 스케일된 해상도 크기 |
| `OutputSize` | float2 | 원본 해상도 크기 |

### 알고리즘: Depth-Aware Bilinear Interpolation

```hlsl
// 1. 텍셀 위치 계산
float2 TexelPos = UV * InputSize - 0.5;
float2 Fraction = frac(TexelPos);
float2 BaseUV = (floor(TexelPos) + 0.5) * TexelSize;

// 2. 2x2 샘플링 (TL, TR, BL, BR)
for (int i = 0; i < 4; i++)
{
    Colors[i] = InputTexture.SampleLevel(InputSampler, SampleUV, 0);
    Depths[i] = FluidDepthTexture.SampleLevel(DepthSampler, SampleUV, 0);
}

// 3. 기준 깊이 결정 (카메라에 가장 가까운 값)
// Reversed-Z에서는 큰 값 = 가까움
float RefDepth = max(max(Depths[0], Depths[1]), max(Depths[2], Depths[3]));

// 4. Depth-Aware 가중치 계산
float DepthDiff = abs(RefDepth - Depths[i]);
float DepthWeight = 1.0 / (1.0 + DepthDiff * DepthSensitivity);

// 5. 최종 가중치 = Bilinear Weight × Depth Weight
float W = BilinearWeights[i] * DepthWeight;
```

### 가중치 계산 상세

**Bilinear Weights (위치 기반):**
```
TL: (1 - fx) * (1 - fy)
TR: (fx)     * (1 - fy)
BL: (1 - fx) * (fy)
BR: (fx)     * (fy)
```

**Depth Weights (깊이 유사도 기반):**
```
DepthWeight = 1 / (1 + |RefDepth - SampleDepth| * Sensitivity)
```

- `DepthSensitivity = 5000.0` (높을수록 깊이 차이에 민감)
- 깊이가 유사한 샘플은 높은 가중치
- 깊이가 크게 다른 샘플(경계면)은 낮은 가중치 → 할로 방지

### 예외 처리

1. **유체 없음**: 모든 샘플의 깊이가 0.0001 미만이면 단순 바이리니어 샘플링
2. **빈 샘플 스킵**: 개별 샘플의 깊이가 0.0001 미만이면 해당 샘플 제외
3. **Fallback**: 총 가중치가 0.001 미만이면 가장 가까운 유효 샘플 사용

## C++ 구현 (KawaiiRayMarchShadingImpl.cpp)

### 스케일 설정

```cpp
const float RenderScale = FMath::Clamp(RenderParams.RenderTargetScale, 0.25f, 1.0f);
const bool bUseScaledRes = RenderScale < 0.99f;

FIntPoint ScaledSize(
    FMath::Max(1, FMath::RoundToInt(FullViewRect.Width() * RenderScale)),
    FMath::Max(1, FMath::RoundToInt(FullViewRect.Height() * RenderScale))
);
```

- 스케일 범위: 0.25 ~ 1.0 (25% ~ 100%)
- 0.99 이상이면 업스케일 패스 스킵

### 텍스처 생성

```cpp
// 스케일된 컬러 텍스처
FRDGTextureDesc IntermediateDesc = FRDGTextureDesc::Create2D(
    ScaledSize,
    PF_FloatRGBA,
    FClearValueBinding::Transparent,
    TexCreate_RenderTargetable | TexCreate_ShaderResource
);
IntermediateTarget = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("FluidRayMarch_Scaled"));

// 스케일된 깊이 텍스처
FRDGTextureDesc ScaledDepthDesc = FRDGTextureDesc::Create2D(
    ScaledSize,
    PF_R32_FLOAT,
    FClearValueBinding::Black,
    TexCreate_RenderTargetable | TexCreate_ShaderResource
);
ScaledDepthTexture = GraphBuilder.CreateTexture(ScaledDepthDesc, TEXT("FluidRayMarch_ScaledDepth"));
```

### Upscale Pass 설정

```cpp
auto* UpscaleParameters = GraphBuilder.AllocParameters<FFluidUpscalePS::FParameters>();

// 입력 텍스처
UpscaleParameters->InputTexture = IntermediateTarget;
UpscaleParameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
UpscaleParameters->FluidDepthTexture = ScaledDepthTexture;
UpscaleParameters->DepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

// 해상도 정보
UpscaleParameters->InputSize = FVector2f(ScaledSize.X, ScaledSize.Y);
UpscaleParameters->OutputSize = FVector2f(FullViewRect.Width(), FullViewRect.Height());

// 출력: Scene에 Alpha Blend
UpscaleParameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, ERenderTargetLoadAction::ELoad);
```

### Alpha Blending 설정

```cpp
GraphicsPSOInit.BlendState = TStaticBlendState<
    CW_RGBA,
    BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,  // Color: src*a + dst*(1-a)
    BO_Add, BF_Zero, BF_One                          // Alpha: preserve dst
>::GetRHI();
```

## 그림자 깊이 텍스처 연동

스케일링 모드에서 그림자 생성을 위해 깊이 텍스처를 반환할 때:

```cpp
if (OutFluidDepthTexture)
{
    // 스케일링 모드면 ScaledDepthTexture 사용 (실제 depth가 출력되는 텍스처)
    *OutFluidDepthTexture = (bUseScaledRes && ScaledDepthTexture) ? ScaledDepthTexture : FluidDepthTexture;
}
```

**주의**: 스케일링 모드에서는 `FluidDepthTexture`가 생성되지만 렌더링되지 않음. 실제 깊이는 `ScaledDepthTexture`에 출력됨.

## 성능 고려사항

### 스케일 값에 따른 성능

| RenderTargetScale | 픽셀 비율 | 권장 사용처 |
|-------------------|----------|-------------|
| 1.0 | 100% | 고품질, 성능 여유 있을 때 |
| 0.75 | 56% | 균형 (권장) |
| 0.5 | 25% | 성능 우선 |
| 0.25 | 6% | 최소 품질 |

### 메모리 사용량

스케일링 모드 활성화 시 추가 텍스처:
- `FluidRayMarch_Scaled`: PF_FloatRGBA (8 bytes/pixel)
- `FluidRayMarch_ScaledDepth`: PF_R32_FLOAT (4 bytes/pixel)

예시 (1920x1080, Scale=0.5):
- Scaled Size: 960x540
- 추가 메모리: 960×540×(8+4) = ~6.2 MB

## 셰이더 파라미터 구조체 (FluidRayMarchShaders.h)

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FFluidUpscaleParameters, )
    // 스케일된 유체 컬러
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)

    // 스케일된 유체 깊이
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidDepthTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)

    // 씬 깊이 (옵션)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)

    // 해상도 정보
    SHADER_PARAMETER(FVector2f, InputSize)
    SHADER_PARAMETER(FVector2f, OutputSize)
    SHADER_PARAMETER(FVector2f, SceneDepthSize)

    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()
```

## 알려진 제한사항

1. **스케일 범위**: 0.25 미만은 지원하지 않음 (품질 저하 심함)
2. **SceneDepthTexture**: 현재 미사용 (향후 확장용으로 선언됨)
3. **깊이 민감도**: `DepthSensitivity = 5000.0` 하드코딩됨

## 향후 개선 가능 사항

1. `DepthSensitivity` 파라미터화하여 사용자 조절 가능하게
2. SceneDepthTexture를 활용한 Edge-Aware 업스케일 추가
3. Temporal Upscaling (TAA 스타일) 지원
4. FSR/DLSS 통합 검토
