# 슬라임 렌더링 연구 노트

## 현재 상태 (2024-12-29)

### 문제점
- 슬라임 표면이 균일하지 않음 (파티클 개별로 보임)
- SSFR로는 Asher Zhu 수준의 스무스한 표면 달성 어려움

### 현재 구현
- **물리**: CPU 기반 PBF (Position Based Fluids)
- **렌더링**: SSFR (Screen Space Fluid Rendering)
- **Shape Matching**: 형태 유지용

---

## 변경 사항

### 1. 균일 스폰 함수 추가
**파일**: `KawaiiSlimeSimulationModule.cpp`
```cpp
void SpawnParticlesUniform(FVector Location, int32 Count, float Radius)
```
- Fibonacci Sphere + 동심원 레이어
- r³ 가중치로 바깥 레이어에 더 많은 파티클 배치

### 2. RelaxSurfaceParticles 비활성화
**파일**: `KawaiiSlimeSimulationContext.cpp`
- Shape Matching과 충돌해서 진동 유발
- 주석 처리로 비활성화

### 3. bRenderSurfaceOnly 옵션 추가
**파일**: `KawaiiFluidSSFRRenderer.cpp`, `KawaiiFluidRendererSettings.h`
- 슬라임만 표면 파티클만 렌더링 가능
- 일반 유체는 영향 없음

### 4. BilateralFilterRadius ClampMax 확장
**파일**: `KawaiiFluidRendererSettings.h`
- 50 → 100으로 확장

---

## 핵심 파라미터

| 파라미터 | 현재값 | 권장값 | 설명 |
|----------|--------|--------|------|
| AutoSpawnRadius | 120 | - | 스폰 반경 |
| SmoothingRadius | 10 | **25~30** | 이웃 탐색 반경 |
| ParticleRadius | 10 | - | 물리 반지름 |
| ParticleRenderRadius | 15 | **20~30** | 렌더링 반지름 |
| BilateralFilterRadius | 20 | **50~80** | 스무딩 강도 |
| SurfaceNeighborThreshold | 25 | 조절 필요 | 표면 감지 임계값 |

### 문제
- 파티클 간격 (~13) > SmoothingRadius (10)
- 모든 파티클이 표면으로 판정됨

---

## Asher Zhu 연구

### 배경
- Epic Games Sr. Technical Artist (UE R&D) 출신
- Niagara Fluids에 대한 깊은 지식
- 2024년 4월 퇴사 후 슬라임 게임 개발 중

### Niagara Fluids 렌더링 방식 (asher.gg/darkhold-of-niagara)
- **SDF (Signed Distance Field)** + **Jump Flood Algorithm**
- Single Layer Water 셰이딩 모델
- 물리 충돌 지원: Static Mesh, Character, Landscape 등

### SDF란?
- 각 점에서 가장 가까운 표면까지의 거리
- SDF = 0 인 지점 = 표면
- 연속적인 스무스 표면 생성 가능

---

## 렌더링 방식 비교

| 방식 | 난이도 | 품질 | 성능 |
|------|--------|------|------|
| SSFR (현재) | 구현됨 | 중간 | 좋음 |
| SSFR 튜닝 | 쉬움 | 중상 | 좋음 |
| SDF + Jump Flood | 중간 (3-5일) | 높음 | 좋음 |
| Marching Cubes | 어려움 (5-7일) | 높음 | 좋음 |

---

## GPU vs CPU 물리

### 현재 (CPU)
- 장점: 입력 지연 없음
- 단점: 파티클 수 제한 (~2000)

### GPU 물리
- 장점: 많은 파티클 가능 (10000+)
- 단점: 1-2 프레임 입력 지연

### 하이브리드 방안
- Nucleus (중심점): CPU에서 즉시 처리
- 파티클: GPU에서 따라감
- 슬라임의 "물렁거림"으로 자연스럽게 보일 수 있음

---

## 다음 단계 옵션

### A. SSFR 튜닝 (빠름)
1. SmoothingRadius 25~30으로
2. BilateralFilterRadius 50~80으로
3. ParticleRenderRadius 늘리기

### B. SDF 렌더링 구현 (중기)
1. Jump Flood Algorithm 구현
2. 기존 SSFR 파이프라인 교체
3. 예상 3-5일

### C. Marching Cubes 구현 (장기)
1. 3D 그리드 밀도 계산
2. 256 케이스 룩업 테이블
3. 메쉬 생성 및 렌더링
4. 예상 5-7일

---

## 관련 파일

### 수정한 파일
- `KawaiiSlimeSimulationModule.h/cpp` - SpawnParticlesUniform 추가
- `KawaiiSlimeSimulationContext.cpp` - RelaxSurfaceParticles 비활성화
- `KawaiiFluidSSFRRenderer.h/cpp` - bRenderSurfaceOnly 추가
- `KawaiiFluidRendererSettings.h` - BilateralFilterRadius ClampMax 확장
- `KawaiiSlimeComponent.cpp` - SpawnParticlesUniform 호출

### 관련 파일
- `FluidRenderingParameters.h` - 렌더링 파라미터
- `FluidSceneViewExtension.cpp` - 실제 렌더링 처리

---

## 참고 자료

- [Asher.gg - Darkhold of Niagara](https://asher.gg/darkhold-of-niagara/)
- [Niagara Fluids Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-fluids-in-unreal-engine)
- [80.lv - Asher Zhu Slime Simulator](https://80.lv/articles/developer-presents-a-physically-correct-slime-simulator-made-in-ue5)
