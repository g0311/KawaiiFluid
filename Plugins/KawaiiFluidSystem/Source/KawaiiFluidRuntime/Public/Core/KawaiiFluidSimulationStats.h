// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"

DECLARE_STATS_GROUP(TEXT("KawaiiFluidSimulation"), STATGROUP_KawaiiFluidSimulation, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Total Simulation"), STAT_FluidTotalSimulation, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Spatial Hash"), STAT_FluidSpatialHash, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Density Solve"), STAT_FluidDensitySolve, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Viscosity"), STAT_FluidViscosity, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Cohesion"), STAT_FluidCohesion, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Collision"), STAT_FluidCollision, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Simulation"), STAT_FluidGPUSimulation, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Readback"), STAT_FluidGPUReadback, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Particle Count"), STAT_FluidParticleCount, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Active Particles"), STAT_FluidActiveParticles, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Attached Particles"), STAT_FluidAttachedParticles, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Substep Count"), STAT_FluidSubstepCount, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);

DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Avg Velocity (cm/s)"), STAT_FluidAvgVelocity, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Max Velocity (cm/s)"), STAT_FluidMaxVelocity, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Avg Density"), STAT_FluidAvgDensity, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Density Error %"), STAT_FluidDensityError, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Avg Neighbors"), STAT_FluidAvgNeighbors, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);

DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Density StdDev"), STAT_FluidDensityStdDev, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Velocity StdDev"), STAT_FluidVelocityStdDev, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Per-Particle Error %"), STAT_FluidPerParticleError, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Kinetic Energy"), STAT_FluidKineticEnergy, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Stability Score"), STAT_FluidStabilityScore, STATGROUP_KawaiiFluidSimulation, KAWAIIFLUIDRUNTIME_API);

/**
 * @struct FKawaiiFluidSimulationStats
 * @brief Statistics snapshot for a single frame used to monitor and compare simulation behavior.
 * 
 * @param ParticleCount Total number of particles in the system.
 * @param ActiveParticleCount Number of particles currently being simulated (non-attached).
 * @param AttachedParticleCount Number of particles currently attached to actors or bones.
 * @param AvgVelocity Average velocity magnitude across all particles (cm/s).
 * @param MinVelocity Minimum velocity magnitude (cm/s).
 * @param MaxVelocity Maximum velocity magnitude (cm/s).
 * @param AvgDensity Average density calculated across all particles.
 * @param MinDensity Minimum calculated density.
 * @param MaxDensity Maximum calculated density.
 * @param DensityError Average relative deviation from rest density (%).
 * @param RestDensity Target rest density used for error calculation.
 * @param DensityStdDev Standard deviation of density values (GPU detailed mode).
 * @param VelocityStdDev Standard deviation of velocity magnitudes (GPU detailed mode).
 * @param PerParticleDensityError Mean absolute density error per particle (%).
 * @param KineticEnergy Average kinetic energy of the fluid system (0.5 * m * v²).
 * @param StabilityScore Composite score (0-100) indicating simulation stability.
 * @param AvgNeighborCount Average number of neighbors per particle.
 * @param MinNeighborCount Minimum neighbor count found.
 * @param MaxNeighborCount Maximum neighbor count found.
 * @param AvgPressureCorrection Average displacement correction from density solver.
 * @param AvgViscosityForce Average force magnitude from viscosity solver.
 * @param AvgCohesionForce Average force magnitude from cohesion/surface tension solver.
 * @param BoundsCollisionCount Number of collisions with simulation volume boundaries.
 * @param PrimitiveCollisionCount Number of collisions with explicitly registered colliders.
 * @param GroundContactCount Number of particles in contact with the world geometry/ground.
 * @param SubstepCount Number of substeps executed in the current frame.
 * @param SolverIterations Number of solver iterations per substep.
 * @param TotalSimulationTimeMs Total CPU/GPU time for simulation in milliseconds.
 * @param SpatialHashTimeMs Time spent building and querying the spatial hash.
 * @param DensitySolveTimeMs Time spent in the PBF density constraint solver.
 * @param ViscosityTimeMs Time spent in the viscosity solver.
 * @param CohesionTimeMs Time spent in the cohesion/surface tension solver.
 * @param CollisionTimeMs Time spent resolving all types of collisions.
 * @param GPUSimulationTimeMs Time spent on GPU compute shaders.
 * @param GPUReadbackTimeMs Time spent transferring data from GPU to CPU.
 * @param bIsGPUSimulation Flag indicating if this is a GPU-based simulation.
 */
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidSimulationStats
{
public:
	int32 ParticleCount = 0;
	int32 ActiveParticleCount = 0;
	int32 AttachedParticleCount = 0;

	float AvgVelocity = 0.0f;
	float MinVelocity = 0.0f;
	float MaxVelocity = 0.0f;

	float AvgDensity = 0.0f;
	float MinDensity = 0.0f;
	float MaxDensity = 0.0f;
	float DensityError = 0.0f;
	float RestDensity = 1000.0f;

	float DensityStdDev = 0.0f;
	float VelocityStdDev = 0.0f;
	float PerParticleDensityError = 0.0f;
	float KineticEnergy = 0.0f;
	float StabilityScore = 0.0f;

	float AvgNeighborCount = 0.0f;
	int32 MinNeighborCount = 0;
	int32 MaxNeighborCount = 0;

	float AvgPressureCorrection = 0.0f;
	float AvgViscosityForce = 0.0f;
	float AvgCohesionForce = 0.0f;

	int32 BoundsCollisionCount = 0;
	int32 PrimitiveCollisionCount = 0;
	int32 GroundContactCount = 0;

	int32 SubstepCount = 0;
	int32 SolverIterations = 0;

	double TotalSimulationTimeMs = 0.0;
	double SpatialHashTimeMs = 0.0;
	double DensitySolveTimeMs = 0.0;
	double ViscosityTimeMs = 0.0;
	double CohesionTimeMs = 0.0;
	double CollisionTimeMs = 0.0;
	double GPUSimulationTimeMs = 0.0;
	double GPUReadbackTimeMs = 0.0;

	bool bIsGPUSimulation = false;

	void Reset();

	void LogStats(const FString& Label = TEXT("")) const;

	FString ToString() const;

	FString CompareWith(const FKawaiiFluidSimulationStats& Other, const FString& OtherLabel = TEXT("Other")) const;
};

