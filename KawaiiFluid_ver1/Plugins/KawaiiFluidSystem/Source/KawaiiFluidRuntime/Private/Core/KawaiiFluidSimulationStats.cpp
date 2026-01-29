// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Core/KawaiiFluidSimulationStats.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

//=============================================================================
// Stat Definitions
//=============================================================================

DEFINE_STAT(STAT_FluidTotalSimulation);
DEFINE_STAT(STAT_FluidSpatialHash);
DEFINE_STAT(STAT_FluidDensitySolve);
DEFINE_STAT(STAT_FluidViscosity);
DEFINE_STAT(STAT_FluidCohesion);
DEFINE_STAT(STAT_FluidCollision);
DEFINE_STAT(STAT_FluidGPUSimulation);
DEFINE_STAT(STAT_FluidGPUReadback);

DEFINE_STAT(STAT_FluidParticleCount);
DEFINE_STAT(STAT_FluidActiveParticles);
DEFINE_STAT(STAT_FluidAttachedParticles);
DEFINE_STAT(STAT_FluidSubstepCount);

DEFINE_STAT(STAT_FluidAvgVelocity);
DEFINE_STAT(STAT_FluidMaxVelocity);
DEFINE_STAT(STAT_FluidAvgDensity);
DEFINE_STAT(STAT_FluidDensityError);
DEFINE_STAT(STAT_FluidAvgNeighbors);

// Stability metrics
DEFINE_STAT(STAT_FluidDensityStdDev);
DEFINE_STAT(STAT_FluidVelocityStdDev);
DEFINE_STAT(STAT_FluidPerParticleError);
DEFINE_STAT(STAT_FluidKineticEnergy);
DEFINE_STAT(STAT_FluidStabilityScore);

//=============================================================================
// FFluidSimulationStats Implementation
//=============================================================================

void FKawaiiFluidSimulationStats::LogStats(const FString& Label) const
{
	const FString ModeStr = bIsGPUSimulation ? TEXT("GPU") : TEXT("CPU");
	const FString LabelStr = Label.IsEmpty() ? ModeStr : FString::Printf(TEXT("%s (%s)"), *Label, *ModeStr);

	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("Fluid Simulation Stats: %s"), *LabelStr);
	UE_LOG(LogTemp, Log, TEXT("========================================"));

	// Particle counts
	UE_LOG(LogTemp, Log, TEXT("Particles: %d total, %d active, %d attached"),
		ParticleCount, ActiveParticleCount, AttachedParticleCount);

	// Velocity
	UE_LOG(LogTemp, Log, TEXT("Velocity (cm/s): Avg=%.2f, Min=%.2f, Max=%.2f"),
		AvgVelocity, MinVelocity, MaxVelocity);

	// Density
	UE_LOG(LogTemp, Log, TEXT("Density: Avg=%.2f, Min=%.2f, Max=%.2f, Rest=%.2f"),
		AvgDensity, MinDensity, MaxDensity, RestDensity);
	UE_LOG(LogTemp, Log, TEXT("Density Error: %.2f%%"), DensityError);

	// Stability Metrics (GPU detailed mode)
	if (DensityStdDev > 0.0f || VelocityStdDev > 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("--- Stability Metrics ---"));
		UE_LOG(LogTemp, Log, TEXT("  Density StdDev: %.2f"), DensityStdDev);
		UE_LOG(LogTemp, Log, TEXT("  Velocity StdDev: %.2f cm/s"), VelocityStdDev);
		UE_LOG(LogTemp, Log, TEXT("  Per-Particle Error: %.2f%%"), PerParticleDensityError);
		UE_LOG(LogTemp, Log, TEXT("  Kinetic Energy: %.2f"), KineticEnergy);
		UE_LOG(LogTemp, Log, TEXT("  Stability Score: %.1f / 100"), StabilityScore);
	}

	// Neighbors
	UE_LOG(LogTemp, Log, TEXT("Neighbors: Avg=%.2f, Min=%d, Max=%d"),
		AvgNeighborCount, MinNeighborCount, MaxNeighborCount);

	// Forces
	UE_LOG(LogTemp, Log, TEXT("Forces: Pressure=%.4f, Viscosity=%.4f, Cohesion=%.4f"),
		AvgPressureCorrection, AvgViscosityForce, AvgCohesionForce);

	// Collision
	UE_LOG(LogTemp, Log, TEXT("Collisions: Bounds=%d, Primitive=%d, Ground=%d"),
		BoundsCollisionCount, PrimitiveCollisionCount, GroundContactCount);

	// Solver
	UE_LOG(LogTemp, Log, TEXT("Solver: Substeps=%d, SolverIter=%d"),
		SubstepCount, SolverIterations);

	// Performance
	UE_LOG(LogTemp, Log, TEXT("Performance (ms): Total=%.3f, Hash=%.3f, Density=%.3f"),
		TotalSimulationTimeMs, SpatialHashTimeMs, DensitySolveTimeMs);
	UE_LOG(LogTemp, Log, TEXT("  Viscosity=%.3f, Cohesion=%.3f, Collision=%.3f"),
		ViscosityTimeMs, CohesionTimeMs, CollisionTimeMs);

	if (bIsGPUSimulation)
	{
		UE_LOG(LogTemp, Log, TEXT("  GPU Sim=%.3f, GPU Readback=%.3f"),
			GPUSimulationTimeMs, GPUReadbackTimeMs);
	}

	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

