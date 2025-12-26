# Unreal Slate UI 프레임워크 완전 가이드

## 1. Slate란?

**Slate**는 언리얼 엔진의 **C++ 전용 UI 프레임워크**입니다.

### UMG vs Slate
| 구분 | UMG (UUserWidget) | Slate (SWidget) |
|------|-------------------|-----------------|
| 언어 | 블루프린트 + C++ | **C++ Only** |
| 용도 | 게임 UI | **에디터 UI**, 고성능 게임 UI |
| 에디터 | 비주얼 디자이너 있음 | 코드로만 작성 |
| 성능 | 약간 느림 (래핑) | 빠름 (네이티브) |
| 에디터 확장 | 불가 | **필수** |

**핵심**: 언리얼 에디터 자체가 Slate로 만들어져 있음. 에디터 확장 = Slate 필수.

---

## 2. 핵심 철학: 선언적 문법 (Declarative Syntax)

### 기존 UI 프레임워크 (명령형)
```cpp
// Win32, Qt 등 - 명령형(Imperative)
Button* btn = new Button();
btn->SetText("Click Me");
btn->SetWidth(100);
btn->SetHeight(30);
btn->SetOnClick(MyCallback);
parent->AddChild(btn);
```

### Slate (선언형)
```cpp
// Slate - 선언형(Declarative)
// "무엇을 원하는지" 선언하면 프레임워크가 처리
SNew(SButton)
.Text(LOCTEXT("Btn", "Click Me"))
.OnClicked(this, &MyClass::OnButtonClicked)
```

**장점**:
- 코드가 UI 구조를 직접 반영 (읽기 쉬움)
- 중첩 구조가 명확
- 실수 줄어듦

---

## 3. 기본 문법: SNew 매크로

### SNew란?
```cpp
// SNew = "Slate New"의 약자
// 내부적으로 MakeShared + 초기화를 수행하는 매크로

SNew(SButton)              // SButton 위젯 생성
.Text(FText::FromString("Hello"))  // 속성 설정 (체이닝)
.OnClicked(...)            // 이벤트 바인딩
```

### SNew 확장 (내부 동작)
```cpp
// SNew(SButton) 은 대략 이렇게 확장됨:
MakeShared<SButton>()
    ->Construct(SButton::FArguments()
        .Text(...)
        .OnClicked(...));
```

### 생성자 인자 전달
```cpp
// 위젯 생성자에 인자 전달 시
SNew(SMyWidget, Arg1, Arg2, Arg3)
// ↓ 확장
MakeShared<SMyWidget>(Arg1, Arg2, Arg3)->Construct(...)
```

---

## 4. SAssignNew: 생성 + 변수 할당

### 문제 상황
```cpp
// 위젯을 나중에 참조해야 할 때
TSharedPtr<SButton> MyButton;

// 방법 1: 비효율적 (생성 후 대입)
MyButton = SNew(SButton).Text(...);

// 방법 2: 권장 (SAssignNew)
SAssignNew(MyButton, SButton).Text(...);
```

### SAssignNew의 진짜 힘: 선언적 구문 내 사용
```cpp
// 이게 핵심!
// 부모 위젯 구성하면서 동시에 자식 위젯 참조 저장
return SNew(SVerticalBox)
    + SVerticalBox::Slot()
    [
        SAssignNew(MyButton, SButton)  // 여기서 MyButton에 할당
        .Text(LOCTEXT("Btn", "Click"))
    ]
    + SVerticalBox::Slot()
    [
        SAssignNew(MyTextBlock, STextBlock)  // 여기서 MyTextBlock에 할당
        .Text(LOCTEXT("Label", "Status"))
    ];

// 이후 MyButton, MyTextBlock 사용 가능
MyButton->SetEnabled(false);
```

---

## 5. SLATE_BEGIN_ARGS: 위젯 인자 정의

### 커스텀 위젯 만들기
```cpp
class SMyCustomWidget : public SCompoundWidget
{
public:
    // ═══════════════════════════════════════
    // SLATE_BEGIN_ARGS ~ SLATE_END_ARGS
    // 이 위젯이 받을 수 있는 "속성들"을 정의
    // ═══════════════════════════════════════
    SLATE_BEGIN_ARGS(SMyCustomWidget)
        : _InitialValue(0)      // 기본값 설정
        , _bIsEnabled(true)
    {}
        // 일반 속성 (값)
        SLATE_ARGUMENT(int32, InitialValue)

        // 속성 (Attribute - 동적 바인딩 가능)
        SLATE_ATTRIBUTE(FText, LabelText)

        // 이벤트 (델리게이트)
        SLATE_EVENT(FOnClicked, OnButtonClicked)

        // bool 속성 (편의 매크로)
        SLATE_ARGUMENT(bool, bIsEnabled)

        // 슬롯 (자식 위젯)
        SLATE_DEFAULT_SLOT(FArguments, Content)

    SLATE_END_ARGS()

    // 생성 함수 (필수)
    void Construct(const FArguments& InArgs);

private:
    // 멤버 변수
    int32 CurrentValue;
    TSharedPtr<SButton> InternalButton;
};
```

