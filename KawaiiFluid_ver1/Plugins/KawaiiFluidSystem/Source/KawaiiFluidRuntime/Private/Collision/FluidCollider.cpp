// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Collision/FluidCollider.h"

UFluidCollider::UFluidCollider()
{
	PrimaryComponentTick.bCanEverTick = false;

	bColliderEnabled = true;
	Friction = 0.3f;
	Restitution = 0.2f;
	bAllowAdhesion = true;
	AdhesionMultiplier = 1.0f;
}

void UFluidCollider::BeginPlay()
{
	Super::BeginPlay();
}

void UFluidCollider::ResolveCollisions(TArray<FFluidParticle>& Particles)
{
	if (!bColliderEnabled)
	{
		return;
	}

	for (FFluidParticle& Particle : Particles)
	{
		ResolveParticleCollision(Particle);
	}
}

bool UFluidCollider::GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const
{
	return false;
}

bool UFluidCollider::IsPointInside(const FVector& Point) const
{
	return false;
}

void UFluidCollider::ResolveParticleCollision(FFluidParticle& Particle)
{
	FVector ClosestPoint;
	FVector Normal;
	float Distance;

	if (!GetClosestPoint(Particle.PredictedPosition, ClosestPoint, Normal, Distance))
	{
		return;
	}

	if (IsPointInside(Particle.PredictedPosition))
	{
		Particle.PredictedPosition = ClosestPoint + Normal * 0.001f;

		float VelDotNormal = FVector::DotProduct(Particle.Velocity, Normal);

		if (VelDotNormal < 0.0f)
		{
			FVector VelNormal = VelDotNormal * Normal;
			FVector VelTangent = Particle.Velocity - VelNormal;

			Particle.Velocity = -Restitution * VelNormal + (1.0f - Friction) * VelTangent;
		}
	}
}
