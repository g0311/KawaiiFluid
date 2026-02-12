// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Tests/KawaiiFluidMetricsCollector.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Core/KawaiiFluidPresetDataAsset.h"

/**
 * @brief Collect comprehensive metrics from a raw particle array.
 * @param Particles Array of fluid particles to analyze.
 * @param RestDensity Target rest density for normalization.
 * @param SimulationBounds World-space box used to check for leaked particles.
 * @return Populated FKawaiiFluidTestMetrics structure.
 */
FKawaiiFluidTestMetrics FKawaiiFluidMetricsCollector::CollectFromParticles(
	const TArray<FKawaiiFluidParticle>& Particles,
	float RestDensity,
	const FBox& SimulationBounds)
{
	FKawaiiFluidTestMetrics Metrics;
	const int32 NumParticles = Particles.Num();

	if (NumParticles == 0)
	{
		return Metrics;
	}

	Metrics.ParticleCount = NumParticles;

	// Initialize accumulators
	double DensitySum = 0.0;
	double DensitySqSum = 0.0;
	double LambdaSum = 0.0;
	double VelocitySum = 0.0;
	FVector COMSum = FVector::ZeroVector;
	float TotalMassSum = 0.0f;
	int64 NeighborCountSum = 0;

	Metrics.MinDensity = TNumericLimits<float>::Max();
	Metrics.MaxDensity = TNumericLimits<float>::Lowest();
	Metrics.MaxVelocity = 0.0f;
	Metrics.MaxLambda = 0.0f;
	Metrics.MinNeighborCount = TNumericLimits<int32>::Max();
	Metrics.MaxNeighborCount = 0;

	FVector BoundsMin = FVector(TNumericLimits<float>::Max());
	FVector BoundsMax = FVector(TNumericLimits<float>::Lowest());

	for (int32 i = 0; i < NumParticles; ++i)
	{
		const FKawaiiFluidParticle& P = Particles[i];

		DensitySum += P.Density;
		DensitySqSum += P.Density * P.Density;
		Metrics.MinDensity = FMath::Min(Metrics.MinDensity, P.Density);
		Metrics.MaxDensity = FMath::Max(Metrics.MaxDensity, P.Density);

		LambdaSum += P.Lambda;
		Metrics.MaxLambda = FMath::Max(Metrics.MaxLambda, FMath::Abs(P.Lambda));

		const float VelMag = P.Velocity.Size();
		VelocitySum += VelMag;
		Metrics.MaxVelocity = FMath::Max(Metrics.MaxVelocity, VelMag);

		COMSum += P.Position * P.Mass;
		TotalMassSum += P.Mass;

		BoundsMin = BoundsMin.ComponentMin(P.Position);
		BoundsMax = BoundsMax.ComponentMax(P.Position);

		const int32 NeighborCount = P.NeighborIndices.Num();
		NeighborCountSum += NeighborCount;
		Metrics.MaxNeighborCount = FMath::Max(Metrics.MaxNeighborCount, NeighborCount);

		if (NeighborCount <= 1)
		{
			Metrics.IsolatedParticleCount++;
		}
		else
		{
			Metrics.MinNeighborCount = FMath::Min(Metrics.MinNeighborCount, NeighborCount);
		}

		if (!FMath::IsFinite(P.Position.X) || !FMath::IsFinite(P.Position.Y) || !FMath::IsFinite(P.Position.Z) ||
			!FMath::IsFinite(P.Velocity.X) || !FMath::IsFinite(P.Velocity.Y) || !FMath::IsFinite(P.Velocity.Z))
		{
			Metrics.InvalidParticles++;
		}

		if (!SimulationBounds.IsInside(P.Position))
		{
			Metrics.ParticlesOutOfBounds++;
		}
	}

	Metrics.AverageDensity = static_cast<float>(DensitySum / NumParticles);
	Metrics.AverageLambda = static_cast<float>(LambdaSum / NumParticles);
	Metrics.AverageVelocity = static_cast<float>(VelocitySum / NumParticles);
	Metrics.AverageNeighborCount = static_cast<float>(NeighborCountSum) / NumParticles;

	const double MeanDensity = DensitySum / NumParticles;
	Metrics.DensityVariance = static_cast<float>((DensitySqSum / NumParticles) - (MeanDensity * MeanDensity));
	Metrics.DensityStdDev = FMath::Sqrt(FMath::Max(0.0f, Metrics.DensityVariance));

	if (RestDensity > 0.0f)
	{
		Metrics.DensityRatio = Metrics.AverageDensity / RestDensity;
	}

	if (TotalMassSum > 0.0f)
	{
		Metrics.CenterOfMass = COMSum / TotalMassSum;
	}

	Metrics.TotalMass = TotalMassSum;
	if (RestDensity > 0.0f)
	{
		Metrics.TotalVolume = (TotalMassSum / RestDensity) * 1e6f;
	}

	Metrics.ParticleBounds = FBox(BoundsMin, BoundsMax);

	if (Metrics.MinNeighborCount == TNumericLimits<int32>::Max())
	{
		Metrics.MinNeighborCount = 0;
	}

	return Metrics;
}

