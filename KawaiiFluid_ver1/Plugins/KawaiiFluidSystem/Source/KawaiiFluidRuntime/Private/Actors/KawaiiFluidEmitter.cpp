// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Actors/KawaiiFluidEmitter.h"
#include "Actors/KawaiiFluidVolume.h"
#include "Components/KawaiiFluidEmitterComponent.h"

AKawaiiFluidEmitter::AKawaiiFluidEmitter()
{
	PrimaryActorTick.bCanEverTick = false;  // EmitterComponent handles ticking

	// Create emitter component as root
	EmitterComponent = CreateDefaultSubobject<UKawaiiFluidEmitterComponent>(TEXT("EmitterComponent"));
	RootComponent = EmitterComponent;

	// Set default spawn settings for continuous emission
	EmitterComponent->SpawnSettings.SpawnType = EFluidSpawnType::Emitter;
	EmitterComponent->SpawnSettings.EmitterType = EFluidEmitterType::Stream;
	EmitterComponent->SpawnSettings.ParticlesPerSecond = 100.0f;
	EmitterComponent->SpawnSettings.SpawnSpeed = 100.0f;
	EmitterComponent->SpawnSettings.SpawnDirection = FVector(0, 0, -1);
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
