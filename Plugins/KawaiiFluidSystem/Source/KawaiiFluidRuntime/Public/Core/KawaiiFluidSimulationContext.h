// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/KawaiiFluidParticle.h"
#include "Core/KawaiiFluidSimulationTypes.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Components/KawaiiFluidVolumeComponent.h"
#include "KawaiiFluidSimulationContext.generated.h"

// Forward declarations
class FKawaiiFluidSpatialHash;
class FKawaiiFluidDensityConstraint;
class FKawaiiFluidViscositySolver;
class FKawaiiFluidAdhesionSolver;
class FKawaiiFluidStackPressureSolver;
class FGPUFluidSimulator;
class FKawaiiFluidRenderResource;
struct FGPUFluidSimulationParams;

/**
 * @brief Stateless Simulation Context containing pure simulation logic.
 * 
 * This class coordinates the simulation flow by dispatching tasks to various solvers.
 * It does not own the simulation state (particles), making it reusable across different fluid components.
 * 
 * @param DensityConstraint Solver for enforcing fluid incompressibility via XPBD.
 * @param ViscositySolver Solver for applying XSPH-based viscosity.
 * @param AdhesionSolver Solver for surface tension and cohesion forces.
 * @param StackPressureSolver Solver for transferring weight between stacked attached particles.
 * @param bSolversInitialized Internal flag indicating if the solvers have been initialized.
 * @param GPUSimulator The GPU simulator instance for compute-shader based simulation.
 * @param RenderResource Shared resources for batched rendering across multiple components.
 * @param PersistentBoneTransforms Bone transforms from the previous frame used for velocity calculation.
 * @param PersistentBoneNameToIndex Mapping of bone names to indices for consistent tracking.
 * @param CachedPreset Weak reference to the preset currently being used.
 * @param TargetVolumeComponent Weak reference to the volume component providing simulation bounds.
 * @param CachedGPUWorldCollisionPrimitives Cached geometry primitives for GPU world collision.
 * @param CachedGPUWorldCollisionBounds The world-space bounds for the current collision cache.
 * @param CachedGPUWorldCollisionWorld The world associated with the current collision cache.
 * @param bGPUWorldCollisionCacheDirty Flag to trigger a rebuild of the world collision cache.
 * @param bStaticBoundaryParticlesDirty Flag to trigger regeneration of static boundary particles.
 * @param CachedLandscapeHeightmap Sampled heightmap data for landscape collision.
 * @param CachedHeightmapWidth Width of the cached heightmap texture.
 * @param CachedHeightmapHeight Height of the cached heightmap texture.
 * @param CachedHeightmapBounds World-space bounds of the extracted landscape area.
 * @param bLandscapeHeightmapDirty Flag to trigger re-extraction of landscape data.
 */
