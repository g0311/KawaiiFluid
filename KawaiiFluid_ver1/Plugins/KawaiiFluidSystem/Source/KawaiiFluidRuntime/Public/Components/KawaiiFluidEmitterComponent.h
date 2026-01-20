// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/KawaiiFluidComponent.h"
#include "KawaiiFluidEmitterComponent.generated.h"

class AKawaiiFluidEmitter;
class AKawaiiFluidVolume;
class UKawaiiFluidSimulationModule;

/**
 * Kawaii Fluid Emitter Component
 *
 * Component that handles particle spawning logic for AKawaiiFluidEmitter.
 * Uses FFluidSpawnSettings for compatibility with existing spawn logic.
 *
 * All configuration properties are stored in this component.
 * The AKawaiiFluidEmitter actor provides the world presence.
 *
 * Responsibilities:
 * - Manage target volume reference and registration
 * - Execute spawn logic based on SpawnSettings
 * - Spawn particles in various shapes (sphere, box, cylinder)
 * - Handle continuous/burst spawning (Stream, HexagonalStream, Spray)
 * - Track spawn state and accumulated time
 */
UCLASS(ClassGroup = (KawaiiFluid), meta = (BlueprintSpawnableComponent, DisplayName = "Kawaii Fluid Emitter"))
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidEmitterComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKawaiiFluidEmitterComponent();

	//========================================
	// UActorComponent Interface
	//========================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//========================================
	// Target Volume Configuration
	//========================================

	/** The target volume to emit particles into */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
	TObjectPtr<AKawaiiFluidVolume> TargetVolume;

	/** Whether to automatically find the nearest volume if TargetVolume is not set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
	bool bAutoFindVolume = true;

	//========================================
	// Wireframe Visualization Settings
	//========================================

	/** Show spawn volume/emitter wireframe in editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Wireframe")
	bool bShowSpawnVolumeWireframe = true;

	/** Wireframe color for spawn volume visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Wireframe",
		meta = (EditCondition = "bShowSpawnVolumeWireframe"))
	FColor SpawnVolumeWireframeColor = FColor::Cyan;

	/** Wireframe line thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Wireframe",
		meta = (EditCondition = "bShowSpawnVolumeWireframe", ClampMin = "0.5", ClampMax = "10.0"))
	float WireframeThickness = 2.0f;

	//========================================
	// Spawn Settings (using FFluidSpawnSettings for compatibility)
	//========================================

	/** All spawn configuration settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
	FFluidSpawnSettings SpawnSettings;

	//========================================
	// Runtime Settings
	//========================================

	/** Whether to auto-spawn on BeginPlay (ShapeVolume mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	bool bAutoSpawnOnBeginPlay = true;

	/** Recycle oldest particles when MaxParticleCount is reached */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime",
		meta = (EditCondition = "SpawnSettings.SpawnType == EFluidSpawnType::Emitter && SpawnSettings.MaxParticleCount > 0"))
	bool bRecycleOldestParticles = false;

	//========================================
	// Target Volume API
	//========================================

	/** Get the target volume */
	UFUNCTION(BlueprintPure, Category = "Target")
	AKawaiiFluidVolume* GetTargetVolume() const { return TargetVolume; }

	/** Set the target volume */
	UFUNCTION(BlueprintCallable, Category = "Target")
	void SetTargetVolume(AKawaiiFluidVolume* NewVolume);

	//========================================
	// API
	//========================================

	/** Get the owner emitter actor */
	UFUNCTION(BlueprintPure, Category = "Emitter")
	AKawaiiFluidEmitter* GetOwnerEmitter() const;

	/** Get particle spacing from target volume's preset */
	UFUNCTION(BlueprintPure, Category = "Emitter")
	float GetParticleSpacing() const;

	/** Execute auto spawn (ShapeVolume mode) */
	UFUNCTION(BlueprintCallable, Category = "Emitter")
	void ExecuteAutoSpawn();

	/** Spawn a burst of particles */
	UFUNCTION(BlueprintCallable, Category = "Emitter")
	void BurstSpawn(int32 Count);

	/** Get total particles spawned by this emitter */
	UFUNCTION(BlueprintPure, Category = "Emitter")
	int32 GetSpawnedParticleCount() const { return SpawnedParticleCount; }

	/** Check if the emitter has reached its particle limit */
	UFUNCTION(BlueprintPure, Category = "Emitter")
	bool HasReachedParticleLimit() const;

	/** Check if ShapeVolume mode */
	UFUNCTION(BlueprintPure, Category = "Emitter")
	bool IsShapeVolumeMode() const { return SpawnSettings.SpawnType == EFluidSpawnType::ShapeVolume; }

	/** Check if Emitter mode */
	UFUNCTION(BlueprintPure, Category = "Emitter")
	bool IsEmitterMode() const { return SpawnSettings.SpawnType == EFluidSpawnType::Emitter; }

