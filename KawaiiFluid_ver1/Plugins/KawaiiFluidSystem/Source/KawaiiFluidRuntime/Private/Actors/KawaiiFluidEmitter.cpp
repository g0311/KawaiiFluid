// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Actors/KawaiiFluidEmitter.h"
#include "Actors/KawaiiFluidVolume.h"
#include "Components/KawaiiFluidEmitterComponent.h"

#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"
#endif

AKawaiiFluidEmitter::AKawaiiFluidEmitter()
{
	PrimaryActorTick.bCanEverTick = false;  // EmitterComponent handles ticking

	// Create emitter component as root
	EmitterComponent = CreateDefaultSubobject<UKawaiiFluidEmitterComponent>(TEXT("KawaiiFluidEmitterComponent"));
	RootComponent = EmitterComponent;

#if WITH_EDITORONLY_DATA
	// Create billboard component for editor visualization
	BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("BillboardComponent"));
	if (BillboardComponent)
	{
		BillboardComponent->SetupAttachment(RootComponent);
		
		// Load custom emitter icon texture
		static ConstructorHelpers::FObjectFinder<UTexture2D> EmitterIconFinder(
			TEXT("/KawaiiFluidSystem/Textures/T_KawaiiFluidEmitter_Icon"));
		if (EmitterIconFinder.Succeeded())
		{
			BillboardComponent->SetSprite(EmitterIconFinder.Object);
		}
		
		BillboardComponent->bIsScreenSizeScaled = true;
		BillboardComponent->SetRelativeScale3D(FVector(0.3f));
	}
#endif
}

void AKawaiiFluidEmitter::BeginPlay()
{
	Super::BeginPlay();
	// EmitterComponent handles volume registration in its BeginPlay
}

void AKawaiiFluidEmitter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EmitterComponent handles volume unregistration in its EndPlay
	Super::EndPlay(EndPlayReason);
}

//========================================
// Delegate Getters (from EmitterComponent)
//========================================

AKawaiiFluidVolume* AKawaiiFluidEmitter::GetTargetVolume() const
{
	return EmitterComponent ? EmitterComponent->GetTargetVolume() : nullptr;
}

void AKawaiiFluidEmitter::SetTargetVolume(AKawaiiFluidVolume* NewVolume)
{
	if (EmitterComponent)
	{
		EmitterComponent->SetTargetVolume(NewVolume);
	}
}

//========================================
// API (Delegate to EmitterComponent)
//========================================

void AKawaiiFluidEmitter::BurstSpawn(int32 Count)
{
	if (EmitterComponent)
	{
		EmitterComponent->BurstSpawn(Count);
	}
}

int32 AKawaiiFluidEmitter::GetSpawnedParticleCount() const
{
	return EmitterComponent ? EmitterComponent->GetSpawnedParticleCount() : 0;
}