FString FKawaiiFluidSimulationStats::ToString() const
{
	const FString ModeStr = bIsGPUSimulation ? TEXT("GPU") : TEXT("CPU");

	FString Result;
	Result += FString::Printf(TEXT("[%s] Particles: %d (Active: %d, Attached: %d)\n"),
		*ModeStr, ParticleCount, ActiveParticleCount, AttachedParticleCount);
	Result += FString::Printf(TEXT("Velocity: Avg=%.1f, Max=%.1f cm/s\n"),
		AvgVelocity, MaxVelocity);
	Result += FString::Printf(TEXT("Density: Avg=%.1f, Error=%.2f%%\n"),
		AvgDensity, DensityError);

	// Stability metrics (show if available)
	if (DensityStdDev > 0.0f || VelocityStdDev > 0.0f)
	{
		Result += FString::Printf(TEXT("Stability: Score=%.1f/100, DenStd=%.1f, VelStd=%.1f, PPErr=%.1f%%\n"),
			StabilityScore, DensityStdDev, VelocityStdDev, PerParticleDensityError);
	}

	Result += FString::Printf(TEXT("Neighbors: Avg=%.1f (Min=%d, Max=%d)\n"),
		AvgNeighborCount, MinNeighborCount, MaxNeighborCount);
	Result += FString::Printf(TEXT("Forces: P=%.4f, V=%.4f, C=%.4f\n"),
		AvgPressureCorrection, AvgViscosityForce, AvgCohesionForce);
	Result += FString::Printf(TEXT("Collisions: Bounds=%d, Prim=%d, Ground=%d\n"),
		BoundsCollisionCount, PrimitiveCollisionCount, GroundContactCount);
	Result += FString::Printf(TEXT("Time: %.2fms (Hash=%.2f, Density=%.2f, Visc=%.2f, Coh=%.2f, Col=%.2f)"),
		TotalSimulationTimeMs, SpatialHashTimeMs, DensitySolveTimeMs,
		ViscosityTimeMs, CohesionTimeMs, CollisionTimeMs);

	return Result;
}