/**
 * @brief Collect metrics from a simulation module, utilizing its preset rest density.
 * @param Module Pointer to the simulation module.
 * @return Collected metrics for the module's particle system.
 */
FKawaiiFluidTestMetrics FKawaiiFluidMetricsCollector::CollectFromModule(const UKawaiiFluidSimulationModule* Module)
{
	FKawaiiFluidTestMetrics Metrics;

	if (!Module)
	{
		return Metrics;
	}

	const TArray<FKawaiiFluidParticle>& Particles = Module->GetParticles();

	float RestDensity = 1000.0f;
	if (const UKawaiiFluidPresetDataAsset* Preset = Module->GetPreset())
	{
		RestDensity = Preset->Density;
	}

	Metrics = CollectFromParticles(Particles, RestDensity);

	Metrics.AverageConstraintError = CalculateAverageConstraintError(Particles, RestDensity);
	Metrics.MaxConstraintError = CalculateMaxConstraintError(Particles, RestDensity);

	return Metrics;
}

/**
 * @brief Calculate the relative density error for a single particle.
 * @param Density Current particle density.
 * @param RestDensity Target rest density.
 * @return Relative error value (0 = no error).
 */
float FKawaiiFluidMetricsCollector::CalculateConstraintError(float Density, float RestDensity)
{
	if (RestDensity <= 0.0f) return 0.0f;
	return (Density / RestDensity) - 1.0f;
}

/**
 * @brief Calculate the mean absolute constraint error across a set of particles.
 * @param Particles Particle array to evaluate.
 * @param RestDensity Target rest density.
 * @return Mean absolute error.
 */
float FKawaiiFluidMetricsCollector::CalculateAverageConstraintError(
	const TArray<FKawaiiFluidParticle>& Particles,
	float RestDensity)
{
	if (Particles.Num() == 0 || RestDensity <= 0.0f) return 0.0f;

	double ErrorSum = 0.0;
	for (const FKawaiiFluidParticle& P : Particles)
	{
		ErrorSum += FMath::Abs(CalculateConstraintError(P.Density, RestDensity));
	}
	return static_cast<float>(ErrorSum / Particles.Num());
}

/**
 * @brief Find the peak constraint error among all provided particles.
 * @param Particles Particle array to evaluate.
 * @param RestDensity Target rest density.
 * @return Maximum absolute error value.
 */
float FKawaiiFluidMetricsCollector::CalculateMaxConstraintError(
	const TArray<FKawaiiFluidParticle>& Particles,
	float RestDensity)
{
	if (Particles.Num() == 0 || RestDensity <= 0.0f) return 0.0f;

	float MaxError = 0.0f;
	for (const FKawaiiFluidParticle& P : Particles)
	{
		MaxError = FMath::Max(MaxError, FMath::Abs(CalculateConstraintError(P.Density, RestDensity)));
	}
	return MaxError;
}

/**
 * @brief Determine if the simulation state has reached a stable equilibrium based on historical data.
 * @param History Historical metrics collected over time.
 * @param VelocityThreshold Maximum average velocity to be considered stable (cm/s).
 * @param DensityVarianceThreshold Maximum allowed variance in density over time.
 * @param MinSamples Minimum number of samples required for a valid check.
 * @return True if the simulation is deemed stable.
 */
bool FKawaiiFluidMetricsCollector::IsInEquilibrium(
	const FFluidTestMetricsHistory& History,
	float VelocityThreshold,
	float DensityVarianceThreshold,
	int32 MinSamples)
{
	if (History.Samples.Num() < MinSamples) return false;

	const int32 StartIdx = History.Samples.Num() - MinSamples;
	for (int32 i = StartIdx; i < History.Samples.Num(); ++i)
	{
		if (History.Samples[i].AverageVelocity > VelocityThreshold)
		{
			return false;
		}
	}

	return History.HasDensityStabilized(MinSamples, DensityVarianceThreshold);
}
