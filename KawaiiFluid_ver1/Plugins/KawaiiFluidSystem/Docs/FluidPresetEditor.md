# Kawaii Fluid Preset Editor 구현 문서

## 개요

`UKawaiiFluidPresetDataAsset`을 더블클릭하면 나이아가라처럼 **전용 에디터**가 열리고, 3D 뷰포트에서 **실시간 유체 시뮬레이션**을 확인할 수 있는 시스템.

---

## 아키텍처 개요

```
                     ┌─────────────────────────────────────┐
                     │     UKawaiiFluidPresetDataAsset     │
                     │         (편집 대상 에셋)             │
                     └──────────────────┬──────────────────┘
                                        │ 더블클릭
                                        ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                    FAssetTypeActions_FluidPreset                          │
│                    (에셋 타입별 동작 정의)                                  │
│  - OpenAssetEditor() → FKawaiiFluidPresetAssetEditor 생성                 │
└───────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                   FKawaiiFluidPresetAssetEditor                           │
│                   (메인 에디터 툴킷)                                       │
│                                                                           │
│  상속: FAssetEditorToolkit + FEditorUndoClient + FTickableEditorObject    │
│                                                                           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐   │
│  │  Viewport Tab   │  │  Details Tab    │  │  Preview Settings Tab   │   │
│  │                 │  │                 │  │                         │   │
│  │  SFluidPreset   │  │  IDetailsView   │  │  IDetailsView           │   │
│  │  EditorViewport │  │  (Preset 속성)  │  │  (프리뷰 설정)           │   │
│  └────────┬────────┘  └─────────────────┘  └─────────────────────────┘   │
└───────────┼───────────────────────────────────────────────────────────────┘
            │
            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                      SFluidPresetEditorViewport                           │
│                      (3D 뷰포트 Slate 위젯)                                │
│                                                                           │
│  상속: SEditorViewport + FGCObject + ICommonEditorViewportToolbarInfoProvider │
│                                                                           │
│  - MakeEditorViewportClient() → FFluidPresetEditorViewportClient 생성     │
│  - PopulateViewportOverlays() → Stats Overlay 추가                        │
└───────────────────────────────────────────────────────────────────────────┘
            │
            ├──────────────────────────────────┐
            ▼                                  ▼
┌──────────────────────────────┐    ┌──────────────────────────────────────┐
│ FFluidPresetEditorViewport   │    │         FFluidPreviewScene           │
│ Client                       │    │         (프리뷰 월드 + 시뮬레이션)     │
│                              │    │                                      │
│ 상속: FEditorViewportClient  │    │  상속: FAdvancedPreviewScene          │
│                              │    │                                      │
│ - Draw() 디버그 드로잉       │    │  - Particles[] (파티클 배열)          │
│ - InputKey() 입력 처리       │    │  - SimulationContext (물리 계산)     │
│ - 카메라 오빗 컨트롤         │    │  - SpatialHash (이웃 검색)           │
│                              │    │  - ISM Component (시각화)            │
└──────────────────────────────┘    └──────────────────────────────────────┘
```

---

## 부모 클래스 상세 설명

### 1. FAssetTypeActions_Base

**헤더**: `AssetTypeActions_Base.h` (AssetTools 모듈)

**역할**: 특정 에셋 타입에 대한 동작 정의

**왜 상속하는가?**
- 에셋 더블클릭 시 동작 커스터마이즈
- Content Browser에서 에셋 아이콘/색상 지정
- 우클릭 컨텍스트 메뉴 항목 추가

**핵심 오버라이드 함수**:
```cpp
class FAssetTypeActions_FluidPreset : public FAssetTypeActions_Base
{
    // 에셋 타입 이름 (Content Browser 표시)
    virtual FText GetName() const override;

    // 지원하는 UClass
    virtual UClass* GetSupportedClass() const override;

    // Content Browser에서 에셋 색상
    virtual FColor GetTypeColor() const override;

    // 에셋 카테고리 (Physics, Animation 등)
    virtual uint32 GetCategories() override;

    // ★ 더블클릭 시 호출 - 커스텀 에디터 열기
    virtual void OpenAssetEditor(
        const TArray<UObject*>& InObjects,
        TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;
};
```

**등록 방법** (모듈 StartupModule):
```cpp
IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_FluidPreset()));
```

---

