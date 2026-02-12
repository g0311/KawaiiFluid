// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/KawaiiFluidParticle.h"
#include "Core/KawaiiFluidSimulationTypes.h"
#include "Core/IKawaiiFluidDataProvider.h"
#include "Simulation/GPUFluidSimulator.h"
#include "Components/KawaiiFluidInteractionComponent.h"
#include "KawaiiFluidSimulationModule.generated.h"

/** @brief Collision event callback type */
DECLARE_DELEGATE_OneParam(FOnModuleCollisionEvent, const FKawaiiFluidCollisionEvent&);

class FKawaiiFluidSpatialHash;
class UKawaiiFluidPresetDataAsset;
class UKawaiiFluidCollider;
class UKawaiiFluidInteractionComponent;
class UKawaiiFluidSimulationContext;
class UKawaiiFluidVolumeComponent;
class AKawaiiFluidVolume;

/**
 * @class UKawaiiFluidSimulationModule
 * @brief Module that owns and manages fluid simulation data and provides high-level APIs for spawning and force application.
 * 
 * Simulation modules manage the particle lifecycle and coordinate with the UKawaiiFluidSimulationContext
 * for actual physics calculations. They can operate in independent or batched modes.
 * 
 * @param TargetSimulationVolume External actor providing bounds for shared simulation space.
 * @param bUniformSize Use a single scalar for all axes of the simulation volume.
 * @param UniformVolumeSize Cube dimensions used when bUniformSize is active.
 * @param VolumeSize Non-uniform box dimensions used when bUniformSize is false.
 * @param VolumeRotation Rotation of the local simulation volume relative to the owner.
 * @param GridResolutionPreset Automatically determined resolution based on volume dimensions.
 * @param CellSize Internal spatial partitioning cell size (syncs with preset).
 * @param GridAxisBits Number of bits per axis used for Z-Order calculations.
 * @param GridResolution Number of cells along a single axis.
 * @param MaxCells Total capacity of the internal spatial grid.
 * @param BoundsExtent Scalar extent used for volume visualization and clipping.
 * @param WorldBoundsMin Calculated world-space minimum corner of the volume.
 * @param WorldBoundsMax Calculated world-space maximum corner of the volume.
 * @param bUseWorldCollision Enable or disable collision with world geometry (static/dynamic).
 * @param VolumeWireframeColor Color of the simulation bounds box in the editor.
 * @param bShowZOrderSpaceWireframe Visualization flag for internal grid partitions.
 * @param ZOrderSpaceWireframeColor Color of the internal grid wireframe.
 * @param bEnableCollisionEvents Enable emission of collision event data.
 * @param MinVelocityForEvent Speed threshold required to trigger an event callback.
 * @param MaxEventsPerFrame Limit on the number of events processed per frame.
 * @param EventCooldownPerParticle Per-particle timer to prevent event duplication.
 * @param Preset Pointer to the fluid physical properties data asset.
 * @param Particles Internal array of particle data (source of truth in CPU mode).
 * @param SpatialHash Spatial grid used specifically for independent simulation mode.
 * @param Colliders List of local colliders registered with this module.
 * @param AccumulatedExternalForce Sum of forces applied externally during the current frame.
 * @param AccumulatedTime Time remaining from previous frames for fixed-step simulation.
 * @param VolumeCenter Calculated world-space center of the simulation volume.
 * @param VolumeRotationQuat Precomputed quaternion representation of the volume rotation.
 * @param bSimulationEnabled Global toggle for the module's simulation logic.
 * @param bIndependentSimulation If true, this module simulates in its own context instead of batching.
 * @param bIsInitialized Flag indicating if the module has completed its setup.
 * @param ParticleLastEventTime Tracking map for particle-specific event cooldowns.
 * @param WeakGPUSimulator Weak pointer to the shared GPU simulator instance.
 * @param bGPUSimulationActive Flag indicating if GPU-based simulation is currently used.
 * @param CachedSimulationContext Reference to the context assigned during subsystem registration.
 * @param OwnedVolumeComponent Internally managed component providing bounds data.
 * @param PreviousRegisteredVolume Tracking reference for volume re-registration in the editor.
 * @param bBoundToVolumeDestroyed Internal state for volume destruction event tracking.
 * @param CachedSourceID Assigned unique identifier for GPU-side source tracking.
 */