protected:
	//========================================
	// Spawn State
	//========================================

	/** Accumulated time for rate-based spawning */
	float SpawnAccumulator = 0.0f;

	/** Accumulated distance for HexagonalStream layer spawning */
	float LayerDistanceAccumulator = 0.0f;

	/** Total particles spawned by this emitter */
	int32 SpawnedParticleCount = 0;

	/** Whether auto spawn has been executed */
	bool bAutoSpawnExecuted = false;

	//========================================
	// Internal Spawn Methods
	//========================================

	/** Process continuous spawning for Emitter mode */
	void ProcessContinuousSpawn(float DeltaTime);

	/** Process Stream emitter mode */
	void ProcessStreamEmitter(float DeltaTime);

	/** Process HexagonalStream emitter mode */
	void ProcessHexagonalStreamEmitter(float DeltaTime);

	/** Process Spray emitter mode */
	void ProcessSprayEmitter(float DeltaTime);

	//========================================
	// Shape Spawning (ShapeVolume mode)
	//========================================

	/** Spawn particles in a sphere shape (hexagonal pattern) */
	int32 SpawnParticlesSphereHexagonal(FVector Center, float Radius, float Spacing, FVector InitialVelocity);

	/** Spawn particles in a box shape (hexagonal pattern) */
	int32 SpawnParticlesBoxHexagonal(FVector Center, FVector Extent, float Spacing, FVector InitialVelocity);

	/** Spawn particles in a cylinder shape (hexagonal pattern) */
	int32 SpawnParticlesCylinderHexagonal(FVector Center, float Radius, float HalfHeight, float Spacing, FVector InitialVelocity);

	/** Spawn particles in a sphere shape (random) */
	void SpawnParticlesSphereRandom(FVector Center, float Radius, int32 Count, FVector InitialVelocity);

	/** Spawn particles in a box shape (random) */
	void SpawnParticlesBoxRandom(FVector Center, FVector HalfExtent, int32 Count, FVector InitialVelocity);

	/** Spawn particles in a cylinder shape (random) */
	void SpawnParticlesCylinderRandom(FVector Center, float Radius, float HalfHeight, int32 Count, FVector InitialVelocity);

	//========================================
	// Directional Spawning (Emitter mode)
	//========================================

	/** Spawn particles with directional velocity (stream/spray) */
	void SpawnParticleDirectional(FVector Position, FVector Direction, float Speed, int32 Count, float ConeAngle = 0.0f);

	/** Spawn a hexagonal layer of particles */
	void SpawnHexagonalLayer(FVector Position, FVector Direction, float Speed, float Radius, float Spacing);

	//========================================
	// Internal Helpers
	//========================================

	/** Queue spawn request to target volume's simulation module */
	void QueueSpawnRequest(const TArray<FVector>& Positions, const TArray<FVector>& Velocities);

	/** Get the simulation module from target volume */
	UKawaiiFluidSimulationModule* GetSimulationModule() const;

	/** Remove oldest particles if recycling is enabled */
	void RecycleOldestParticlesIfNeeded(int32 NewParticleCount);

	/** Apply jitter to position */
	FVector ApplyJitter(FVector Position, float Spacing) const;

	//========================================
	// Volume Registration (Internal)
	//========================================

	/** Register this emitter to its target volume */
	void RegisterToVolume();

	/** Unregister this emitter from its target volume */
	void UnregisterFromVolume();

	/** Find the nearest volume automatically */
	AKawaiiFluidVolume* FindNearestVolume() const;

#if WITH_EDITOR
	//========================================
	// Debug Visualization
	//========================================

	/** Draw spawn volume/emitter wireframe visualization */
	void DrawSpawnVolumeVisualization();
#endif
};