### 2. FAssetEditorToolkit

**헤더**: `Toolkits/AssetEditorToolkit.h` (UnrealEd 모듈)

**역할**: 에셋 편집용 독립 에디터 창 프레임워크

**왜 상속하는가?**
- 탭 기반 레이아웃 관리 (Viewport, Details 등)
- 메뉴바, 툴바 자동 생성
- 에디터 창 생명주기 관리
- 언리얼 에디터 통합 (창 관리, 포커스 등)

**핵심 오버라이드 함수**:
```cpp
class FKawaiiFluidPresetAssetEditor : public FAssetEditorToolkit
{
    // 툴킷 고유 이름 (내부 식별용)
    virtual FName GetToolkitFName() const override;

    // 창 제목에 표시될 기본 이름
    virtual FText GetBaseToolkitName() const override;

    // 월드 센트릭 모드 탭 접두사
    virtual FString GetWorldCentricTabPrefix() const override;

    // 탭 색상
    virtual FLinearColor GetWorldCentricTabColorScale() const override;

    // ★ 탭 스포너 등록 (Viewport, Details 등)
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
};
```

**탭 등록 패턴**:
```cpp
void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    // 부모 클래스 호출 (기본 메뉴/툴바 등록)
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

    // 뷰포트 탭 등록
    InTabManager->RegisterTabSpawner(ViewportTabId,
        FOnSpawnTab::CreateSP(this, &ThisClass::SpawnTab_Viewport))
        .SetDisplayName(LOCTEXT("Viewport", "Viewport"))
        .SetIcon(FSlateIcon(...));

    // Details 탭 등록
    InTabManager->RegisterTabSpawner(DetailsTabId,
        FOnSpawnTab::CreateSP(this, &ThisClass::SpawnTab_Details))
        .SetDisplayName(LOCTEXT("Details", "Details"));
}
```

**레이아웃 정의**:
```cpp
const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("LayoutName")
    ->AddArea(
        FTabManager::NewPrimaryArea()
        ->SetOrientation(Orient_Horizontal)
        ->Split(
            FTabManager::NewStack()
            ->SetSizeCoefficient(0.7f)
            ->AddTab(ViewportTabId, ETabState::OpenedTab)
        )
        ->Split(
            FTabManager::NewStack()
            ->SetSizeCoefficient(0.3f)
            ->AddTab(DetailsTabId, ETabState::OpenedTab)
        )
    );
```

---

### 3. FTickableEditorObject

**헤더**: `TickableEditorObject.h` (UnrealEd 모듈)

**역할**: 에디터에서 매 프레임 Tick 받기

**왜 상속하는가?**
- 시뮬레이션 업데이트 (물리 계산)
- 실시간 프리뷰 갱신
- 게임 월드 없이도 Tick 동작

**핵심 오버라이드 함수**:
```cpp
class FKawaiiFluidPresetAssetEditor : public FTickableEditorObject
{
    // ★ 매 프레임 호출
    virtual void Tick(float DeltaTime) override
    {
        UpdateSimulation(DeltaTime);
    }

    // 프로파일링용 Stat ID
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FKawaiiFluidPresetAssetEditor, STATGROUP_Tickables);
    }

    // Tick 가능 여부
    virtual bool IsTickable() const override { return true; }

    // Tick 타입 (Always = 에디터 포커스 무관하게 항상)
    virtual ETickableTickType GetTickableTickType() const override
    {
        return ETickableTickType::Always;
    }
};
```

**주의사항**:
- `ETickableTickType::Always`는 에디터 창이 비활성화되어도 Tick
- 성능을 위해 창 포커스 체크 권장:
```cpp
void Tick(float DeltaTime)
{
    // 현재 창이 포커스된 경우에만 시뮬레이션
    if (FSlateApplication::IsInitialized())
    {
        TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
        TSharedPtr<SWindow> MyWindow = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());
        if (MyWindow != ActiveWindow)
            return;
    }
    UpdateSimulation(DeltaTime);
}
```

---

### 4. FEditorUndoClient

**헤더**: `EditorUndoClient.h` (UnrealEd 모듈)

**역할**: Undo/Redo 이벤트 수신

**왜 상속하는가?**
- 프로퍼티 변경 Undo 시 프리뷰 갱신
- 에디터 상태 동기화