FString FKawaiiFluidSimulationStats::CompareWith(const FKawaiiFluidSimulationStats& Other, const FString& OtherLabel) const
{
	const FString ThisMode = bIsGPUSimulation ? TEXT("GPU") : TEXT("CPU");
	const FString OtherMode = Other.bIsGPUSimulation ? TEXT("GPU") : TEXT("CPU");

	FString Result;
	Result += FString::Printf(TEXT("=== Comparison: %s vs %s ===\n"), *ThisMode, *OtherLabel);

	// Particle counts
	Result += FString::Printf(TEXT("Particles: %d vs %d (diff: %d)\n"),
		ParticleCount, Other.ParticleCount, ParticleCount - Other.ParticleCount);

	// Velocity comparison
	float VelDiff = AvgVelocity - Other.AvgVelocity;
	float VelDiffPct = (Other.AvgVelocity > 0.001f) ? (VelDiff / Other.AvgVelocity * 100.0f) : 0.0f;
	Result += FString::Printf(TEXT("Avg Velocity: %.2f vs %.2f (diff: %.2f, %.1f%%)\n"),
		AvgVelocity, Other.AvgVelocity, VelDiff, VelDiffPct);

	// Density comparison
	float DenDiff = AvgDensity - Other.AvgDensity;
	float DenDiffPct = (Other.AvgDensity > 0.001f) ? (DenDiff / Other.AvgDensity * 100.0f) : 0.0f;
	Result += FString::Printf(TEXT("Avg Density: %.2f vs %.2f (diff: %.2f, %.1f%%)\n"),
		AvgDensity, Other.AvgDensity, DenDiff, DenDiffPct);

	// Density error comparison
	Result += FString::Printf(TEXT("Density Error: %.2f%% vs %.2f%%\n"),
		DensityError, Other.DensityError);

	// Neighbor count comparison
	float NeighborDiff = AvgNeighborCount - Other.AvgNeighborCount;
	Result += FString::Printf(TEXT("Avg Neighbors: %.2f vs %.2f (diff: %.2f)\n"),
		AvgNeighborCount, Other.AvgNeighborCount, NeighborDiff);

	// Force comparison
	Result += FString::Printf(TEXT("Pressure Corr: %.4f vs %.4f\n"),
		AvgPressureCorrection, Other.AvgPressureCorrection);
	Result += FString::Printf(TEXT("Viscosity: %.4f vs %.4f\n"),
		AvgViscosityForce, Other.AvgViscosityForce);
	Result += FString::Printf(TEXT("Cohesion: %.4f vs %.4f\n"),
		AvgCohesionForce, Other.AvgCohesionForce);

	// Performance comparison
	Result += FString::Printf(TEXT("Total Time: %.2fms vs %.2fms\n"),
		TotalSimulationTimeMs, Other.TotalSimulationTimeMs);

	return Result;
}

//=============================================================================
// FFluidStatsCollector Implementation
//=============================================================================

