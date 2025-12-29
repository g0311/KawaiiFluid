# 유체 렌더링 Q&A 세션 노트

**날짜**: 2024-12-29
**주제**: GPU 유체 시뮬레이션, CPU-GPU 동기화, SSFR vs Jumpflood SDF

---

## 1. Niagara Fluids 유체 표현 기법

### Q: Niagara Fluids에서 사용하는 유체 표현 기법과 GPU 지연 처리 방법은?

**핵심 유체 시뮬레이션 기법:**
- **FLIP (Fluid Implicit Particle) Solver**: 2D/3D 물 시뮬레이션
- PIC/FLIP 비율 0.75~0.95가 최적
- Shallow Water 구현으로 얕은 물 표현

**SDF 기반 표면 렌더링:**
- Signed Distance Field + **Jumpflood Renderer**로 물 표면 생성
- "Single Layer Water" 셰이딩 모델 + Pixel Depth Offset

**GPU 지연 핸들링:**
- `Num Cells Max Axis`로 복셀 크기/해상도 제어
- Standalone Play 모드에서 프로파일링 필수
- 스케일러빌리티 설정 분리 (파티클 수, 그리드 해상도)

**렌더링 비용 절감:**
| 기법 | 설명 |
|------|------|
| Screenspace Rasterization | 폼 강도를 렌더 타겟에 래스터화 후 밉맵 블러 |
| Dithered Masked Flipbook | 반투명 대신 디더링 마스크 (비용↓) |
| Sphere Rasterization | 파티클 속성을 스크린스페이스로 추출 |

---

## 2. CPU-GPU 동기화

### Q: CPU에서 캐릭터 입력을 받는데, GPU 물리는 어떻게 지연 없이 처리되나?

**핵심: 단방향 데이터 흐름 (One-Way Communication)**

```
CPU → GPU (매 프레임)
├── 캐릭터 위치/속도
├── 충돌체 Transform
└── 입력 이벤트

GPU → CPU (거의 없음)
└── Readback이 없어서 빠름
```

**지연 처리 기법:**
1. **1프레임 지연 허용**: 유체는 시각적 효과라 1프레임 차이 인지 어려움
2. **SDF 기반 충돌**: GPU에서 직접 충돌 판정, CPU 대기 불필요
3. **Async Compute**: 렌더링과 물리 계산 병렬 실행

---

## 3. Physics Asset을 통한 캐릭터-유체 상호작용

### Q: 플레이어 SkeletalMesh가 움직일 때 GPU 유체 입자는 어떻게 영향받나?

**Physics Asset Data Interface 방식:**

```
[CPU] SkeletalMesh 애니메이션
         ↓
[CPU] Physics Asset의 Collision Primitive 추출 (Capsule, Sphere, Box)
         ↓
[GPU Buffer] 프리미티브 Transform 업로드 (~1.5KB/캐릭터)
         ↓
[GPU] Niagara에서 SDF로 변환 → 입자 충돌
```

**왜 전체 메시가 아닌 Physics Asset인가?**
| 방식 | 장점 | 단점 |
|------|------|------|
| **Physics Asset** | 빠름, 데이터 작음 | 정확도↓ |
| Mesh SDF 실시간 생성 | 정확함 | **너무 느림** |

**핵심**: CPU→GPU 단방향이라 Readback 없음 → 0~1프레임 지연

---

## 4. 양방향 물리 구현 방법

### Q: 유체가 캐릭터를 밀어내는 물리를 구현하고 싶다면?

**방법 1: 샘플링 포인트 Readback**
- 캐릭터 위치 주변 속도/압력 샘플링 (5-10개 포인트)
- 비동기 Readback (2-3프레임 지연)
- 적은 데이터량, 급격한 변화에 부정확

**방법 2: CPU 유체 프록시 (하이브리드)**
- GPU: 고해상도 유체 (비주얼용)
- CPU: 저해상도 유체 (물리용, 16x16x16)
- 지연 없음, CPU 부하 증가