**핵심 오버라이드 함수**:
```cpp
class FKawaiiFluidPresetAssetEditor : public FEditorUndoClient
{
    virtual void PostUndo(bool bSuccess) override
    {
        if (bSuccess)
        {
            RefreshPreview();  // 프리뷰 갱신
        }
    }

    virtual void PostRedo(bool bSuccess) override
    {
        if (bSuccess)
        {
            RefreshPreview();
        }
    }
};
```

**등록 필수**:
```cpp
// 생성 시
GEditor->RegisterForUndo(this);

// 소멸 시
GEditor->UnregisterForUndo(this);
```

---

### 5. FAdvancedPreviewScene

**헤더**: `AdvancedPreviewScene.h` (AdvancedPreviewScene 모듈)

**역할**: 독립된 3D 프리뷰 월드 제공

**왜 상속하는가?**
- 게임 월드와 분리된 독립 UWorld
- 자체 조명, 환경 설정
- 프리뷰 전용 액터/컴포넌트 관리

**FPreviewScene vs FAdvancedPreviewScene**:
| 클래스 | 특징 |
|--------|------|
| FPreviewScene | 기본 프리뷰 씬, 단순 |
| FAdvancedPreviewScene | 환경 설정, 스카이라이트, PostProcess 등 고급 기능 |

**핵심 기능**:
```cpp
class FFluidPreviewScene : public FAdvancedPreviewScene
{
public:
    FFluidPreviewScene(FPreviewScene::ConstructionValues CVS)
        : FAdvancedPreviewScene(CVS)
    {
        // 프리뷰 월드 접근
        UWorld* World = GetWorld();

        // 액터 스폰
        AActor* Actor = World->SpawnActor<AActor>(...);

        // 컴포넌트 추가
        AddComponent(MeshComponent, FTransform::Identity);
    }

    // ★ GC 방지 필수 (TObjectPtr 사용)
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override
    {
        FAdvancedPreviewScene::AddReferencedObjects(Collector);
        Collector.AddReferencedObject(MyComponent);
    }
};
```

**ConstructionValues 옵션**:
```cpp
FPreviewScene::ConstructionValues CVS;
CVS.bCreatePhysicsScene = false;  // 물리 씬 생성 여부
CVS.LightBrightness = 3;          // 조명 밝기
CVS.SkyBrightness = 1;            // 스카이 밝기
CVS.bAllowAudioPlayback = false;  // 오디오 허용
```

---

### 6. SEditorViewport

**헤더**: `SEditorViewport.h` (UnrealEd 모듈)

**역할**: 3D 렌더링 Slate 위젯

**왜 상속하는가?**
- 3D 씬 렌더링
- 뷰포트 입력 처리
- 오버레이 위젯 지원

**핵심 오버라이드 함수**:
```cpp
class SFluidPresetEditorViewport : public SEditorViewport
{
protected:
    // ★ 뷰포트 클라이언트 생성
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
    {
        ViewportClient = MakeShareable(
            new FFluidPresetEditorViewportClient(PreviewScene.ToSharedRef(), SharedThis(this))
        );
        return ViewportClient.ToSharedRef();
    }

    // ★ 오버레이 위젯 추가 (통계, 디버그 정보 등)
    virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override
    {
        SEditorViewport::PopulateViewportOverlays(Overlay);

        Overlay->AddSlot()
            .VAlign(VAlign_Top)
            .HAlign(HAlign_Left)
            [
                SNew(SFluidPreviewStatsOverlay, PreviewScene)
            ];
    }

    // 커맨드 바인딩 (단축키 등)
    virtual void BindCommands() override;
};
```

**FGCObject 상속 필수**:
```cpp
class SFluidPresetEditorViewport : public SEditorViewport, public FGCObject
{
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override { return TEXT("SFluidPresetEditorViewport"); }
};
```

---

### 7. FEditorViewportClient

**헤더**: `EditorViewportClient.h` (UnrealEd 모듈)

**역할**: 뷰포트 렌더링/입력/카메라 제어

**왜 상속하는가?**
- 카메라 오빗/줌/팬 컨트롤
- 씬 렌더링 커스터마이즈
- 디버그 드로잉 (PDI)
- 마우스/키보드 입력 처리

