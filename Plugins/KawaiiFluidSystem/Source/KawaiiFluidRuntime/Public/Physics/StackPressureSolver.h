// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/KawaiiFluidParticle.h"

/**
 * Stack Pressure Solver
 * Applies weight transfer from stacked attached particles for realistic dripping behavior
 *
 * When particles are attached to a surface and stacked on top of each other,
 * particles higher up should transfer their weight to particles below,
 * causing lower particles to slide down faster.
 *
 * This creates more realistic "dripping" behavior instead of all particles
 * sliding down at the same speed.
 */
class KAWAIIFLUIDRUNTIME_API FStackPressureSolver
{
public:
	FStackPressureSolver();

	/**
	 * Apply stack pressure to attached particles
	 * Particles above transfer their weight to particles below, accelerating dripping
	 *
	 * @param Particles - Array of fluid particles
	 * @param Gravity - World gravity vector (cm/s^2)
	 * @param StackPressureScale - Stack pressure strength multiplier (0-10, recommended: 0.5-2.0)
	 * @param SmoothingRadius - Neighbor search radius (cm)
	 * @param DeltaTime - Simulation delta time (seconds)
	 */
	void Apply(
		TArray<FKawaiiFluidParticle>& Particles,
		const FVector& Gravity,
		float StackPressureScale,
		float SmoothingRadius,
		float DeltaTime
	);

private:
	/**
	 * Check if neighbor is "above" on the surface (relative to sliding direction)
	 * "Above" means in the opposite direction of tangent gravity
	 *
	 * @param ParticleI - The particle receiving weight
	 * @param ParticleJ - The neighbor particle (potential weight source)
	 * @param TangentGravityDir - Normalized tangent gravity direction (sliding direction)
	 * @return Height difference (positive if J is above I)
	 */
	float GetHeightDifference(
		const FKawaiiFluidParticle& ParticleI,
		const FKawaiiFluidParticle& ParticleJ,
		const FVector& TangentGravityDir
	) const;
};
