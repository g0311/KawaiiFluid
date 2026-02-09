// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/KawaiiFluidParticle.h"

/**
 * @brief Viscosity solver.
 *
 * XSPH-based viscosity implementation.
 * Represents viscosity effects by averaging particle velocities with their neighbors.
 *
 * High viscosity coefficient = viscous fluids like honey, slime
 * Low viscosity coefficient = flowing fluids like water
 */
class KAWAIIFLUIDRUNTIME_API FViscositySolver
{
public:
	FViscositySolver();

	/**
	 * @brief Apply XSPH viscosity.
	 *
	 * v_i = v_i + c * Σ(v_j - v_i) * W(r_ij, h)
	 *
	 * @param Particles Particle array
	 * @param ViscosityCoeff Viscosity coefficient (0.0 ~ 1.0)
	 * @param SmoothingRadius Kernel radius
	 */
	void ApplyXSPH(TArray<FKawaiiFluidParticle>& Particles, float ViscosityCoeff, float SmoothingRadius);

};