**핵심 오버라이드 함수**:
```cpp
class FFluidPresetEditorViewportClient : public FEditorViewportClient
{
public:
    // ★ 매 프레임 호출 (뷰포트 업데이트)
    virtual void Tick(float DeltaSeconds) override;

    // ★ 디버그 드로잉 (속도 벡터, 그리드 등)
    virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override
    {
        FEditorViewportClient::Draw(View, PDI);

        // 속도 벡터 그리기
        for (const FFluidParticle& P : Particles)
        {
            PDI->DrawLine(P.Position, P.Position + P.Velocity, FColor::Red, SDPG_World);
        }
    }

    // 키보드 입력
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

    // 마우스 클릭
    virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy,
        FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;

    // 배경색
    virtual FLinearColor GetBackgroundColor() const override
    {
        return FLinearColor(0.1f, 0.1f, 0.12f);
    }

    // 오빗 카메라 활성화
    virtual bool ShouldOrbitCamera() const override { return true; }
};
```

**생성자 패턴**:
```cpp
FFluidPresetEditorViewportClient::FFluidPresetEditorViewportClient(
    TSharedRef<FFluidPreviewScene> InPreviewScene,
    TSharedRef<SFluidPresetEditorViewport> InViewportWidget)
    : FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InViewportWidget))
{
    // 기본 설정
    SetRealtime(true);                    // 실시간 렌더링
    SetShowStats(true);                   // 통계 표시
    EngineShowFlags.Grid = true;          // 그리드 표시

    // 초기 카메라 위치
    SetViewLocation(FVector(-300, 0, 200));
    SetViewRotation(FRotator(-20, 0, 0));
}
```

---

## 데이터 흐름

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         시뮬레이션 루프                                  │
└─────────────────────────────────────────────────────────────────────────┘

1. FKawaiiFluidPresetAssetEditor::Tick(DeltaTime)
   │
   ├─ 창 포커스 체크
   │
   └─ UpdateSimulation(DeltaTime)
      │
      └─ FFluidPreviewScene::TickSimulation(DeltaTime)
         │
         ├─ ContinuousSpawn(DeltaTime)          ← 파티클 연속 스폰
         │   └─ SpawnAccumulator += DeltaTime * ParticlesPerSecond
         │   └─ 정수 개수만큼 스폰
         │
         ├─ SimulationContext->Simulate(...)    ← SPH 물리 계산
         │   ├─ SpatialHash 빌드
         │   ├─ 밀도/압력 계산
         │   ├─ 점성/표면장력 계산
         │   └─ 위치 적분
         │
         ├─ HandleFloorCollision()              ← 바닥 충돌
         ├─ HandleWallCollision()               ← 벽 충돌
         │
         └─ UpdateParticleVisuals()             ← ISM 인스턴스 업데이트
             └─ ParticleMeshComponent->UpdateInstanceTransform(...)

┌─────────────────────────────────────────────────────────────────────────┐
│                         프로퍼티 변경 흐름                               │
└─────────────────────────────────────────────────────────────────────────┘

1. 유저가 Details Panel에서 값 변경
   │
   └─ IDetailsView::OnFinishedChangingProperties()
      │
      └─ FKawaiiFluidPresetAssetEditor::OnPresetPropertyChanged()
         │
         └─ RefreshPreview()
            │
            ├─ FFluidPreviewScene::RefreshFromPreset()
            │   ├─ 색상 업데이트
            │   ├─ SpatialHash 셀 크기 업데이트
            │   └─ 파티클 반경 캐시
            │
            └─ ViewportWidget->RefreshViewport()
