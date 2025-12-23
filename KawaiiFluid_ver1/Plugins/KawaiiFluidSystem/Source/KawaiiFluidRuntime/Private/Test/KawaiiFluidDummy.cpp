// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Test/KawaiiFluidDummy.h"
#include "Components/KawaiiFluidDummyComponent.h"
#include "Components/SceneComponent.h"

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