UCLASS(BlueprintType, Blueprintable)
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidSimulationContext : public UObject
{
	GENERATED_BODY()

public:
	UKawaiiFluidSimulationContext();
	virtual ~UKawaiiFluidSimulationContext() override;

	virtual void InitializeSolvers(const UKawaiiFluidPresetDataAsset* Preset);

	//========================================
	// GPU Simulation Mode
	//========================================

	virtual void InitializeGPUSimulator(int32 MaxParticleCount);

	virtual void ReleaseGPUSimulator();

	bool IsGPUSimulatorReady() const;

	FGPUFluidSimulator* GetGPUSimulator() const { return GPUSimulator.Get(); }

	TSharedPtr<FGPUFluidSimulator> GetGPUSimulatorShared() const { return GPUSimulator; }

	UKawaiiFluidPresetDataAsset* GetCachedPreset() const { return CachedPreset.Get(); }

	void SetCachedPreset(UKawaiiFluidPresetDataAsset* InPreset) { CachedPreset = InPreset; }

	void MarkGPUWorldCollisionCacheDirty() { bGPUWorldCollisionCacheDirty = true; }

	void MarkLandscapeHeightmapDirty() { bLandscapeHeightmapDirty = true; }

	//========================================
	// Target Volume Component (Z-Order Space Bounds)
	//========================================

	UKawaiiFluidVolumeComponent* GetTargetVolumeComponent() const { return TargetVolumeComponent.Get(); }

	void SetTargetVolumeComponent(UKawaiiFluidVolumeComponent* InVolumeComponent) { TargetVolumeComponent = InVolumeComponent; }

	//========================================
	// Render Resource (for batch rendering)
	//========================================

	void InitializeRenderResource();

	void ReleaseRenderResource();

	FKawaiiFluidRenderResource* GetRenderResource() const { return RenderResource.Get(); }

	TSharedPtr<FKawaiiFluidRenderResource> GetRenderResourceShared() const { return RenderResource; }

	bool HasValidRenderResource() const { return RenderResource.IsValid(); }

	virtual void Simulate(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		FKawaiiFluidSpatialHash& SpatialHash,
		float DeltaTime,
		float& AccumulatedTime
	);

	virtual void SimulateSubstep(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		FKawaiiFluidSpatialHash& SpatialHash,
		float SubstepDT
	);

	void RunInitializationSimulation(
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		int32 ParticleCount
	);

protected:
	//========================================
	// Simulation Steps
	//========================================

	virtual void PredictPositions(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FVector& ExternalForce,
		float DeltaTime
	);

	virtual void UpdateNeighbors(
		TArray<FKawaiiFluidParticle>& Particles,
		FKawaiiFluidSpatialHash& SpatialHash,
		float SmoothingRadius
	);

	virtual void SolveDensityConstraints(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		float DeltaTime
	);

	virtual void HandleCollisions(
		TArray<FKawaiiFluidParticle>& Particles,
		const TArray<TObjectPtr<UKawaiiFluidCollider>>& Colliders,
		float SubstepDT
	);

	virtual void HandleWorldCollision(
		TArray<FKawaiiFluidParticle>& Particles,
		const FKawaiiFluidSimulationParams& Params,
		FKawaiiFluidSpatialHash& SpatialHash,
		float ParticleRadius,
		float SubstepDT,
		float Friction = 0.5f,
		float Restitution = 0.0f
	);

	virtual void HandleWorldCollision_Sweep(
		TArray<FKawaiiFluidParticle>& Particles,
		const FKawaiiFluidSimulationParams& Params,
		FKawaiiFluidSpatialHash& SpatialHash,
		float ParticleRadius,
		float SubstepDT,
		float Friction,
		float Restitution
	);

	virtual void HandleWorldCollision_SDF(
		TArray<FKawaiiFluidParticle>& Particles,
		const FKawaiiFluidSimulationParams& Params,
		FKawaiiFluidSpatialHash& SpatialHash,
		float ParticleRadius,
		float SubstepDT,
		float Friction,
		float Restitution
	);

	virtual void FinalizePositions(
		TArray<FKawaiiFluidParticle>& Particles,
		float DeltaTime
	);

	virtual void ApplyViscosity(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset
	);

	virtual void ApplyAdhesion(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const TArray<TObjectPtr<UKawaiiFluidCollider>>& Colliders
	);

	virtual void ApplyCohesion(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset
	);

	virtual void CollectSimulationStats(
		const TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		int32 SubstepCount,
		bool bIsGPU
	);

	virtual void CollectGPUSimulationStats(
		const UKawaiiFluidPresetDataAsset* Preset,
		int32 ParticleCount,
		int32 SubstepCount
	);

	virtual void UpdateAttachedParticlePositions(
		TArray<FKawaiiFluidParticle>& Particles,
		const TArray<TObjectPtr<UKawaiiFluidInteractionComponent>>& InteractionComponents
	);

	virtual void CacheColliderShapes(const TArray<TObjectPtr<UKawaiiFluidCollider>>& Colliders);

	void AppendGPUWorldCollisionPrimitives(
		FGPUCollisionPrimitives& OutPrimitives,
		const FKawaiiFluidSimulationParams& Params,
		const FBox& QueryBounds,
		float DefaultFriction,
		float DefaultRestitution,
		const TSet<const AActor*>& FluidColliderOwners
	);

protected:
	//========================================
	// Cached Solvers
	//========================================

	TSharedPtr<FKawaiiFluidDensityConstraint> DensityConstraint;

	TSharedPtr<FKawaiiFluidViscositySolver> ViscositySolver;

	TSharedPtr<FKawaiiFluidAdhesionSolver> AdhesionSolver;

	TSharedPtr<FKawaiiFluidStackPressureSolver> StackPressureSolver;

	bool bSolversInitialized = false;

	void EnsureSolversInitialized(const UKawaiiFluidPresetDataAsset* Preset);

	//========================================
	// GPU Simulation
	//========================================

	TSharedPtr<FGPUFluidSimulator> GPUSimulator;

	//========================================
	// Render Resource
	//========================================

	TSharedPtr<FKawaiiFluidRenderResource> RenderResource;

	virtual void SimulateGPU(
		TArray<FKawaiiFluidParticle>& Particles,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		FKawaiiFluidSpatialHash& SpatialHash,
		float DeltaTime,
		float& AccumulatedTime
	);

	FGPUFluidSimulationParams BuildGPUSimParams(
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		float SubstepDT
	) const;

	TArray<int32> ExtractAttachedParticleIndices(const TArray<FKawaiiFluidParticle>& Particles) const;

	virtual void HandleAttachedParticlesCPU(
		TArray<FKawaiiFluidParticle>& Particles,
		const TArray<int32>& AttachedIndices,
		const UKawaiiFluidPresetDataAsset* Preset,
		const FKawaiiFluidSimulationParams& Params,
		float SubstepDT
	);

	//========================================
	// Persistent Bone Transform Data
	//========================================

	TArray<FGPUBoneTransform> PersistentBoneTransforms;

	TMap<FName, int32> PersistentBoneNameToIndex;

	TWeakObjectPtr<UKawaiiFluidPresetDataAsset> CachedPreset;

	TWeakObjectPtr<UKawaiiFluidVolumeComponent> TargetVolumeComponent;

	//========================================
	// GPU World Collision Cache
	//========================================

	FGPUCollisionPrimitives CachedGPUWorldCollisionPrimitives;

	FBox CachedGPUWorldCollisionBounds = FBox(EForceInit::ForceInit);

	TWeakObjectPtr<UWorld> CachedGPUWorldCollisionWorld;

	bool bGPUWorldCollisionCacheDirty = true;

	bool bStaticBoundaryParticlesDirty = true;

	//========================================
	// Landscape Heightmap Collision Cache
	//========================================

	TArray<float> CachedLandscapeHeightmap;

	int32 CachedHeightmapWidth = 0;
	int32 CachedHeightmapHeight = 0;

	FBox CachedHeightmapBounds = FBox(EForceInit::ForceInit);

	bool bLandscapeHeightmapDirty = true;

	void UpdateLandscapeHeightmapCollision(const FKawaiiFluidSimulationParams& Params, const UKawaiiFluidPresetDataAsset* Preset);
};