**방법 3: 예측 + 보정**
- 유체 힘 예측 → 즉시 적용
- Readback 도착 시 부드럽게 보정
- 즉각 반응 + 정확도, 구현 복잡

**현실적 추천:**
| 상황 | 추천 방법 |
|------|-----------|
| 강한 물살에 밀려남 | Readback + 지연 허용 |
| 물속 이동 저항 | CPU 프록시 |
| 폭포/파도 충격 | 예측+보정 |

---

## 5. 하이브리드 CPU-GPU 유체 시뮬레이션

### Q: GPU에서 물리 계산하다가 플레이어 근처만 CPU로 돌리면?

**개념:**
```
[GPU 영역] ●●●●●┌────────┐●●●●●
           ●●●●●│ CPU    │●●●●●
           ●●●●●│ 영역 🧍 │●●●●●
           ●●●●●└────────┘●●●●●
```

**문제점:**
1. **경계 동기화**: 양쪽 다 Readback 필요 → 원래 문제로 회귀
2. **파티클 전환 비용**: 매 프레임 수천 개 전환 가능
3. **시뮬레이션 불일치**: GPU(FLIP) vs CPU(다른 solver) → 경계에서 튀거나 폭발

**해결책:**
- Ghost Particle: 경계 파티클을 양쪽에 복사
- 비동기 + 보간: 2프레임 지연이지만 영역이 작아서 눈에 덜 띔
- 단방향 경계: GPU→CPU 허용, CPU→GPU 무시

---

## 6. 저해상도 CPU 유체 프록시

### Q: 플레이어 주변만 저해상도 CPU 유체로 처리하는 방법은?

```
┌─────────────────────────────────────────┐
│  GPU 고해상도 유체 (100,000+ 파티클)     │
│      ┌───────────┐                      │
│      │ CPU 유체  │  (500~2000 파티클)   │
│      │ ○ 🧍 ○ ○ │                      │
│      └───────────┘                      │
└─────────────────────────────────────────┘
```

**동기화 방법:**
1. GPU가 CPU 영역을 **구멍처럼 비워둠**
2. CPU 유체가 그 자리를 채움
3. 블렌딩 영역에서 알파 페이드

**핵심**: 두 시뮬레이션이 물리적으로 연결될 필요 없음. 시각적으로만 하나처럼 보이면 됨.

---

## 7. 블렌딩 영역 끊김 해결

### Q: CPU-GPU 블렌딩 영역에서 끊겨 보이는 건 어떻게 처리?

**해결 기법:**

| 순위 | 기법 | 효과 | 비용 |
|------|------|------|------|
| 1 | 밀도 매칭 | ★★★★★ | 낮음 |
| 2 | 속도 매칭 | ★★★★☆ | 낮음 |
| 3 | 노이즈 경계 | ★★★★☆ | 낮음 |
| 4 | 알파 페이드 | ★★★☆☆ | 낮음 |
| 5 | 포스트 블러 | ★★★☆☆ | 중간 |

**노이즈 경계 예시:**
```
직선 경계 (눈에 띔):
●●●●●●●●│○○○○○○○○

노이즈 경계 (자연스러움):
●●●●●●●○●○○○●○○○○○
```

---

## 8. SSFR vs Niagara 렌더링

### Q: 현재 SSFR을 Niagara로 바꾸는 게 나아?

**현재 시스템 (KawaiiFluid):**
- CPU SPH 시뮬레이션
- SSFR 메타볼 렌더링
- Niagara Data Interface 연동 가능
- 양방향 물리 가능

**결론: 바꾸지 마세요**
- 이미 핵심 기능 구현됨
- 양방향 물리 필요하면 CPU가 맞음
- 슬라임 특화 (KawaiiSlimeComponent)

---

## 9. Bilateral Filter 한계

### Q: Bilateral Filter를 써도 가까이 가면 각 입자가 보이는데?

**원인:**
```
[멀리서]           [가까이서]
●●●●●●●●          ●     ●     ●
스크린에서 겹침    스크린에서 간격 넓어짐
Blur가 합쳐줌     Blur가 못 합침
```

