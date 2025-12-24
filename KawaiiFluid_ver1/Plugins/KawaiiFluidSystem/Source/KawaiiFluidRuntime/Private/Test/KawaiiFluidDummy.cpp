// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Test/KawaiiFluidDummy.h"
#include "Components/KawaiiFluidDummyComponent.h"
#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

AKawaiiFluidDummy::AKawaiiFluidDummy()
{
	PrimaryActorTick.bCanEverTick = false;  // Component가 Tick 처리

	// Root Component 생성
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootSceneComponent;

	// ✅ DummyComponent 생성 (실제 구현)
	DummyComponent = CreateDefaultSubobject<UKawaiiFluidDummyComponent>(TEXT("DummyComponent"));

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidDummy: Created with UKawaiiFluidDummyComponent wrapper (Legacy)"));
}

void AKawaiiFluidDummy::BeginPlay()
{
	Super::BeginPlay();

	// Niagara 자동 생성 (테스트 편의 기능)
	if (bAutoCreateNiagara && DummyComponent)
	{
		CreateAutoNiagaraSystem();
	}
}

void AKawaiiFluidDummy::CreateAutoNiagaraSystem()
{
	// 이미 생성되었으면 스킵
	if (AutoNiagaraComponent)
	{
		return;
	}

	// Niagara System 로드
	UNiagaraSystem* SystemToUse = nullptr;

	if (NiagaraSystemAsset.IsValid())
	{
		SystemToUse = NiagaraSystemAsset.Get();
	}
	else if (!NiagaraSystemAsset.IsNull())
	{
		// 동기 로드 시도
		SystemToUse = NiagaraSystemAsset.LoadSynchronous();
	}

	// 시스템이 없으면 경고만 출력 (기본 시스템 생성은 복잡하므로 생략)
	if (!SystemToUse)
	{
		UE_LOG(LogTemp, Warning, 
			TEXT("AKawaiiFluidDummy: No Niagara System assigned. Please assign NiagaraSystemAsset in the editor."));
		UE_LOG(LogTemp, Warning, 
			TEXT("  → Create a Niagara System with Kawaii Fluid Data Interface and assign it."));
		return;
	}

	// Niagara Component 생성
	AutoNiagaraComponent = NewObject<UNiagaraComponent>(this, UNiagaraComponent::StaticClass(), TEXT("AutoNiagaraComponent"));
	if (!AutoNiagaraComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AKawaiiFluidDummy: Failed to create Niagara Component"));
		return;
	}

	// Component 설정
	AutoNiagaraComponent->SetupAttachment(RootSceneComponent);
	AutoNiagaraComponent->RegisterComponent();
	AutoNiagaraComponent->SetAsset(SystemToUse);

	// Data Interface 파라미터 자동 설정 (Actor 참조)
	// Niagara System의 User Parameter에서 "FluidSource" (또는 설정된 이름) 찾기
	AutoNiagaraComponent->SetActorParameter(DataInterfaceParameterName, this);

	// 활성화
	AutoNiagaraComponent->Activate(true);

	UE_LOG(LogTemp, Log, 
		TEXT("AKawaiiFluidDummy: Auto-created Niagara Component with system: %s"), 
		*SystemToUse->GetName());
	UE_LOG(LogTemp, Log, 
		TEXT("  → Data Interface Parameter '%s' set to this actor"), 
		*DataInterfaceParameterName.ToString());
}

void AKawaiiFluidDummy::SetNiagaraSystem(UNiagaraSystem* System)
{
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("AKawaiiFluidDummy::SetNiagaraSystem: Null system provided"));
		return;
	}

	// 기존 Component 제거
	if (AutoNiagaraComponent)
	{
		AutoNiagaraComponent->DestroyComponent();
		AutoNiagaraComponent = nullptr;
	}

	// 새 System 설정
	NiagaraSystemAsset = System;
	
	// 생성
	CreateAutoNiagaraSystem();
}

void AKawaiiFluidDummy::QuickTestSetup(int32 InParticleCount, EKawaiiFluidDummyGenMode InDataMode, UNiagaraSystem* InNiagaraSystem)
{
	if (!DummyComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AKawaiiFluidDummy::QuickTestSetup: No DummyComponent"));
		return;
	}

	// 1. 파티클 설정
	DummyComponent->ParticleCount = FMath::Clamp(InParticleCount, 1, 10000);
	DummyComponent->DataMode = InDataMode;
	DummyComponent->RegenerateTestData();

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidDummy::QuickTestSetup: ParticleCount=%d, DataMode=%d"), 
		InParticleCount, (int32)InDataMode);

	// 2. Niagara 설정 (제공된 경우)
	if (InNiagaraSystem)
	{
		bAutoCreateNiagara = true;
		SetNiagaraSystem(InNiagaraSystem);
	}
	else if (bAutoCreateNiagara && !AutoNiagaraComponent)
	{
		// 이미 설정된 System으로 생성
		CreateAutoNiagaraSystem();
	}

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidDummy::QuickTestSetup: Ready for testing!"));
}

UNiagaraSystem* AKawaiiFluidDummy::CreateDefaultNiagaraSystem()
{
	// 런타임에서 Niagara System을 코드로 생성하는 것은 매우 복잡하고 권장되지 않음
	// 에디터에서 미리 만들어진 에셋을 사용하도록 안내만 제공
	UE_LOG(LogTemp, Error, 
		TEXT("AKawaiiFluidDummy: Cannot create default Niagara System at runtime."));
	UE_LOG(LogTemp, Error, 
		TEXT("  → Please create a Niagara System in the editor and assign it to NiagaraSystemAsset."));
	
	return nullptr;
}
