// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/KawaiiFluidSimulationContext.h"
#include "KawaiiSlimeSimulationContext.generated.h"

/**
 * Slime-specific Simulation Context
 *
 * Inherits from UKawaiiFluidSimulationContext and overrides SimulateSubstep
 * to provide slime-specific physics behavior.
 *
 * Slime-specific features:
 * - Shape Matching with cluster-based center calculation
 * - Nucleus Attraction (cohesion force)
 * - Surface Tension
 * - Anti-Gravity during jump
 *
 * Usage:
 * Set this class in Preset->ContextClass for slime presets.
 */
UCLASS(BlueprintType, Blueprintable)
class KAWAIIFLUIDRUNTIME_API UKawaiiSlimeSimulationContext : public UKawaiiFluidSimulationContext
{
	GENERATED_BODY()

public:
	UKawaiiSlimeSimulationContext();

	//========================================
	// Override: SimulateSubstep
	//========================================

	/**
	 * Slime-specific substep simulation
	 * Includes: Shape Matching, Nucleus Attraction, Surface Tension
	 */
	virtual void SimulateSubstep(
		TArray<FFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		FSpatialHash& SpatialHash,
		float SubstepDT
	) override;

protected:
	//========================================
	// Slime-specific Physics
	//========================================

	/**
	 * Apply nucleus attraction - pull particles toward cluster center
	 * This is the cohesion force that keeps the slime together
	 */
	virtual void ApplyNucleusAttraction(
		TArray<FFluidParticle>& Particles,
		const FKawaiiFluidSimulationParams& Params,
		float DeltaTime
	);

	/**
	 * Apply surface tension to surface particles
	 * Pulls surface particles inward to create smooth surface
	 */
	virtual void ApplySurfaceTension(
		TArray<FFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params
	);

	/**
	 * Update surface particle detection
	 * Identifies which particles are on the surface using neighbor count
	 */
	virtual void UpdateSurfaceParticles(
		TArray<FFluidParticle>& Particles,
		const FKawaiiFluidSimulationParams& Params
	);

	/**
	 * Apply anti-gravity during jump to maintain form
	 */
	virtual void ApplyAntiGravity(
		TArray<FFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		float DeltaTime
	);

	/**
	 * Relax surface particles for uniform distribution
	 * Ensures particles on the surface maintain even spacing
	 * This improves SSFR rendering quality by reducing bumpy surfaces
	 */
	virtual void RelaxSurfaceParticles(
		TArray<FFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		float DeltaTime
	);

	/**
	 * Override density constraints to exclude surface particles
	 * Surface particles are handled by RelaxSurfaceParticles instead
	 */
	virtual void SolveDensityConstraints(
		TArray<FFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		float DeltaTime
	) override;

	//========================================
	// Helper Functions
	//========================================

	/**
	 * Calculate center of mass for a cluster
	 */
	FVector CalculateClusterCenter(
		const TArray<FFluidParticle>& Particles,
		int32 SourceID
	) const;

	/**
	 * Calculate max distance from center (for falloff calculations)
	 */
	float CalculateMaxDistanceFromCenter(
		const TArray<FFluidParticle>& Particles,
		const FVector& Center,
		int32 SourceID
	) const;
};
