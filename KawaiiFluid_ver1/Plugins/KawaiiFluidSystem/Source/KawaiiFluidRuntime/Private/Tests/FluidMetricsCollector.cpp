// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Tests/FluidMetricsCollector.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Data/KawaiiFluidPresetDataAsset.h"

FFluidTestMetrics FFluidMetricsCollector::CollectFromModule(const UKawaiiFluidSimulationModule* Module)
{
	FFluidTestMetrics Metrics;

	if (!Module)
	{
		return Metrics;
	}

	// Get particle data from module
	const TArray<FFluidParticle>& Particles = Module->GetParticles();

	// Get rest density from module preset
	float RestDensity = 1000.0f;  // Default
	if (const UKawaiiFluidPresetDataAsset* Preset = Module->GetPreset())
	{
		RestDensity = Preset->Density;
	}

	// Collect from particles
	Metrics = CollectFromParticles(Particles, RestDensity);

	// Add constraint error calculations
	Metrics.AverageConstraintError = CalculateAverageConstraintError(Particles, RestDensity);
	Metrics.MaxConstraintError = CalculateMaxConstraintError(Particles, RestDensity);

	return Metrics;
}