/**
 * @class FKawaiiFluidSimulationStatsCollector
 * @brief Collector class that accumulates and aggregates statistics during the simulation loop.
 * 
 * @param CurrentStats Statistics currently being collected for the active frame.
 * @param PreviousStats Finalized statistics from the last completed frame.
 * @param VelocitySum Accumulator for averaging particle velocity.
 * @param VelocitySampleCount Number of velocity samples collected.
 * @param DensitySum Accumulator for averaging particle density.
 * @param DensitySampleCount Number of density samples collected.
 * @param NeighborSum Accumulator for averaging neighbor counts.
 * @param NeighborSampleCount Number of neighbor count samples.
 * @param PressureCorrectionSum Accumulator for averaging pressure correction displacement.
 * @param PressureCorrectionSampleCount Number of pressure correction samples.
 * @param ViscosityForceSum Accumulator for averaging viscosity forces.
 * @param ViscosityForceSampleCount Number of viscosity force samples.
 * @param CohesionForceSum Accumulator for averaging cohesion forces.
 * @param CohesionForceSampleCount Number of cohesion force samples.
 * @param bEnabled Global flag to enable or disable statistics collection.
 * @param bDetailedGPU Enable detailed GPU metrics that require expensive readbacks.
 * @param bReadbackRequested Flag indicating if a debug readback is needed for visualization.
 * @param bFrameActive Internal state flag ensuring BeginFrame/EndFrame pairing.
 */
class KAWAIIFLUIDRUNTIME_API FKawaiiFluidSimulationStatsCollector
{
public:
	FKawaiiFluidSimulationStatsCollector() = default;

	void BeginFrame();

	void EndFrame();

	const FKawaiiFluidSimulationStats& GetStats() const { return CurrentStats; }

	const FKawaiiFluidSimulationStats& GetPreviousStats() const { return PreviousStats; }