**Bilateral Filter의 한계:**
- Sigma가 고정 → 가까이서는 커버 못 함
- 파티클 간격 > 필터 커버리지 → 합쳐지지 않음

**해결책들:**
| 방식 | 가까이서 품질 | 성능 |
|------|--------------|------|
| Bilateral (현재) | ❌ 입자 보임 | 중간 |
| 동적 Sigma | △ 개선 | 중간 |
| **Jumpflood SDF** | ◎ 매우 좋음 | **빠름** |

---

## 10. SSFR vs Jumpflood SDF 비교

### Q: Jumpflood가 구현된 프로젝트와 SSFR 차이는?

**알고리즘 비교:**

```
SSFR (Bilateral):
[파티클] → [깊이] → [Bilateral Blur] → [노말] → [합성]
                         ↑
               "픽셀 주변을 블러로 합침"

Jumpflood SDF:
[파티클] → [시드 초기화] → [JFA log(N) 패스] → [SDF] → [노말] → [합성]
                                   ↑
               "각 픽셀에서 가장 가까운 파티클 찾기"
```

**성능 비교:**
| 항목 | SSFR (Bilateral) | Jumpflood SDF |
|------|------------------|---------------|
| 1024p 샘플/픽셀 | 81 × 5 = **405** | 9 × 10 = **90** |
| 카메라 거리 | 가까우면 품질↓ | **영향 없음** |
| 파티클 밀도 | 민감 | 무관 |

**실제 구현 프로젝트:**
| 프로젝트 | 방식 |
|----------|------|
| Niagara Fluids (Epic) | SDF + Jumpflood |
| NiagaraFluid (mushe) | SSFR |
| Asher Zhu 슬라임 | SDF |

---

## 11. Jumpflood와 물리 독립성

### Q: Jumpflood로 전환해도 점성, 부착 등 물리적 적용은 정상 처리되나?

**예, 물리는 전혀 영향받지 않습니다.**

```
┌─────────────────────┐      ┌─────────────────────┐
│ [물리] CPU          │      │ [렌더링] GPU        │
│ SimulationModule    │ ───→ │ RenderingModule     │
│                     │파티클│                     │
│ • Adhesion (부착)   │위치  │ • Smoothing Pass ←변경
│ • Viscosity (점성)  │배열  │                     │
│ • Surface Tension   │      │                     │
└─────────────────────┘      └─────────────────────┘
         ↑                            ↑
     영향 없음                   여기만 변경
```

**Jumpflood는 순수하게 "파티클을 어떻게 예쁘게 그리느냐"의 문제**

---

## 12. Jumpflood 적용 범위

### Q: Jumpflood가 다른 점성유체에도 적용 가능한가?

**예, 모든 파티클 기반 유체에 적용 가능합니다.**

| 유체 | 물리 특성 | Jumpflood 적용 |
|------|-----------|----------------|
| 물 | 낮은 점성 | ✅ |
| 슬라임 | 높은 점성 + Shape Matching | ✅ |
| 꿀 | 매우 높은 점성 | ✅ |
| 용암 | 점성 + 열 | ✅ |

**이유**: Jumpflood는 파티클 위치만 필요. 물리가 뭐든 상관없음.

---

## 참고 자료

- [Asher.gg - Darkhold of Niagara](https://asher.gg/darkhold-of-niagara/)
- [Jump Flooding Algorithm (demofox)](https://blog.demofox.org/2016/02/29/fast-voronoi-diagrams-and-distance-dield-textures-on-the-gpu-with-the-jump-flooding-algorithm/)
- [NiagaraFluid GitHub](https://github.com/mushe/NiagaraFluid)
- [Niagara Fluids Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-fluids-in-unreal-engine)
- [Screen Space Fluid Rendering (NVIDIA GDC)](https://developer.download.nvidia.com/presentations/2010/gdc/Direct3D_Effects.pdf)