```

---

## Preview Settings 상세

| 설정 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| **Spawn** ||||
| `ParticlesPerSecond` | float | 60 | 초당 스폰할 파티클 수 (1~500) |
| `MaxParticleCount` | int32 | 1000 | 최대 파티클 수 (1~5000) |
| `SpawnLocation` | FVector | (0, 0, 200) | 스폰 위치 (월드 좌표) |
| `SpawnVelocity` | FVector | (0, 0, -50) | 초기 속도 |
| `SpawnRadius` | float | 15 | 스폰 영역 반경 (1~100) |
| **Environment** ||||
| `bShowFloor` | bool | true | 바닥 메시 표시 |
| `FloorHeight` | float | 0 | 바닥 Z 위치 |
| `FloorSize` | FVector | (500, 500, 10) | 바닥 크기 |
| `bShowWalls` | bool | false | 경계 벽 표시 |
| `WallHeight` | float | 300 | 벽 높이 |
| **Debug** ||||
| `bShowVelocityVectors` | bool | false | 속도 벡터 표시 |
| `bShowNeighborConnections` | bool | false | 이웃 연결선 표시 |
| `bShowSpatialHashGrid` | bool | false | 공간 해시 그리드 표시 |
| `bShowDensityColors` | bool | false | 밀도 기반 색상 표시 |

---

## GC 관리 (중요)

### 문제점
`FFluidPreviewScene`은 `UObject`가 아니므로 `UPROPERTY()` 매크로가 GC 방지에 효과가 없음.

### 해결책
1. **TObjectPtr<T> 사용**: UE5 방식의 스마트 포인터
2. **AddReferencedObjects 오버라이드**: GC 루트에 등록

```cpp
// 헤더 (.h)
class FFluidPreviewScene : public FAdvancedPreviewScene
{
    // TObjectPtr로 선언 (raw pointer 대신)
    TObjectPtr<UKawaiiFluidSimulationContext> SimulationContext;
    TObjectPtr<AActor> PreviewActor;
    TArray<TObjectPtr<UStaticMeshComponent>> WallMeshComponents;

    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};

// 구현 (.cpp)
void FFluidPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
    FAdvancedPreviewScene::AddReferencedObjects(Collector);

    Collector.AddReferencedObject(SimulationContext);
    Collector.AddReferencedObject(PreviewActor);
    Collector.AddReferencedObjects(WallMeshComponents);  // TArray용
}
```

**주의**: UE5.x에서는 `AddReferencedObject(UObject*)` 대신 `TObjectPtr` 필수
(Incremental GC 호환성)

---

## 파일 구조

```
KawaiiFluidEditor/
├── Public/
│   ├── AssetTypeActions/
│   │   └── AssetTypeActions_FluidPreset.h    ← FAssetTypeActions_Base 상속
│   ├── Editor/
│   │   └── KawaiiFluidPresetAssetEditor.h    ← FAssetEditorToolkit + FTickableEditorObject
│   ├── Preview/
│   │   ├── FluidPreviewScene.h               ← FAdvancedPreviewScene 상속
│   │   └── FluidPreviewSettings.h            ← USTRUCT + UObject 래퍼
│   ├── Viewport/
│   │   ├── SFluidPresetEditorViewport.h      ← SEditorViewport 상속
│   │   └── FluidPresetEditorViewportClient.h ← FEditorViewportClient 상속
│   ├── Widgets/
│   │   ├── SFluidPreviewPlaybackControls.h   ← SCompoundWidget 상속
│   │   └── SFluidPreviewStatsOverlay.h       ← SCompoundWidget 상속
│   └── Style/
│       └── FluidEditorStyle.h                ← FSlateStyleSet
└── Private/
    └── (위 헤더들의 .cpp 파일들)
```

---

## 모듈 등록

```cpp
// KawaiiFluidEditorModule.cpp

void FKawaiiFluidEditorModule::StartupModule()
{
    // 스타일 등록
    FFluidEditorStyle::Initialize();

    // AssetTypeActions 등록
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    FluidPresetTypeActions = MakeShareable(new FAssetTypeActions_FluidPreset());
    AssetTools.RegisterAssetTypeActions(FluidPresetTypeActions.ToSharedRef());
}

void FKawaiiFluidEditorModule::ShutdownModule()
{
    // AssetTypeActions 해제
    if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
    {
        IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
        AssetTools.UnregisterAssetTypeActions(FluidPresetTypeActions.ToSharedRef());
    }

    // 스타일 해제
    FFluidEditorStyle::Shutdown();
}
```

---

## 확장 포인트

### SSFR 렌더링 통합 (TODO)
- `FKawaiiFluidRenderResource` 추가하여 GPU 버퍼 관리
- `IKawaiiFluidRenderable` 인터페이스 구현
- `UFluidRendererSubsystem` 연동

### 추가 기능 아이디어
- **콜라이더 프리뷰**: 박스, 구체 등 장애물 배치
- **프리셋 비교**: 2개 프리셋 동시 시뮬레이션
- **레코딩**: 시뮬레이션 결과 저장/재생
- **성능 프로파일링**: 병목 구간 시각화