	bool IsEnabled() const { return bEnabled; }

	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	bool IsDetailedGPUEnabled() const { return bDetailedGPU; }

	void SetDetailedGPUEnabled(bool bInDetailedGPU) { bDetailedGPU = bInDetailedGPU; }

	bool IsReadbackRequested() const { return bReadbackRequested; }

	void SetReadbackRequested(bool bInRequested) { bReadbackRequested = bInRequested; }

	bool IsAnyReadbackNeeded() const { return bDetailedGPU || bReadbackRequested; }

	void SetParticleCounts(int32 Total, int32 Active, int32 Attached);

	void AddVelocitySample(float VelocityMagnitude);

	void AddDensitySample(float Density);

	void AddNeighborCountSample(int32 NeighborCount);

	void SetRestDensity(float InRestDensity) { CurrentStats.RestDensity = InRestDensity; }

	void CalculateStabilityMetrics(
		const float* Densities,
		const float* Velocities,
		const float* Masses,
		int32 Count,
		float RestDensity);

	void AddPressureCorrectionSample(float CorrectionMagnitude);

	void AddViscosityForceSample(float ForceMagnitude);

	void AddCohesionForceSample(float ForceMagnitude);

	void AddBoundsCollision() { CurrentStats.BoundsCollisionCount++; }

	void AddPrimitiveCollision() { CurrentStats.PrimitiveCollisionCount++; }

	void AddGroundContact() { CurrentStats.GroundContactCount++; }

	void SetSubstepCount(int32 Count) { CurrentStats.SubstepCount = Count; }

	void SetSolverIterations(int32 Iterations) { CurrentStats.SolverIterations = Iterations; }

	void SetGPUSimulation(bool bGPU) { CurrentStats.bIsGPUSimulation = bGPU; }

	void SetTotalSimulationTime(double Ms) { CurrentStats.TotalSimulationTimeMs = Ms; }
	void SetSpatialHashTime(double Ms) { CurrentStats.SpatialHashTimeMs = Ms; }
	void SetDensitySolveTime(double Ms) { CurrentStats.DensitySolveTimeMs = Ms; }
	void SetViscosityTime(double Ms) { CurrentStats.ViscosityTimeMs = Ms; }
	void SetCohesionTime(double Ms) { CurrentStats.CohesionTimeMs = Ms; }
	void SetCollisionTime(double Ms) { CurrentStats.CollisionTimeMs = Ms; }
	void SetGPUSimulationTime(double Ms) { CurrentStats.GPUSimulationTimeMs = Ms; }
	void SetGPUReadbackTime(double Ms) { CurrentStats.GPUReadbackTimeMs = Ms; }

	void UpdateEngineStats() const;

private:
	FKawaiiFluidSimulationStats CurrentStats;
	FKawaiiFluidSimulationStats PreviousStats;

	double VelocitySum = 0.0;
	int32 VelocitySampleCount = 0;

	double DensitySum = 0.0;
	int32 DensitySampleCount = 0;

	double NeighborSum = 0.0;
	int32 NeighborSampleCount = 0;

	double PressureCorrectionSum = 0.0;
	int32 PressureCorrectionSampleCount = 0;

	double ViscosityForceSum = 0.0;
	int32 ViscosityForceSampleCount = 0;

	double CohesionForceSum = 0.0;
	int32 CohesionForceSampleCount = 0;

	bool bEnabled = false;
	bool bDetailedGPU = false;
	bool bReadbackRequested = false;
	bool bFrameActive = false;
};

KAWAIIFLUIDRUNTIME_API FKawaiiFluidSimulationStatsCollector& GetFluidStatsCollector();

/**
 * @class FKawaiiFluidStatsCommand
 * @brief Console command handler for the "KawaiiFluidSimulation.Stats" command.
 * 
 * @param StatsCommand The auto-registered console command instance.
 */
class KAWAIIFLUIDRUNTIME_API FKawaiiFluidStatsCommand
{
public:
	static void Register();

	static void Unregister();

	static void HandleStatsCommand(const TArray<FString>& Args, UWorld* World);

private:
	static FAutoConsoleCommand StatsCommand;
};