### SLATE 매크로 종류

| 매크로 | 용도 | 사용 예 |
|--------|------|---------|
| `SLATE_ARGUMENT(Type, Name)` | 단순 값 전달 | `.Name(값)` |
| `SLATE_ATTRIBUTE(Type, Name)` | 동적 바인딩 가능 | `.Name(TAttribute<T>::Create(...))` |
| `SLATE_EVENT(DelegateType, Name)` | 이벤트 콜백 | `.Name(this, &Class::Handler)` |
| `SLATE_DEFAULT_SLOT(Args, Name)` | 기본 자식 슬롯 | `[ SNew(...) ]` |
| `SLATE_NAMED_SLOT(Args, Name)` | 이름 있는 슬롯 | `.Name() [ SNew(...) ]` |

### Construct 함수 구현
```cpp
void SMyCustomWidget::Construct(const FArguments& InArgs)
{
    // InArgs에서 전달받은 값 사용
    CurrentValue = InArgs._InitialValue;  // 언더스코어 접두사!

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(InArgs._LabelText)  // Attribute 바인딩
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(InternalButton, SButton)
            .IsEnabled(InArgs._bIsEnabled)
            .OnClicked(InArgs._OnButtonClicked)  // 이벤트 전달
            [
                InArgs._Content.Widget  // 슬롯 내용
            ]
        ]
    ];
}
```

### 사용
```cpp
SNew(SMyCustomWidget)
.InitialValue(42)
.LabelText(LOCTEXT("Label", "My Label"))
.bIsEnabled(true)
.OnButtonClicked(this, &MyClass::HandleClick)
[
    SNew(STextBlock).Text(LOCTEXT("Inner", "Button Content"))
]
```

---

## 6. SLATE_ATTRIBUTE vs SLATE_ARGUMENT

### SLATE_ARGUMENT: 정적 값
```cpp
SLATE_ARGUMENT(FString, Title)

// 사용: 한 번 설정하면 끝
SNew(SMyWidget).Title(TEXT("Hello"))
```

### SLATE_ATTRIBUTE: 동적 바인딩
```cpp
SLATE_ATTRIBUTE(FText, StatusText)

// 사용 1: 정적 값
SNew(SMyWidget).StatusText(LOCTEXT("Status", "Ready"))

// 사용 2: 람다로 동적 바인딩 (매 프레임 호출됨!)
SNew(SMyWidget).StatusText_Lambda([this]() {
    return FText::AsNumber(CurrentHealth);
})

// 사용 3: 델리게이트 바인딩
SNew(SMyWidget).StatusText(this, &MyClass::GetStatusText)
```

**TAttribute의 힘**: UI가 자동으로 최신 상태 유지
```cpp
// 매 프레임 GetStatusText()가 호출되어 UI 갱신
FText MyClass::GetStatusText() const
{
    return FText::Format(LOCTEXT("HP", "HP: {0}"), CurrentHealth);
}
```

---

## 7. 레이아웃: Slot 시스템

### 기본 컨테이너

#### SVerticalBox (세로 배치)
```cpp
SNew(SVerticalBox)

// Slot 추가: operator+ 사용
+ SVerticalBox::Slot()
.AutoHeight()           // 콘텐츠 크기만큼
.Padding(5)             // 여백
[
    SNew(STextBlock).Text(LOCTEXT("Title", "Title"))
]

+ SVerticalBox::Slot()
.FillHeight(1.0f)       // 남은 공간 채우기
[
    SNew(SListView<...>)
]

+ SVerticalBox::Slot()
.AutoHeight()
.HAlign(HAlign_Right)   // 수평 정렬
.VAlign(VAlign_Center)  // 수직 정렬
[
    SNew(SButton)
]
```

#### SHorizontalBox (가로 배치)
```cpp
SNew(SHorizontalBox)

+ SHorizontalBox::Slot()
.AutoWidth()            // 콘텐츠 너비만큼
[
    SNew(SImage)
]

+ SHorizontalBox::Slot()
.FillWidth(1.0f)        // 남은 공간 채우기
[
    SNew(STextBlock)
]
```

#### SOverlay (겹치기)
```cpp
SNew(SOverlay)

// 아래 레이어
+ SOverlay::Slot()
[
    SNew(SImage)  // 배경
]

// 위 레이어
+ SOverlay::Slot()
.HAlign(HAlign_Center)
.VAlign(VAlign_Center)
[
    SNew(STextBlock).Text(LOCTEXT("Overlay", "Centered Text"))
]
```