UCLASS(DefaultToInstanced, EditInlineNew, BlueprintType)
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidSimulationModule : public UObject, public IKawaiiFluidDataProvider
{
	GENERATED_BODY()

public:
	UKawaiiFluidSimulationModule();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "Fluid|Module")
	virtual void Initialize(UKawaiiFluidPresetDataAsset* InPreset);

	UFUNCTION(BlueprintCallable, Category = "Fluid|Module")
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "Fluid|Module")
	bool IsInitialized() const { return bIsInitialized; }

	UFUNCTION(BlueprintCallable, Category = "Fluid|Module")
	void SetPreset(UKawaiiFluidPresetDataAsset* InPreset);

	UFUNCTION(BlueprintPure, Category = "Fluid|Module")
	UKawaiiFluidPresetDataAsset* GetPreset() const { return Preset; }

	virtual FKawaiiFluidSimulationParams BuildSimulationParams() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume", meta = (DisplayName = "Target Volume (External)"))
	TObjectPtr<AKawaiiFluidVolume> TargetSimulationVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume",
		meta = (EditCondition = "TargetSimulationVolume == nullptr", EditConditionHides, DisplayName = "Uniform Size"))
	bool bUniformSize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume",
		meta = (EditCondition = "TargetSimulationVolume == nullptr && bUniformSize", EditConditionHides,
			DisplayName = "Size", ClampMin = "10.0", ClampMax = "5120.0"))
	float UniformVolumeSize = 2560.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume",
		meta = (EditCondition = "TargetSimulationVolume == nullptr && bUniformSize == false", EditConditionHides,
			DisplayName = "Size"))
	FVector VolumeSize = FVector(2560.0f, 2560.0f, 2560.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume",
		meta = (EditCondition = "TargetSimulationVolume == nullptr", EditConditionHides, DisplayName = "Rotation"))
	FRotator VolumeRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced",
		meta = (DisplayName = "Internal Grid Preset (Auto)"))
	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced",
		meta = (DisplayName = "Cell Size (Auto)"))
	float CellSize = 20.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced")
	int32 GridAxisBits = 7;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced")
	int32 GridResolution = 128;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced")
	int32 MaxCells = 2097152;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced")
	float BoundsExtent = 2560.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced")
	FVector WorldBoundsMin = FVector(-1280.0f, -1280.0f, -1280.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fluid|Simulation Volume|Advanced")
	FVector WorldBoundsMax = FVector(1280.0f, 1280.0f, 1280.0f);

	UFUNCTION(BlueprintPure, Category = "Fluid|Simulation Volume")
	AKawaiiFluidVolume* GetTargetSimulationVolume() const { return TargetSimulationVolume; }

	UFUNCTION(BlueprintCallable, Category = "Fluid|Simulation Volume")
	void SetTargetSimulationVolume(AKawaiiFluidVolume* NewSimulationVolume);

	UFUNCTION(BlueprintPure, Category = "Fluid|Simulation Volume")
	FVector GetEffectiveVolumeSize() const
	{
		return bUniformSize ? FVector(UniformVolumeSize) : VolumeSize;
	}

	FVector GetVolumeHalfExtent() const
	{
		return GetEffectiveVolumeSize() * 0.5f;
	}

	UFUNCTION(BlueprintCallable, Category = "Fluid|Simulation Volume")
	void RecalculateVolumeBounds();

	void UpdateVolumeInfoDisplay();

	void OnPresetChangedExternal(UKawaiiFluidPresetDataAsset* NewPreset);

	virtual const TArray<FKawaiiFluidParticle>& GetParticles() const override { return Particles; }

	TArray<FKawaiiFluidParticle>& GetParticlesMutable() { return Particles; }

	UFUNCTION(BlueprintPure, Category = "Fluid")
	virtual int32 GetParticleCount() const override
	{
		if (bGPUSimulationActive)
		{
			if (TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin())
			{
				return GPUSim->GetParticleCount() + GPUSim->GetPendingSpawnCount();
			}
		}
		return Particles.Num();
	}

	UFUNCTION(BlueprintPure, Category = "Fluid")
	int32 GetParticleCountForSource(int32 SourceID) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticle(FVector Position, FVector Velocity = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void SpawnParticles(FVector Location, int32 Count, float SpawnRadius);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesSphere(FVector Center, float Radius, float Spacing,
	                           bool bJitter = true, float JitterAmount = 0.2f,
	                           FVector Velocity = FVector::ZeroVector,
	                           FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesBox(FVector Center, FVector Extent, float Spacing,
	                        bool bJitter = true, float JitterAmount = 0.2f,
	                        FVector Velocity = FVector::ZeroVector,
	                        FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesCylinder(FVector Center, float Radius, float HalfHeight, float Spacing,
	                             bool bJitter = true, float JitterAmount = 0.2f,
	                             FVector Velocity = FVector::ZeroVector,
	                             FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesBoxHexagonal(FVector Center, FVector Extent, float Spacing,
	                                  bool bJitter = true, float JitterAmount = 0.2f,
	                                  FVector Velocity = FVector::ZeroVector,
	                                  FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesSphereHexagonal(FVector Center, float Radius, float Spacing,
	                                     bool bJitter = true, float JitterAmount = 0.2f,
	                                     FVector Velocity = FVector::ZeroVector,
	                                     FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesCylinderHexagonal(FVector Center, float Radius, float HalfHeight, float Spacing,
	                                       bool bJitter = true, float JitterAmount = 0.2f,
	                                       FVector Velocity = FVector::ZeroVector,
	                                       FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesSphereByCount(FVector Center, float Radius, int32 Count,
	                                  bool bJitter = true, float JitterAmount = 0.2f,
	                                  FVector Velocity = FVector::ZeroVector,
	                                  FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesBoxByCount(FVector Center, FVector Extent, int32 Count,
	                               bool bJitter = true, float JitterAmount = 0.2f,
	                               FVector Velocity = FVector::ZeroVector,
	                               FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticlesCylinderByCount(FVector Center, float Radius, float HalfHeight, int32 Count,
	                                    bool bJitter = true, float JitterAmount = 0.2f,
	                                    FVector Velocity = FVector::ZeroVector,
	                                    FRotator Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticleDirectional(FVector Position, FVector Direction, float Speed,
	                               float Radius = 0.0f, float ConeAngle = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	int32 SpawnParticleDirectionalHexLayer(FVector Position, FVector Direction, float Speed,
	                                        float Radius, float Spacing = 0.0f, float Jitter = 0.15f);

	int32 SpawnParticleDirectionalHexLayerBatch(FVector Position, FVector Direction, float Speed,
	                                             float Radius, float Spacing, float Jitter,
	                                             TArray<FGPUSpawnRequest>& OutBatch);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void ClearAllParticles();

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void DespawnByBrushGPU(FVector Center, float Radius);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void DespawnBySourceGPU(int32 SourceID);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	TArray<FVector> GetParticlePositions() const;

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	TArray<FVector> GetParticleVelocities() const;

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void ApplyExternalForce(FVector Force);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void ApplyForceToParticle(int32 ParticleIndex, FVector Force);

	FVector GetAccumulatedExternalForce() const { return AccumulatedExternalForce; }

	void ResetExternalForce() { AccumulatedExternalForce = FVector::ZeroVector; }

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void RegisterCollider(UKawaiiFluidCollider* Collider);

	UFUNCTION(BlueprintCallable, Category = "Fluid")
	void UnregisterCollider(UKawaiiFluidCollider* Collider);

	void ClearColliders() { Colliders.Empty(); }

	const TArray<TObjectPtr<UKawaiiFluidCollider>>& GetColliders() const { return Colliders; }

	FKawaiiFluidSpatialHash* GetSpatialHash() const { return SpatialHash.Get(); }

	void InitializeSpatialHash(float InCellSize);

	float GetAccumulatedTime() const { return AccumulatedTime; }
	void SetAccumulatedTime(float Time) { AccumulatedTime = Time; }

	UFUNCTION(BlueprintCallable, Category = "Fluid|Query")
	TArray<int32> GetParticlesInRadius(FVector Location, float Radius) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid|Query")
	TArray<int32> GetParticlesInBox(FVector Center, FVector Extent) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid|Query")
	bool GetParticleInfo(int32 ParticleIndex, FVector& OutPosition, FVector& OutVelocity, float& OutDensity) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid|Module")
	void SetSimulationEnabled(bool bEnabled) { bSimulationEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Fluid|Module")
	bool IsSimulationEnabled() const { return bSimulationEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Fluid|Module")
	void SetIndependentSimulation(bool bIndependent) { bIndependentSimulation = bIndependent; }

	UFUNCTION(BlueprintPure, Category = "Fluid|Module")
	bool IsIndependentSimulation() const { return bIndependentSimulation; }

	UFUNCTION(BlueprintPure, Category = "Fluid|Module")
	AActor* GetOwnerActor() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Collision")
	bool bUseWorldCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume",
		meta = (EditCondition = "TargetSimulationVolume == nullptr", EditConditionHides, DisplayName = "Wireframe Color"))
	FColor VolumeWireframeColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume|Advanced",
		meta = (EditCondition = "TargetSimulationVolume == nullptr", EditConditionHides, DisplayName = "Show Internal Grid Wireframe"))
	bool bShowZOrderSpaceWireframe = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Simulation Volume|Advanced",
		meta = (EditCondition = "TargetSimulationVolume == nullptr && bShowZOrderSpaceWireframe", EditConditionHides, DisplayName = "Grid Wireframe Color"))
	FColor ZOrderSpaceWireframeColor = FColor::Red;

	UFUNCTION(BlueprintCallable, Category = "Fluid|Simulation Volume")
	void SetSimulationVolume(const FVector& Size, const FRotator& Rotation, float Bounce, float Friction);

	void ResolveVolumeBoundaryCollisions();

	void ResolveContainmentCollisions();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events")
	bool bEnableCollisionEvents = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events", meta = (ClampMin = "0.0", EditCondition = "bEnableCollisionEvents"))
	float MinVelocityForEvent = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events", meta = (ClampMin = "0", EditCondition = "bEnableCollisionEvents"))
	int32 MaxEventsPerFrame = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events", meta = (ClampMin = "0.0", EditCondition = "bEnableCollisionEvents"))
	float EventCooldownPerParticle = 0.1f;

	void SetCollisionEventCallback(FOnModuleCollisionEvent InCallback) { OnCollisionEventCallback = InCallback; }

	const FOnModuleCollisionEvent& GetCollisionEventCallback() const { return OnCollisionEventCallback; }

	void ProcessCollisionFeedback(
		const TMap<int32, UKawaiiFluidInteractionComponent*>& OwnerIDToIC,
		const TArray<FKawaiiFluidCollisionEvent>& CPUFeedbackBuffer);

	UPROPERTY()
	TObjectPtr<UKawaiiFluidPresetDataAsset> Preset;

private:
	FOnModuleCollisionEvent OnCollisionEventCallback;

	UPROPERTY()
	TArray<FKawaiiFluidParticle> Particles;

	TSharedPtr<FKawaiiFluidSpatialHash> SpatialHash;

	UPROPERTY()
	TArray<TObjectPtr<UKawaiiFluidCollider>> Colliders;

	FVector AccumulatedExternalForce = FVector::ZeroVector;

	float AccumulatedTime = 0.0f;

	FVector VolumeCenter = FVector::ZeroVector;

	FQuat VolumeRotationQuat = FQuat::Identity;

	bool bSimulationEnabled = true;

	bool bIndependentSimulation = true;

	bool bIsInitialized = false;

	TMap<int32, float> ParticleLastEventTime;

public:
	TMap<int32, float>& GetParticleLastEventTimeMap() { return ParticleLastEventTime; }

	virtual float GetParticleRadius() const override;

	virtual bool IsDataValid() const override
	{
		if (bGPUSimulationActive)
		{
			if (TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin())
			{
				return GPUSim->GetParticleCount() > 0 || GPUSim->GetPendingSpawnCount() > 0;
			}
		}
		return Particles.Num() > 0;
	}

	virtual FString GetDebugName() const override;

	virtual bool IsGPUSimulationActive() const override;

	virtual int32 GetGPUParticleCount() const override;

	virtual FGPUFluidSimulator* GetGPUSimulator() const override { return WeakGPUSimulator.Pin().Get(); }

	void SetGPUSimulator(const TSharedPtr<FGPUFluidSimulator>& InSimulator) { WeakGPUSimulator = InSimulator; }

	void SetGPUSimulationActive(bool bActive) { bGPUSimulationActive = bActive; }

	UFUNCTION(BlueprintCallable, Category = "Fluid|Module")
	void SyncGPUParticlesToCPU();

	void UploadCPUParticlesToGPU();

	UKawaiiFluidSimulationContext* GetSimulationContext() const { return CachedSimulationContext; }

	void SetSimulationContext(UKawaiiFluidSimulationContext* InContext) { CachedSimulationContext = InContext; }

	UFUNCTION(BlueprintPure, Category = "Fluid|Simulation Volume")
	UKawaiiFluidVolumeComponent* GetTargetVolumeComponent() const;

	UKawaiiFluidVolumeComponent* GetOwnedVolumeComponent() const { return OwnedVolumeComponent; }

	UFUNCTION(BlueprintPure, Category = "Fluid|Simulation Volume")
	bool IsUsingExternalVolume() const { return TargetSimulationVolume != nullptr; }

	void SetSourceID(int32 InSourceID);

	int32 GetSourceID() const { return CachedSourceID; }

private:
	TWeakPtr<FGPUFluidSimulator> WeakGPUSimulator;

	bool bGPUSimulationActive = false;

	UKawaiiFluidSimulationContext* CachedSimulationContext = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UKawaiiFluidVolumeComponent> OwnedVolumeComponent = nullptr;

	TWeakObjectPtr<UKawaiiFluidVolumeComponent> PreviousRegisteredVolume = nullptr;

	UFUNCTION()
	void OnTargetVolumeDestroyed(AActor* DestroyedActor);

	void BindToVolumeDestroyedEvent();
	void UnbindFromVolumeDestroyedEvent();

	bool bBoundToVolumeDestroyed = false;

#if WITH_EDITOR
	void OnPresetPropertyChanged(UKawaiiFluidPresetDataAsset* ChangedPreset);

	void BindToPresetPropertyChanged();
	void UnbindFromPresetPropertyChanged();

	FDelegateHandle PresetPropertyChangedHandle;

	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

	void BindToObjectsReplaced();
	void UnbindFromObjectsReplaced();

	FDelegateHandle ObjectsReplacedHandle;

	void OnPreBeginPIE(bool bIsSimulating);

	FDelegateHandle PreBeginPIEHandle;
#endif

	int32 CachedSourceID = -1;
};