void FKawaiiFluidSimulationStatsCollector::BeginFrame()
{
	if (!bEnabled)
	{
		return;
	}

	// Save previous stats
	PreviousStats = CurrentStats;

	// For GPU simulation with detailed stats enabled, stats are updated asynchronously via readback.
	// Preserve all GPU-updated stats before reset to avoid flickering.
	// When detailed stats is disabled, allow stats to reset to 0.
	const bool bPreserveGPUStats = CurrentStats.bIsGPUSimulation && bDetailedGPU;

	// Save basic stats (GPU readback is async, not every frame)
	const int32 SavedParticleCount = CurrentStats.ParticleCount;
	const int32 SavedActiveParticleCount = CurrentStats.ActiveParticleCount;
	const int32 SavedAttachedParticleCount = CurrentStats.AttachedParticleCount;
	const float SavedAvgVelocity = CurrentStats.AvgVelocity;
	const float SavedMinVelocity = CurrentStats.MinVelocity;
	const float SavedMaxVelocity = CurrentStats.MaxVelocity;
	const float SavedAvgDensity = CurrentStats.AvgDensity;
	const float SavedMinDensity = CurrentStats.MinDensity;
	const float SavedMaxDensity = CurrentStats.MaxDensity;
	const float SavedDensityError = CurrentStats.DensityError;
	const float SavedAvgNeighborCount = CurrentStats.AvgNeighborCount;
	const int32 SavedMinNeighborCount = CurrentStats.MinNeighborCount;
	const int32 SavedMaxNeighborCount = CurrentStats.MaxNeighborCount;

	// Save stability metrics
	const float SavedDensityStdDev = CurrentStats.DensityStdDev;
	const float SavedVelocityStdDev = CurrentStats.VelocityStdDev;
	const float SavedPerParticleDensityError = CurrentStats.PerParticleDensityError;
	const float SavedKineticEnergy = CurrentStats.KineticEnergy;
	const float SavedStabilityScore = CurrentStats.StabilityScore;

	// Reset current stats
	CurrentStats.Reset();

	// Restore GPU stats (only updated when GPU readback completes)
	if (bPreserveGPUStats)
	{
		CurrentStats.bIsGPUSimulation = true;
		CurrentStats.ParticleCount = SavedParticleCount;
		CurrentStats.ActiveParticleCount = SavedActiveParticleCount;
		CurrentStats.AttachedParticleCount = SavedAttachedParticleCount;
		CurrentStats.AvgVelocity = SavedAvgVelocity;
		CurrentStats.MinVelocity = SavedMinVelocity;
		CurrentStats.MaxVelocity = SavedMaxVelocity;
		CurrentStats.AvgDensity = SavedAvgDensity;
		CurrentStats.MinDensity = SavedMinDensity;
		CurrentStats.MaxDensity = SavedMaxDensity;
		CurrentStats.DensityError = SavedDensityError;
		CurrentStats.AvgNeighborCount = SavedAvgNeighborCount;
		CurrentStats.MinNeighborCount = SavedMinNeighborCount;
		CurrentStats.MaxNeighborCount = SavedMaxNeighborCount;
		CurrentStats.DensityStdDev = SavedDensityStdDev;
		CurrentStats.VelocityStdDev = SavedVelocityStdDev;
		CurrentStats.PerParticleDensityError = SavedPerParticleDensityError;
		CurrentStats.KineticEnergy = SavedKineticEnergy;
		CurrentStats.StabilityScore = SavedStabilityScore;
	}

	// Reset accumulators
	VelocitySum = 0.0;
	VelocitySampleCount = 0;
	DensitySum = 0.0;
	DensitySampleCount = 0;
	NeighborSum = 0.0;
	NeighborSampleCount = 0;
	PressureCorrectionSum = 0.0;
	PressureCorrectionSampleCount = 0;
	ViscosityForceSum = 0.0;
	ViscosityForceSampleCount = 0;
	CohesionForceSum = 0.0;
	CohesionForceSampleCount = 0;

	// Initialize min/max (only when not preserving GPU stats)
	if (!bPreserveGPUStats)
	{
		CurrentStats.MinVelocity = TNumericLimits<float>::Max();
		CurrentStats.MaxVelocity = TNumericLimits<float>::Lowest();
		CurrentStats.MinDensity = TNumericLimits<float>::Max();
		CurrentStats.MaxDensity = TNumericLimits<float>::Lowest();
		CurrentStats.MinNeighborCount = TNumericLimits<int32>::Max();
		CurrentStats.MaxNeighborCount = TNumericLimits<int32>::Lowest();
	}

	bFrameActive = true;
}

