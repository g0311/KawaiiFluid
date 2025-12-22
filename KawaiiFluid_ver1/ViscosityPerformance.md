# 점성 유체 구현 및 병목 분석

## 구현 개요
- `AFluidSimulator::Tick`는 위치 예측 → 이웃 탐색 → 밀도 제약 반복 → 위치 확정 후 `ApplyViscosity()`에서 점성을 별도 단계로 적용한다.
- 점성 계산은 `FViscositySolver::ApplyXSPH`가 담당하며, 두 번의 `ParallelFor`로 (1) 이웃들의 속도 차이 누적 → (2) 임시 버퍼를 실제 속도로 반영하는 구조이다.
- 모든 입자는 `TArray<FFluidParticle> Particles`에 저장되고, `NeighborIndices`를 통해 같은 이웃 목록을 밀도/점성/접착 솔버가 공유한다.
- 이웃 캐시는 스무딩 반경을 셀 크기로 쓰는 `FSpatialHash`를 매 프레임 재구축해 얻으며, 반경 내 여부 검증 없이 주변 셀의 모든 입자를 `NeighborIndices`에 추가한다.
- 커널 함수는 `SPHKernels` 네임스페이스에 모여 있고 Poly6/Spiky/Viscosity Laplacian 계산 시마다 `FMath::Pow`와 cm→m 단위 변환을 수행한다.

## 확인된 병목
- **XSPH 내부 연산 밀도**: `SPHKernels::Poly6` 호출이 이웃마다 `h^9`와 `r.Size()`를 재계산하며, `ParallelFor`에서 가장 많은 클럭을 소비한다.
- **이웃 리스트 정제 부재**: `GetNeighbors`가 실제 반경 체크 없이 셀 단위로 인덱스를 붙여 넣어, XSPH 루프에서 0 가중치로 끝날 이웃까지 모두 처리해야 한다.
- **캐시 미사용**: `FDensityConstraint`는 `FSPHKernelCoeffs`로 커널 계수를 1회만 계산하지만 점성 솔버는 동일한 최적화가 없어, 스레드별로 동일 상수를 반복 계산한다.
- **이중 ParallelFor 오버헤드**: 속도 적용이 단순 복사임에도 두 번째 `ParallelFor`로 분리되어 있어 스케줄러 세팅 비용이 다시 발생한다.

## 개선 제안
1. **커널 계수 캐싱**: Poly6 계수, `h`, `h²`, `cm→m` 상수 등을 프레임당 한 번만 계산해 `ApplyXSPH`에 넘기고 루프 내부에서는 단순 곱셈/덧셈만 수행하도록 한다.
2. **반경 기반 필터링**: `SpatialHash::GetNeighbors`에서 거리 제곱 비교로 즉시 걸러내거나, XSPH 루프에서 가중치 계산 전에 `r² > h²`이면 continue 처리하여 불필요한 `sqrt`와 커널 호출을 줄인다.
3. **속도 적용 루프 단순화**: 두 번째 `ParallelFor` 대신 단일 루프(또는 첫 번째 루프에서 곧바로 기록)로 합쳐 스케줄링 오버헤드를 제거한다.
4. **작업량 균등화**: 필요 시 `EParallelForFlags::Unbalanced` 등 스케줄링 옵션을 조정해 이웃 수 편차가 큰 파티클이 한 쓰레드를 독점하지 않도록 한다.
