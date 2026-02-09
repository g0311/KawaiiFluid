// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Physics/ViscositySolver.h"
#include "Physics/SPHKernels.h"
#include "Async/ParallelFor.h"

namespace ViscosityConstants
{
	constexpr float CM_TO_M = 0.01f;
	constexpr float CM_TO_M_SQ = CM_TO_M * CM_TO_M;
}

FViscositySolver::FViscositySolver()
{
}

void FViscositySolver::ApplyXSPH(TArray<FKawaiiFluidParticle>& Particles, float ViscosityCoeff, float SmoothingRadius)
{
	if (ViscosityCoeff <= 0.0f)
	{
		return;
	}

	const int32 ParticleCount = Particles.Num();
	if (ParticleCount == 0)
	{
		return;
	}

	// [Optimization 1] Cache kernel coefficients - compute once per frame
	SPHKernels::FKernelCoefficients KernelCoeffs;
	KernelCoeffs.Precompute(SmoothingRadius);

	// Radius squared (to avoid sqrt calls)
	const float RadiusSquared = SmoothingRadius * SmoothingRadius;

	// Store new velocities in temporary array
	TArray<FVector> NewVelocities;
	NewVelocities.SetNum(ParticleCount);

	// [Optimization 4] Load balancing - use Unbalanced flag to handle varying neighbor counts
	ParallelFor(ParticleCount, [&](int32 i)
	{
		const FKawaiiFluidParticle& Particle = Particles[i];
		FVector VelocityCorrection = FVector::ZeroVector;
		float WeightSum = 0.0f;

		for (int32 NeighborIdx : Particle.NeighborIndices)
		{
			if (NeighborIdx == i)
			{
				continue;
			}

			const FKawaiiFluidParticle& Neighbor = Particles[NeighborIdx];
			const FVector r = Particle.Position - Neighbor.Position;

			// [Optimization 2] Radius-based filtering - early skip if r² > h² (avoid sqrt)
			const float rSquared = r.SizeSquared();
			if (rSquared > RadiusSquared)
			{
				continue;
			}

			// [Optimization 1] Directly compute Poly6 with cached coefficients
			// W(r, h) = Poly6Coeff * (h² - r²)³
			// Unit conversion: cm -> m (coefficients already computed in m units)
			const float h2_m = KernelCoeffs.h2;
			const float r2_m = rSquared * ViscosityConstants::CM_TO_M_SQ;
			const float diff = h2_m - r2_m;
			const float Weight = (diff > 0.0f) ? KernelCoeffs.Poly6Coeff * diff * diff * diff : 0.0f;

			// Velocity difference
			const FVector VelocityDiff = Neighbor.Velocity - Particle.Velocity;

			VelocityCorrection += VelocityDiff * Weight;
			WeightSum += Weight;
		}

		// Normalization (optional)
		if (WeightSum > 0.0f)
		{
			VelocityCorrection /= WeightSum;
		}

		// Apply XSPH viscosity: v_new = v + c * Σ(v_j - v_i) * W
		NewVelocities[i] = Particle.Velocity + ViscosityCoeff * VelocityCorrection;

	}, EParallelForFlags::Unbalanced);

	// [Optimization 3] Simplify velocity application loop - use simple for loop instead of ParallelFor
	// Simple copy operations have more scheduler overhead than benefit
	for (int32 i = 0; i < ParticleCount; ++i)
	{
		Particles[i].Velocity = NewVelocities[i];
	}
}