void FKawaiiFluidSimulationStatsCollector::EndFrame()
{
	if (!bEnabled || !bFrameActive)
	{
		return;
	}

	// Calculate averages
	if (VelocitySampleCount > 0)
	{
		CurrentStats.AvgVelocity = static_cast<float>(VelocitySum / VelocitySampleCount);
	}

	if (DensitySampleCount > 0)
	{
		CurrentStats.AvgDensity = static_cast<float>(DensitySum / DensitySampleCount);

		// Calculate density error
		if (CurrentStats.RestDensity > 0.001f)
		{
			double ErrorSum = 0.0;
			// Note: We track the error during sampling now
			CurrentStats.DensityError = static_cast<float>(
				FMath::Abs(CurrentStats.AvgDensity - CurrentStats.RestDensity) /
				CurrentStats.RestDensity * 100.0);
		}
	}

	if (NeighborSampleCount > 0)
	{
		CurrentStats.AvgNeighborCount = static_cast<float>(NeighborSum / NeighborSampleCount);
	}

	if (PressureCorrectionSampleCount > 0)
	{
		CurrentStats.AvgPressureCorrection = static_cast<float>(PressureCorrectionSum / PressureCorrectionSampleCount);
	}

	if (ViscosityForceSampleCount > 0)
	{
		CurrentStats.AvgViscosityForce = static_cast<float>(ViscosityForceSum / ViscosityForceSampleCount);
	}

	if (CohesionForceSampleCount > 0)
	{
		CurrentStats.AvgCohesionForce = static_cast<float>(CohesionForceSum / CohesionForceSampleCount);
	}

	// Fix min/max if no samples
	if (CurrentStats.MinVelocity == TNumericLimits<float>::Max())
	{
		CurrentStats.MinVelocity = 0.0f;
	}
	if (CurrentStats.MaxVelocity == TNumericLimits<float>::Lowest())
	{
		CurrentStats.MaxVelocity = 0.0f;
	}
	if (CurrentStats.MinDensity == TNumericLimits<float>::Max())
	{
		CurrentStats.MinDensity = 0.0f;
	}
	if (CurrentStats.MaxDensity == TNumericLimits<float>::Lowest())
	{
		CurrentStats.MaxDensity = 0.0f;
	}
	if (CurrentStats.MinNeighborCount == TNumericLimits<int32>::Max())
	{
		CurrentStats.MinNeighborCount = 0;
	}
	if (CurrentStats.MaxNeighborCount == TNumericLimits<int32>::Lowest())
	{
		CurrentStats.MaxNeighborCount = 0;
	}

	// Update engine stats
	UpdateEngineStats();

	bFrameActive = false;
}

void FKawaiiFluidSimulationStatsCollector::SetParticleCounts(int32 Total, int32 Active, int32 Attached)
{
	CurrentStats.ParticleCount = Total;
	CurrentStats.ActiveParticleCount = Active;
	CurrentStats.AttachedParticleCount = Attached;
}

void FKawaiiFluidSimulationStatsCollector::AddVelocitySample(float VelocityMagnitude)
{
	if (!bEnabled || !bFrameActive)
	{
		return;
	}

	VelocitySum += VelocityMagnitude;
	VelocitySampleCount++;

	CurrentStats.MinVelocity = FMath::Min(CurrentStats.MinVelocity, VelocityMagnitude);
	CurrentStats.MaxVelocity = FMath::Max(CurrentStats.MaxVelocity, VelocityMagnitude);
}

void FKawaiiFluidSimulationStatsCollector::AddDensitySample(float Density)
{
	if (!bEnabled || !bFrameActive)
	{
		return;
	}

	DensitySum += Density;
	DensitySampleCount++;

	CurrentStats.MinDensity = FMath::Min(CurrentStats.MinDensity, Density);
	CurrentStats.MaxDensity = FMath::Max(CurrentStats.MaxDensity, Density);
}

void FKawaiiFluidSimulationStatsCollector::AddNeighborCountSample(int32 NeighborCount)
{
	if (!bEnabled || !bFrameActive)
	{
		return;
	}

	NeighborSum += NeighborCount;
	NeighborSampleCount++;

	CurrentStats.MinNeighborCount = FMath::Min(CurrentStats.MinNeighborCount, NeighborCount);
	CurrentStats.MaxNeighborCount = FMath::Max(CurrentStats.MaxNeighborCount, NeighborCount);
}

void FKawaiiFluidSimulationStatsCollector::AddPressureCorrectionSample(float CorrectionMagnitude)
{
	if (!bEnabled || !bFrameActive)
	{
		return;
	}

	PressureCorrectionSum += CorrectionMagnitude;
	PressureCorrectionSampleCount++;
}

void FKawaiiFluidSimulationStatsCollector::AddViscosityForceSample(float ForceMagnitude)
{
	if (!bEnabled || !bFrameActive)
	{
		return;
	}

	ViscosityForceSum += ForceMagnitude;
	ViscosityForceSampleCount++;
}

void FKawaiiFluidSimulationStatsCollector::AddCohesionForceSample(float ForceMagnitude)
{
	if (!bEnabled || !bFrameActive)
	{
		return;
	}

	CohesionForceSum += ForceMagnitude;
	CohesionForceSampleCount++;
}