### Slot 속성 요약
```cpp
.AutoHeight() / .AutoWidth()     // 콘텐츠 크기
.FillHeight(1.0f) / .FillWidth() // 비율로 채우기
.MaxHeight(100) / .MaxWidth()    // 최대 크기
.Padding(FMargin(5, 10, 5, 10))  // 여백 (L, T, R, B)
.Padding(5)                      // 전체 여백
.HAlign(HAlign_Left/Center/Right/Fill)
.VAlign(VAlign_Top/Center/Bottom/Fill)
```

---

## 8. 이벤트 바인딩

### 기본 패턴
```cpp
// 멤버 함수 바인딩
SNew(SButton)
.OnClicked(this, &MyClass::HandleButtonClick)

// 핸들러
FReply MyClass::HandleButtonClick()
{
    // 처리
    return FReply::Handled();
}
```

### 람다 바인딩
```cpp
SNew(SButton)
.OnClicked_Lambda([this]() {
    UE_LOG(LogTemp, Log, TEXT("Clicked!"));
    return FReply::Handled();
})
```

### FReply 반환값
```cpp
FReply::Handled()           // 이벤트 처리됨, 전파 중단
FReply::Unhandled()         // 처리 안 됨, 부모로 전파
FReply::Handled().SetUserFocus(Widget)  // 포커스 변경
FReply::Handled().CaptureMouse(Widget)  // 마우스 캡처
```

---

## 9. 주요 위젯 종류

### 기본 위젯
| 위젯 | 용도 |
|------|------|
| `STextBlock` | 텍스트 표시 |
| `SEditableTextBox` | 텍스트 입력 |
| `SButton` | 버튼 |
| `SCheckBox` | 체크박스 |
| `SSlider` | 슬라이더 |
| `SComboBox` | 드롭다운 |
| `SImage` | 이미지 |
| `SSpacer` | 빈 공간 |

### 컨테이너 위젯
| 위젯 | 용도 |
|------|------|
| `SVerticalBox` | 세로 배치 |
| `SHorizontalBox` | 가로 배치 |
| `SOverlay` | 겹치기 |
| `SBox` | 크기 지정 래퍼 |
| `SBorder` | 테두리 + 배경 |
| `SScrollBox` | 스크롤 영역 |
| `SSplitter` | 분할 영역 |
| `SGridPanel` | 그리드 |

### 에디터 전용 위젯
| 위젯 | 용도 |
|------|------|
| `SEditorViewport` | 3D 뷰포트 |
| `IDetailsView` | 프로퍼티 패널 |
| `SAssetDropTarget` | 에셋 드래그앤드롭 |
| `SColorPicker` | 색상 선택기 |
| `SCurveEditor` | 커브 에디터 |

---

## 10. 위젯 계층 구조

```
SWidget (최상위 추상 클래스)
├── SLeafWidget (자식 없는 위젯)
│   ├── STextBlock
│   ├── SImage
│   └── SSpacer
│
├── SCompoundWidget (단일 자식 - ChildSlot)
│   ├── SBorder
│   ├── SButton
│   └── 커스텀 위젯 대부분
│
└── SPanel (다중 자식 - Slots)
    ├── SVerticalBox
    ├── SHorizontalBox
    ├── SOverlay
    └── SGridPanel
```

### SCompoundWidget 상속 (가장 일반적)
```cpp
class SMyWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMyWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        // ChildSlot: 단일 자식만 가능
        ChildSlot
        [
            SNew(SVerticalBox)
            // ...
        ];
    }
};
```

### SPanel 상속 (다중 자식)
```cpp
class SMyPanel : public SPanel
{
public:
    // Slot 정의
    class FSlot : public TSlotBase<FSlot>
    {
    public:
        FSlot& SomeProperty(float Value) { /* ... */ return *this; }
    };

    SLATE_BEGIN_ARGS(SMyPanel) {}
        SLATE_SLOT_ARGUMENT(FSlot, Slots)
    SLATE_END_ARGS()

    // ...
};
```

---

## 11. Visibility와 Enabled

### Visibility
```cpp
SNew(STextBlock)
.Visibility(EVisibility::Visible)      // 표시 + 히트 테스트
.Visibility(EVisibility::Hidden)       // 숨김 (공간 차지)
.Visibility(EVisibility::Collapsed)    // 숨김 (공간 없음)
.Visibility(EVisibility::HitTestInvisible)  // 표시, 클릭 안 됨

// 동적 바인딩
.Visibility(this, &MyClass::GetVisibility)

EVisibility MyClass::GetVisibility() const
{
    return bShowWidget ? EVisibility::Visible : EVisibility::Collapsed;
}
```

### Enabled
```cpp
SNew(SButton)
.IsEnabled(true)
.IsEnabled(this, &MyClass::IsButtonEnabled)  // 동적

bool MyClass::IsButtonEnabled() const
{
    return CanPerformAction();
}
```

