// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/FluidEmitterComponent.h"
#include "Core/FluidSimulator.h"
#include "Kismet/GameplayStatics.h"

UFluidEmitterComponent::UFluidEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	TargetSimulator = nullptr;
	bIsEmitting = false;
	Pattern = EEmitterPattern::Point;
	ParticlesPerSecond = 100.0f;
	EmitRadius = 10.0f;
	EmitBoxExtent = FVector(10.0f, 10.0f, 10.0f);
	ConeAngle = 30.0f;
	InitialVelocity = FVector(0.0f, 0.0f, -100.0f);
	VelocityRandomness = 10.0f;
	bBurstMode = false;
	BurstCount = 100;
	AccumulatedTime = 0.0f;
}

void UFluidEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetSimulator)
	{
		FindSimulatorInWorld();
	}
}

void UFluidEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsEmitting || !TargetSimulator || bBurstMode)
	{
		return;
	}

	AccumulatedTime += DeltaTime;

	float TimePerParticle = 1.0f / ParticlesPerSecond;

	while (AccumulatedTime >= TimePerParticle)
	{
		AccumulatedTime -= TimePerParticle;

		FVector EmitPos = CalculateEmitPosition();
		FVector EmitVel = CalculateEmitVelocity();

		TargetSimulator->SpawnParticles(EmitPos, 1, 0.0f);

		TArray<FFluidParticle>& Particles = TargetSimulator->GetParticlesMutable();
		if (Particles.Num() > 0)
		{
			Particles.Last().Velocity = EmitVel;
		}
	}
}

void UFluidEmitterComponent::StartEmitting()
{
	bIsEmitting = true;
	AccumulatedTime = 0.0f;
}

void UFluidEmitterComponent::StopEmitting()
{
	bIsEmitting = false;
}

void UFluidEmitterComponent::Burst(int32 Count)
{
	if (!TargetSimulator)
	{
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		FVector EmitPos = CalculateEmitPosition();
		FVector EmitVel = CalculateEmitVelocity();

		TargetSimulator->SpawnParticles(EmitPos, 1, 0.0f);

		TArray<FFluidParticle>& Particles = TargetSimulator->GetParticlesMutable();
		if (Particles.Num() > 0)
		{
			Particles.Last().Velocity = EmitVel;
		}
	}
}

void UFluidEmitterComponent::SetTargetSimulator(AFluidSimulator* Simulator)
{
	TargetSimulator = Simulator;
}

FVector UFluidEmitterComponent::CalculateEmitPosition() const
{
	FVector BasePosition = GetComponentLocation();

	switch (Pattern)
	{
	case EEmitterPattern::Point:
		return BasePosition;

	case EEmitterPattern::Sphere:
		return BasePosition + FMath::VRand() * FMath::FRandRange(0.0f, EmitRadius);

	case EEmitterPattern::Cone:
	{
		FVector ConeDirection = GetForwardVector();
		float Angle = FMath::FRandRange(0.0f, FMath::DegreesToRadians(ConeAngle));

		FVector RandomDir = FMath::VRand();
		RandomDir = FVector::VectorPlaneProject(RandomDir, ConeDirection).GetSafeNormal();

		FVector OffsetDir = ConeDirection * FMath::Cos(Angle) + RandomDir * FMath::Sin(Angle);
		float Distance = FMath::FRandRange(0.0f, EmitRadius);

		return BasePosition + OffsetDir * Distance;
	}

	case EEmitterPattern::Box:
	{
		FVector RandomOffset;
		RandomOffset.X = FMath::FRandRange(-EmitBoxExtent.X, EmitBoxExtent.X);
		RandomOffset.Y = FMath::FRandRange(-EmitBoxExtent.Y, EmitBoxExtent.Y);
		RandomOffset.Z = FMath::FRandRange(-EmitBoxExtent.Z, EmitBoxExtent.Z);

		return BasePosition + GetComponentRotation().RotateVector(RandomOffset);
	}

	default:
		return BasePosition;
	}
}

FVector UFluidEmitterComponent::CalculateEmitVelocity() const
{
	FVector WorldVelocity = GetComponentRotation().RotateVector(InitialVelocity);

	if (VelocityRandomness > 0.0f)
	{
		WorldVelocity += FMath::VRand() * VelocityRandomness;
	}

	return WorldVelocity;
}

void UFluidEmitterComponent::FindSimulatorInWorld()
{
	AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), AFluidSimulator::StaticClass());
	if (FoundActor)
	{
		TargetSimulator = Cast<AFluidSimulator>(FoundActor);
	}
}
