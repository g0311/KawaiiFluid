// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Physics/StackPressureSolver.h"
#include "Physics/SPHKernels.h"
#include "Async/ParallelFor.h"

FStackPressureSolver::FStackPressureSolver()
{
}

void FStackPressureSolver::Apply(
	TArray<FKawaiiFluidParticle>& Particles,
	const FVector& Gravity,
	float StackPressureScale,
	float SmoothingRadius,
	float DeltaTime)
{
	// Skip if disabled or no particles
	if (StackPressureScale <= 0.0f || Particles.Num() == 0 || DeltaTime <= 0.0f)
	{
		return;
	}

	const float RadiusSq = SmoothingRadius * SmoothingRadius;
	const int32 ParticleCount = Particles.Num();

	// Pre-allocate stack forces array
	TArray<FVector> StackForces;
	StackForces.SetNumZeroed(ParticleCount);

	// Calculate stack pressure forces (parallel)
	ParallelFor(ParticleCount, [&](int32 i)
	{
		FKawaiiFluidParticle& Particle = Particles[i];

		// Only process attached particles
		if (!Particle.bIsAttached)
		{
			return;
		}

		const FVector& SurfaceNormal = Particle.AttachedSurfaceNormal;

		// Calculate tangent gravity (gravity projected onto surface)
		// TangentGravity = Gravity - (Gravity . Normal) * Normal
		float NormalComponent = FVector::DotProduct(Gravity, SurfaceNormal);
		FVector TangentGravity = Gravity - NormalComponent * SurfaceNormal;

		float TangentMag = TangentGravity.Size();

		// Skip if on horizontal surface (no sliding direction)
		if (TangentMag < 0.1f)
		{
			return;
		}

		FVector TangentDir = TangentGravity / TangentMag;

		// "Up" direction on surface = opposite of tangent gravity
		// Particles in this direction are "above" and contribute weight
		FVector UpDir = -TangentDir;

		// Accumulate stack weight from neighbors above
		float StackWeight = 0.0f;

		for (int32 NeighborIdx : Particle.NeighborIndices)
		{
			if (NeighborIdx == i || NeighborIdx < 0 || NeighborIdx >= ParticleCount)
			{
				continue;
			}

			const FKawaiiFluidParticle& Neighbor = Particles[NeighborIdx];

			// Only consider attached neighbors
			if (!Neighbor.bIsAttached)
			{
				continue;
			}

			// Optional: Only consider neighbors attached to same actor
			// This prevents weight transfer between different surfaces
			if (Neighbor.AttachedActor != Particle.AttachedActor)
			{
				continue;
			}

			FVector ToNeighbor = Neighbor.Position - Particle.Position;
			float DistSq = ToNeighbor.SizeSquared();

			// Skip if too far or too close
			if (DistSq > RadiusSq || DistSq < KINDA_SMALL_NUMBER)
			{
				continue;
			}

			// Check if neighbor is "above" (in the up direction on surface)
			float HeightDiff = FVector::DotProduct(ToNeighbor, UpDir);

			if (HeightDiff > 0.0f)
			{
				// Neighbor is above - accumulate its weight
				float Dist = FMath::Sqrt(DistSq);

				// Use SPH Poly6 kernel for smooth weight falloff
				float KernelWeight = SPHKernels::Poly6(Dist, SmoothingRadius);

				// Weight contribution:
				// - Proportional to neighbor's mass
				// - Proportional to kernel weight (closer = more influence)
				// - Proportional to how much "above" the neighbor is
				float HeightFactor = HeightDiff / Dist;  // Normalized height contribution
				StackWeight += Neighbor.Mass * KernelWeight * HeightFactor;
			}
		}

		// Calculate stack pressure force
		// Force = tangent direction * accumulated weight * scale
		if (StackWeight > 0.0f)
		{
			StackForces[i] = TangentDir * StackWeight * StackPressureScale;
		}

	}, EParallelForFlags::Unbalanced);

	// Apply stack forces to velocities (parallel)
	ParallelFor(ParticleCount, [&](int32 i)
	{
		if (Particles[i].bIsAttached && !StackForces[i].IsNearlyZero())
		{
			// Apply as acceleration: v += F * dt
			Particles[i].Velocity += StackForces[i] * DeltaTime;
		}
	});
}

float FStackPressureSolver::GetHeightDifference(
	const FKawaiiFluidParticle& ParticleI,
	const FKawaiiFluidParticle& ParticleJ,
	const FVector& TangentGravityDir) const
{
	// "Up" is opposite of sliding direction
	FVector UpDir = -TangentGravityDir;
	FVector ToNeighbor = ParticleJ.Position - ParticleI.Position;

	// Positive if J is above I
	return FVector::DotProduct(ToNeighbor, UpDir);
}