---

## 12. 스타일링

### FSlateStyleSet
```cpp
// 스타일 정의
TSharedRef<FSlateStyleSet> StyleSet = MakeShareable(new FSlateStyleSet("MyStyle"));

// 버튼 스타일
StyleSet->Set("MyButton", FButtonStyle()
    .SetNormal(FSlateColorBrush(FLinearColor(0.2f, 0.2f, 0.2f)))
    .SetHovered(FSlateColorBrush(FLinearColor(0.3f, 0.3f, 0.3f)))
    .SetPressed(FSlateColorBrush(FLinearColor(0.1f, 0.1f, 0.1f)))
);

// 등록
FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
```

### 스타일 사용
```cpp
SNew(SButton)
.ButtonStyle(FMyStyle::Get(), "MyButton")
```

### 인라인 스타일
```cpp
SNew(SBorder)
.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.15f))
.Padding(10)
[
    SNew(STextBlock)
    .Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
    .ColorAndOpacity(FSlateColor(FLinearColor::White))
]
```

---

## 13. 팁과 베스트 프랙티스

### 1. 위젯 참조 저장
```cpp
// 나중에 조작할 위젯만 저장
TSharedPtr<SEditableTextBox> NameInput;
TSharedPtr<SButton> SubmitButton;

// 참조 불필요한 것들은 저장 안 함
SNew(SVerticalBox)
+ SVerticalBox::Slot()
[
    SNew(STextBlock).Text(...)  // 참조 필요 없음
]
+ SVerticalBox::Slot()
[
    SAssignNew(NameInput, SEditableTextBox)  // 나중에 값 읽어야 함
]
```

### 2. 성능: Invalidation
```cpp
// 나쁜 예: 매 프레임 전체 리빌드
virtual void Tick(float DeltaTime) override
{
    ChildSlot[ SNew(...) ];  // 매 프레임 새로 생성
}

// 좋은 예: TAttribute 사용
SNew(STextBlock)
.Text(this, &MyClass::GetDynamicText)  // 텍스트만 갱신
```

### 3. LOCTEXT 사용
```cpp
// 하드코딩 (나쁨)
.Text(FText::FromString("Hello"))

// LOCTEXT (좋음 - 로컬라이제이션 지원)
#define LOCTEXT_NAMESPACE "MyWidget"
.Text(LOCTEXT("Greeting", "Hello"))
#undef LOCTEXT_NAMESPACE
```

### 4. 조건부 위젯
```cpp
SNew(SVerticalBox)
+ SVerticalBox::Slot()
[
    bShowAdvanced
    ? SNew(SAdvancedPanel)
    : SNew(SSimplePanel)
]
```

---

## 14. 에디터 통합 예제

### 커스텀 에셋 에디터 전체 구조
```cpp
// 1. AssetTypeActions - 더블클릭 처리
class FAssetTypeActions_MyAsset : public FAssetTypeActions_Base
{
    void OpenAssetEditor(const TArray<UObject*>& Objects, TSharedPtr<IToolkitHost> Host) override
    {
        FMyAssetEditor* Editor = new FMyAssetEditor();
        Editor->InitEditor(Mode, Host, Objects);
    }
};

// 2. AssetEditorToolkit - 에디터 창
class FMyAssetEditor : public FAssetEditorToolkit
{
    void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override
    {
        TabManager->RegisterTabSpawner(ViewportTabId,
            FOnSpawnTab::CreateSP(this, &FMyAssetEditor::SpawnViewportTab));
    }

    TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args)
    {
        return SNew(SDockTab)
        [
            SAssignNew(Viewport, SMyEditorViewport, PreviewScene)
        ];
    }
};

// 3. SEditorViewport - 3D 뷰포트
class SMyEditorViewport : public SEditorViewport
{
    TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
    {
        return MakeShareable(new FMyViewportClient(PreviewScene, SharedThis(this)));
    }
};

// 4. FEditorViewportClient - 렌더링/입력
class FMyViewportClient : public FEditorViewportClient
{
    void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override
    {
        // 커스텀 드로잉
    }
};
```

---

## 요약

| 개념 | 설명 |
|------|------|
| **SNew** | 위젯 생성 매크로 |
| **SAssignNew** | 생성 + 변수 할당 |
| **SLATE_BEGIN_ARGS** | 위젯 속성 정의 |
| **SLATE_ARGUMENT** | 정적 값 |
| **SLATE_ATTRIBUTE** | 동적 바인딩 가능 |
| **SLATE_EVENT** | 이벤트 델리게이트 |
| **ChildSlot** | SCompoundWidget의 단일 자식 |
| **Slot()** | SPanel의 다중 자식 |
| **TAttribute** | 동적 값 바인딩 |
| **FReply** | 이벤트 처리 결과 |
