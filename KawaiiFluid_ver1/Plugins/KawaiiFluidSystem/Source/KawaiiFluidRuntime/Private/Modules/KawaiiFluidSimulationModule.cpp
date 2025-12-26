// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Modules/KawaiiFluidSimulationModule.h"
#include "Core/SpatialHash.h"
#include "Collision/FluidCollider.h"
#include "Components/FluidInteractionComponent.h"
#include "Data/KawaiiFluidPresetDataAsset.h"

UKawaiiFluidSimulationModule::UKawaiiFluidSimulationModule()
{
}

void UKawaiiFluidSimulationModule::Initialize(UKawaiiFluidPresetDataAsset* InPreset)
{
	if (bIsInitialized)
	{
		return;
	}

	Preset = InPreset;
	bRuntimePresetDirty = true;

	// SpatialHash 초기화 (Independent 모드용)
	float CellSize = 20.0f;
	if (Preset)
	{
		CellSize = Preset->SmoothingRadius;
	}
	InitializeSpatialHash(CellSize);

	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidSimulationModule initialized"));
}

void UKawaiiFluidSimulationModule::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	Particles.Empty();
	Colliders.Empty();
	InteractionComponents.Empty();
	SpatialHash.Reset();
	Preset = nullptr;
	RuntimePreset = nullptr;

	bIsInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidSimulationModule shutdown"));
}

void UKawaiiFluidSimulationModule::SetPreset(UKawaiiFluidPresetDataAsset* InPreset)
{
	Preset = InPreset;
	bRuntimePresetDirty = true;

	// SpatialHash 재구성
	if (Preset && SpatialHash.IsValid())
	{
		SpatialHash = MakeShared<FSpatialHash>(Preset->SmoothingRadius);
	}
}

void UKawaiiFluidSimulationModule::InitializeSpatialHash(float CellSize)
{
	SpatialHash = MakeShared<FSpatialHash>(CellSize);
}

bool UKawaiiFluidSimulationModule::HasAnyOverride() const
{
	return bOverride_RestDensity || bOverride_Compliance || bOverride_SmoothingRadius ||
	       bOverride_ViscosityCoefficient || bOverride_Gravity || bOverride_AdhesionStrength;
}

UKawaiiFluidPresetDataAsset* UKawaiiFluidSimulationModule::GetEffectivePreset()
{
	if (!HasAnyOverride())
	{
		return Preset;
	}

	if (bRuntimePresetDirty)
	{
		UpdateRuntimePreset();
	}

	return RuntimePreset ? RuntimePreset : Preset.Get();
}

void UKawaiiFluidSimulationModule::UpdateRuntimePreset()
{
	if (!Preset)
	{
		return;
	}

	// RuntimePreset이 없으면 생성
	if (!RuntimePreset)
	{
		RuntimePreset = DuplicateObject<UKawaiiFluidPresetDataAsset>(Preset, GetTransientPackage());
	}
	else
	{
		// 기존 RuntimePreset을 베이스 Preset으로 리셋
		RuntimePreset->RestDensity = Preset->RestDensity;
		RuntimePreset->Compliance = Preset->Compliance;
		RuntimePreset->SmoothingRadius = Preset->SmoothingRadius;
		RuntimePreset->ViscosityCoefficient = Preset->ViscosityCoefficient;
		RuntimePreset->Gravity = Preset->Gravity;
		RuntimePreset->AdhesionStrength = Preset->AdhesionStrength;
	}

	// Override 적용
	if (bOverride_RestDensity)
	{
		RuntimePreset->RestDensity = Override_RestDensity;
	}
	if (bOverride_Compliance)
	{
		RuntimePreset->Compliance = Override_Compliance;
	}
	if (bOverride_SmoothingRadius)
	{
		RuntimePreset->SmoothingRadius = Override_SmoothingRadius;
	}
	if (bOverride_ViscosityCoefficient)
	{
		RuntimePreset->ViscosityCoefficient = Override_ViscosityCoefficient;
	}
	if (bOverride_Gravity)
	{
		RuntimePreset->Gravity = Override_Gravity;
	}
	if (bOverride_AdhesionStrength)
	{
		RuntimePreset->AdhesionStrength = Override_AdhesionStrength;
	}

	bRuntimePresetDirty = false;
}

