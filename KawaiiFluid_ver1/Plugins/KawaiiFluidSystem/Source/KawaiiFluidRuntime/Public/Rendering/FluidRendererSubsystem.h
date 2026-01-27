// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "FluidRenderingParameters.h"
#include "Core/KawaiiFluidRenderingTypes.h"
#include "FluidRendererSubsystem.generated.h"

class FFluidSceneViewExtension;
class UKawaiiFluidRenderingModule;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/** Number of shadow quality levels (Low, Medium, High) */
static constexpr int32 NUM_SHADOW_QUALITY_LEVELS = 3;

/**
 * Fluid rendering world subsystem
 *
 * Responsibilities:
 * - Manages UKawaiiFluidRenderingModule integration
 * - Provides SSFR rendering pipeline (ViewExtension)
 * - Aggregates shadow particles from multiple volumes and updates ISM per quality level
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UFluidRendererSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End of USubsystem interface

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bEnableISMShadow && !IsTemplate(); }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	// End of FTickableGameObject interface

	//========================================
	// RenderingModule Management
	//========================================

	/** Register RenderingModule (called automatically) */
	void RegisterRenderingModule(UKawaiiFluidRenderingModule* Module);

	/** Unregister RenderingModule */
	void UnregisterRenderingModule(UKawaiiFluidRenderingModule* Module);

	/** Get all registered RenderingModules */
	const TArray<TObjectPtr<UKawaiiFluidRenderingModule>>& GetAllRenderingModules() const { return RegisteredRenderingModules; }

	//========================================
	// Rendering Parameters
	//========================================

	/** Global rendering parameters */
	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Fluid Rendering")
	FFluidRenderingParameters RenderingParameters;

	/** View Extension accessor */
	TSharedPtr<FFluidSceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() const { return ViewExtension; }

private:
	/** Registered RenderingModules */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKawaiiFluidRenderingModule>> RegisteredRenderingModules;

	/** Scene View Extension (rendering pipeline injection) */
	TSharedPtr<FFluidSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;

	//========================================
	// ISM Shadow (Instanced Mesh Shadow)
	// Multi-volume aggregation: particles are collected per-quality and rendered in Tick
	//========================================

public:
	/**
	 * @brief Enable/disable ISM shadow via instanced spheres.
	 * When enabled, creates sphere instances at particle positions for shadow casting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow|ISM")
	bool bEnableISMShadow = true;

	/**
	 * @brief Skip factor for particle-to-instance conversion.
	 * Value of 2 means every other particle becomes an instance.
	 * Higher values improve performance but reduce shadow detail.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow|ISM", meta = (ClampMin = "1", ClampMax = "10"))
	int32 ParticleSkipFactor = 1;

	/**
	 * @brief Register shadow particles for aggregation.
	 * Call this from Volume/Component Tick. Particles are aggregated per quality level
	 * and rendered in Subsystem Tick.
	 * @param ParticlePositions Array of particle world positions.
	 * @param NumParticles Number of particles.
	 * @param ParticleRadius Radius of each particle (used for shadow sphere size).
	 * @param Quality Shadow mesh quality level (Low/Medium/High).
	 */
	void RegisterShadowParticles(const FVector* ParticlePositions, int32 NumParticles, float ParticleRadius, EFluidShadowMeshQuality Quality);

private:
	/** Actor that owns the ISM shadow components. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> ShadowProxyActor = nullptr;

	/** ISM components for each quality level (Low/Medium/High). */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ShadowInstanceComponents[NUM_SHADOW_QUALITY_LEVELS];

	/** Shadow sphere meshes for each quality level (Low/Medium/High). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ShadowSphereMeshes[NUM_SHADOW_QUALITY_LEVELS];

	/** Get or create shadow sphere mesh for specified quality level. */
	UStaticMesh* GetOrCreateShadowMesh(EFluidShadowMeshQuality Quality);

	/** Get or create ISM component for specified quality level. */
	UInstancedStaticMeshComponent* GetOrCreateShadowISM(EFluidShadowMeshQuality Quality);

	//========================================
	// Per-Quality Particle Aggregation Buffers
	//========================================

	/** Aggregated particle positions per quality level. */
	TArray<FVector> AggregatedPositions[NUM_SHADOW_QUALITY_LEVELS];

	/** Aggregated particle radii per quality level (uses max radius). */
	float AggregatedRadius[NUM_SHADOW_QUALITY_LEVELS] = { 0.0f, 0.0f, 0.0f };

	/** Whether any particles were registered this frame per quality. */
	bool bHasParticlesThisFrame[NUM_SHADOW_QUALITY_LEVELS] = { false, false, false };

	/** Cached instance transforms for batch update. */
	TArray<FTransform> CachedInstanceTransforms;

	/** Flush aggregated particles to ISM components. Called in Tick. */
	void FlushShadowInstances();

	/** Clear aggregation buffers for next frame. */
	void ClearAggregationBuffers();

	/** Cleanup all shadow resources. */
	void CleanupShadowResources();
};