void FKawaiiFluidSimulationStatsCollector::CalculateStabilityMetrics(
	const float* Densities,
	const float* Velocities,
	const float* Masses,
	int32 Count,
	float RestDensity)
{
	if (!bEnabled || Count <= 0 || !Densities || !Velocities)
	{
		return;
	}

	// Pass 1: Calculate means
	double DensityMean = 0.0;
	double VelocityMean = 0.0;
	double KineticEnergySum = 0.0;
	double PerParticleErrorSum = 0.0;

	for (int32 i = 0; i < Count; ++i)
	{
		const float Density = Densities[i];
		const float Velocity = Velocities[i];
		const float Mass = Masses ? Masses[i] : 1.0f;

		DensityMean += Density;
		VelocityMean += Velocity;

		// Kinetic energy: 0.5 * m * v²
		KineticEnergySum += 0.5 * Mass * Velocity * Velocity;

		// Per-particle density error: |ρᵢ - ρ₀| / ρ₀ * 100
		if (RestDensity > 0.001f)
		{
			PerParticleErrorSum += FMath::Abs(Density - RestDensity) / RestDensity * 100.0;
		}
	}

	DensityMean /= Count;
	VelocityMean /= Count;

	// Pass 2: Calculate standard deviations
	double DensityVariance = 0.0;
	double VelocityVariance = 0.0;

	for (int32 i = 0; i < Count; ++i)
	{
		const double DensityDiff = Densities[i] - DensityMean;
		const double VelocityDiff = Velocities[i] - VelocityMean;

		DensityVariance += DensityDiff * DensityDiff;
		VelocityVariance += VelocityDiff * VelocityDiff;
	}

	DensityVariance /= Count;
	VelocityVariance /= Count;

	// Store results
	CurrentStats.DensityStdDev = static_cast<float>(FMath::Sqrt(DensityVariance));
	CurrentStats.VelocityStdDev = static_cast<float>(FMath::Sqrt(VelocityVariance));
	CurrentStats.PerParticleDensityError = static_cast<float>(PerParticleErrorSum / Count);
	CurrentStats.KineticEnergy = static_cast<float>(KineticEnergySum / Count);

	// Calculate Stability Score (0-100, higher = more stable)
	// Factors:
	//   1. PerParticleDensityError: target < 3% for stable, > 10% is unstable
	//   2. VelocityStdDev: target < 5 cm/s for stable, > 50 cm/s is chaotic
	//   3. AvgVelocity: target < 10 cm/s for "resting" fluid
	//   4. KineticEnergy: lower is better for resting fluid

	// Density error score (0-25): 3% error = 25, 10% error = 0
	const float DensityErrorScore = FMath::Clamp(25.0f - (CurrentStats.PerParticleDensityError - 3.0f) * (25.0f / 7.0f), 0.0f, 25.0f);

	// Velocity StdDev score (0-25): 5 cm/s = 25, 50 cm/s = 0
	const float VelocityStdDevScore = FMath::Clamp(25.0f - (CurrentStats.VelocityStdDev - 5.0f) * (25.0f / 45.0f), 0.0f, 25.0f);

	// Average velocity score (0-25): 10 cm/s = 25, 100 cm/s = 0
	const float AvgVelocity = static_cast<float>(VelocityMean);
	const float AvgVelocityScore = FMath::Clamp(25.0f - (AvgVelocity - 10.0f) * (25.0f / 90.0f), 0.0f, 25.0f);

	// Max velocity score (0-25): 50 cm/s = 25, 500 cm/s = 0
	const float MaxVelocityScore = FMath::Clamp(25.0f - (CurrentStats.MaxVelocity - 50.0f) * (25.0f / 450.0f), 0.0f, 25.0f);

	CurrentStats.StabilityScore = DensityErrorScore + VelocityStdDevScore + AvgVelocityScore + MaxVelocityScore;
}