FKawaiiFluidSimulationParams UKawaiiFluidSimulationModule::BuildSimulationParams() const
{
	FKawaiiFluidSimulationParams Params;

	// 외력
	Params.ExternalForce = AccumulatedExternalForce;

	// 콜라이더 / 상호작용 컴포넌트
	Params.Colliders = Colliders;
	Params.InteractionComponents = InteractionComponents;

	// Preset에서 콜리전 설정 가져오기
	if (Preset)
	{
		Params.CollisionChannel = Preset->CollisionChannel;
		Params.ParticleRadius = Preset->ParticleRadius;
	}

	// World와 IgnoreActor는 Component에서 설정해야 함 (모듈은 World 접근 불가)

	return Params;
}

int32 UKawaiiFluidSimulationModule::SpawnParticle(FVector Position, FVector Velocity)
{
	FFluidParticle NewParticle(Position, NextParticleID++);
	NewParticle.Velocity = Velocity;

	if (Preset)
	{
		NewParticle.Mass = Preset->ParticleMass;
	}
	else
	{
		NewParticle.Mass = 1.0f;
	}

	Particles.Add(NewParticle);
	return NewParticle.ParticleID;
}

void UKawaiiFluidSimulationModule::SpawnParticles(FVector Location, int32 Count, float SpawnRadius)
{
	Particles.Reserve(Particles.Num() + Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FVector RandomOffset = FMath::VRand() * FMath::FRandRange(0.0f, SpawnRadius);
		FVector SpawnPos = Location + RandomOffset;
		SpawnParticle(SpawnPos);
	}
}

void UKawaiiFluidSimulationModule::ClearAllParticles()
{
	Particles.Empty();
	NextParticleID = 0;
}

TArray<FVector> UKawaiiFluidSimulationModule::GetParticlePositions() const
{
	TArray<FVector> Positions;
	Positions.Reserve(Particles.Num());

	for (const FFluidParticle& Particle : Particles)
	{
		Positions.Add(Particle.Position);
	}

	return Positions;
}

TArray<FVector> UKawaiiFluidSimulationModule::GetParticleVelocities() const
{
	TArray<FVector> Velocities;
	Velocities.Reserve(Particles.Num());

	for (const FFluidParticle& Particle : Particles)
	{
		Velocities.Add(Particle.Velocity);
	}

	return Velocities;
}

void UKawaiiFluidSimulationModule::ApplyExternalForce(FVector Force)
{
	AccumulatedExternalForce += Force;
}

void UKawaiiFluidSimulationModule::ApplyForceToParticle(int32 ParticleIndex, FVector Force)
{
	if (Particles.IsValidIndex(ParticleIndex))
	{
		Particles[ParticleIndex].Velocity += Force;
	}
}

void UKawaiiFluidSimulationModule::RegisterCollider(UFluidCollider* Collider)
{
	if (Collider && !Colliders.Contains(Collider))
	{
		Colliders.Add(Collider);
	}
}

void UKawaiiFluidSimulationModule::UnregisterCollider(UFluidCollider* Collider)
{
	Colliders.Remove(Collider);
}

void UKawaiiFluidSimulationModule::RegisterInteractionComponent(UFluidInteractionComponent* Component)
{
	if (Component && !InteractionComponents.Contains(Component))
	{
		InteractionComponents.Add(Component);
	}
}

void UKawaiiFluidSimulationModule::UnregisterInteractionComponent(UFluidInteractionComponent* Component)
{
	InteractionComponents.Remove(Component);
}

TArray<int32> UKawaiiFluidSimulationModule::GetParticlesInRadius(FVector Location, float Radius) const
{
	TArray<int32> Result;
	const float RadiusSq = Radius * Radius;

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		const float DistSq = FVector::DistSquared(Particles[i].Position, Location);
		if (DistSq <= RadiusSq)
		{
			Result.Add(i);
		}
	}

	return Result;
}

TArray<int32> UKawaiiFluidSimulationModule::GetParticlesInBox(FVector Center, FVector Extent) const
{
	TArray<int32> Result;
	const FBox Box(Center - Extent, Center + Extent);

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		if (Box.IsInside(Particles[i].Position))
		{
			Result.Add(i);
		}
	}

	return Result;
}

bool UKawaiiFluidSimulationModule::GetParticleInfo(int32 ParticleIndex, FVector& OutPosition, FVector& OutVelocity, float& OutDensity) const
{
	if (!Particles.IsValidIndex(ParticleIndex))
	{
		return false;
	}

	const FFluidParticle& Particle = Particles[ParticleIndex];
	OutPosition = Particle.Position;
	OutVelocity = Particle.Velocity;
	OutDensity = Particle.Density;

	return true;
}