void FKawaiiFluidSimulationStatsCollector::UpdateEngineStats() const
{
	SET_DWORD_STAT(STAT_FluidParticleCount, CurrentStats.ParticleCount);
	SET_DWORD_STAT(STAT_FluidActiveParticles, CurrentStats.ActiveParticleCount);
	SET_DWORD_STAT(STAT_FluidAttachedParticles, CurrentStats.AttachedParticleCount);
	SET_DWORD_STAT(STAT_FluidSubstepCount, CurrentStats.SubstepCount);

	SET_FLOAT_STAT(STAT_FluidAvgVelocity, CurrentStats.AvgVelocity);
	SET_FLOAT_STAT(STAT_FluidMaxVelocity, CurrentStats.MaxVelocity);
	SET_FLOAT_STAT(STAT_FluidAvgDensity, CurrentStats.AvgDensity);
	SET_FLOAT_STAT(STAT_FluidDensityError, CurrentStats.DensityError);
	SET_FLOAT_STAT(STAT_FluidAvgNeighbors, CurrentStats.AvgNeighborCount);

	// Stability metrics (GPU detailed mode)
	SET_FLOAT_STAT(STAT_FluidDensityStdDev, CurrentStats.DensityStdDev);
	SET_FLOAT_STAT(STAT_FluidVelocityStdDev, CurrentStats.VelocityStdDev);
	SET_FLOAT_STAT(STAT_FluidPerParticleError, CurrentStats.PerParticleDensityError);
	SET_FLOAT_STAT(STAT_FluidKineticEnergy, CurrentStats.KineticEnergy);
	SET_FLOAT_STAT(STAT_FluidStabilityScore, CurrentStats.StabilityScore);
}

//=============================================================================
// Global Stat Collector Instance
//=============================================================================

static FKawaiiFluidSimulationStatsCollector GFluidStatsCollector;

FKawaiiFluidSimulationStatsCollector& GetFluidStatsCollector()
{
	return GFluidStatsCollector;
}

//=============================================================================
// Console Command
//=============================================================================

static void HandleFluidStatsCommand(const TArray<FString>& Args)
{
	FKawaiiFluidStatsCommand::HandleStatsCommand(Args, nullptr);
}

FAutoConsoleCommand FKawaiiFluidStatsCommand::StatsCommand(
	TEXT("KawaiiFluidSimulation.Stats"),
	TEXT("Fluid simulation statistics\n")
	TEXT("  KawaiiFluidSimulation.Stats on          - Enable stat collection\n")
	TEXT("  KawaiiFluidSimulation.Stats off         - Disable stat collection\n")
	TEXT("  KawaiiFluidSimulation.Stats detailed on - Enable GPU detailed stats (causes readback)\n")
	TEXT("  KawaiiFluidSimulation.Stats detailed off- Disable GPU detailed stats\n")
	TEXT("  KawaiiFluidSimulation.Stats show        - Show current stats\n")
	TEXT("  KawaiiFluidSimulation.Stats log         - Log stats to output log\n")
	TEXT("  KawaiiFluidSimulation.Stats reset       - Reset statistics"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleFluidStatsCommand),
	ECVF_Default
);

void FKawaiiFluidStatsCommand::Register()
{
	// Console command is auto-registered via FAutoConsoleCommand
}

void FKawaiiFluidStatsCommand::Unregister()
{
	// Console command is auto-unregistered
}

void FKawaiiFluidStatsCommand::HandleStatsCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() == 0)
	{
		// Show help
		UE_LOG(LogTemp, Log, TEXT("KawaiiFluidSimulation.Stats - Fluid simulation statistics"));
		UE_LOG(LogTemp, Log, TEXT("Usage:"));
		UE_LOG(LogTemp, Log, TEXT("  KawaiiFluidSimulation.Stats on          - Enable stat collection"));
		UE_LOG(LogTemp, Log, TEXT("  KawaiiFluidSimulation.Stats off         - Disable stat collection"));
		UE_LOG(LogTemp, Log, TEXT("  KawaiiFluidSimulation.Stats detailed on - Enable GPU detailed stats (causes readback)"));
		UE_LOG(LogTemp, Log, TEXT("  KawaiiFluidSimulation.Stats detailed off- Disable GPU detailed stats"));
		UE_LOG(LogTemp, Log, TEXT("  KawaiiFluidSimulation.Stats show        - Show current stats"));
		UE_LOG(LogTemp, Log, TEXT("  KawaiiFluidSimulation.Stats log         - Log stats to output log"));
		UE_LOG(LogTemp, Log, TEXT("  KawaiiFluidSimulation.Stats reset       - Reset statistics"));
		UE_LOG(LogTemp, Log, TEXT("Currently: %s, Detailed GPU: %s"),
			GetFluidStatsCollector().IsEnabled() ? TEXT("ENABLED") : TEXT("DISABLED"),
			GetFluidStatsCollector().IsDetailedGPUEnabled() ? TEXT("ON") : TEXT("OFF"));
		return;
	}

	const FString& Command = Args[0].ToLower();

	if (Command == TEXT("on") || Command == TEXT("enable") || Command == TEXT("1"))
	{
		GetFluidStatsCollector().SetEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("Fluid stats collection ENABLED"));

		// Auto-enable stat display
		if (GEngine)
		{
			GEngine->Exec(nullptr, TEXT("stat KawaiiFluidSimulation"));
		}
	}
	else if (Command == TEXT("off") || Command == TEXT("disable") || Command == TEXT("0"))
	{
		GetFluidStatsCollector().SetEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("Fluid stats collection DISABLED"));

		// Auto-disable stat display
		if (GEngine)
		{
			GEngine->Exec(nullptr, TEXT("stat KawaiiFluidSimulation"));
		}
	}
	else if (Command == TEXT("detailed"))
	{
		// Handle "detailed on/off"
		if (Args.Num() >= 2)
		{
			const FString& SubCommand = Args[1].ToLower();
			if (SubCommand == TEXT("on") || SubCommand == TEXT("1"))
			{
				GetFluidStatsCollector().SetDetailedGPUEnabled(true);
				UE_LOG(LogTemp, Log, TEXT("GPU detailed stats ENABLED (will cause GPU readback - may affect performance)"));
			}
			else if (SubCommand == TEXT("off") || SubCommand == TEXT("0"))
			{
				GetFluidStatsCollector().SetDetailedGPUEnabled(false);
				UE_LOG(LogTemp, Log, TEXT("GPU detailed stats DISABLED"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: KawaiiFluidSimulation.Stats detailed on/off"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("GPU detailed stats: %s"),
				GetFluidStatsCollector().IsDetailedGPUEnabled() ? TEXT("ON") : TEXT("OFF"));
			UE_LOG(LogTemp, Log, TEXT("Usage: KawaiiFluidSimulation.Stats detailed on/off"));
		}
	}
	else if (Command == TEXT("show"))
	{
		if (!GetFluidStatsCollector().IsEnabled())
		{
			UE_LOG(LogTemp, Warning, TEXT("Stats collection is disabled. Use 'KawaiiFluidSimulation.Stats on' first."));
			return;
		}

		const FKawaiiFluidSimulationStats& Stats = GetFluidStatsCollector().GetStats();
		UE_LOG(LogTemp, Log, TEXT("%s"), *Stats.ToString());
	}
	else if (Command == TEXT("log"))
	{
		if (!GetFluidStatsCollector().IsEnabled())
		{
			UE_LOG(LogTemp, Warning, TEXT("Stats collection is disabled. Use 'KawaiiFluidSimulation.Stats on' first."));
			return;
		}

		const FKawaiiFluidSimulationStats& Stats = GetFluidStatsCollector().GetStats();
		Stats.LogStats();
		UE_LOG(LogTemp, Log, TEXT("Stats logged to Output Log"));
	}
	else if (Command == TEXT("reset"))
	{
		GetFluidStatsCollector().BeginFrame();  // This resets
		UE_LOG(LogTemp, Log, TEXT("Stats reset"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown command: %s"), *Command);
		UE_LOG(LogTemp, Log, TEXT("Use 'KawaiiFluidSimulation.Stats' for help"));
	}
}
