// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"
#include "Preview/KawaiiFluidPreviewSettings.h"
#include "Core/IKawaiiFluidDataProvider.h"

class UKawaiiFluidPresetDataAsset;
class UKawaiiFluidSimulationContext;
class UKawaiiFluidSimulationModule;
class UKawaiiFluidRenderingModule;
struct FKawaiiFluidParticle;

/**
 * @brief FKawaiiFluidPreviewScene
 * 
 * A specialized preview world for fluid simulation.
 * Manages the GPU simulator, simulation module, and rendering module 
 * in a lightweight environment for asset editing.
 * 
 * @param CurrentPreset The fluid preset currently being previewed
 * @param PreviewSettingsObject Wrapper object for simulation settings in Details Panel
 * @param SimulationContext Physics solver with GPU simulator integration
 * @param SimulationModule Module handling particle spawn and update logic
 * @param RenderingModule Module handling visualization (ISM/Metaball)
 * @param PreviewActor Transient actor hosting simulation components
 * @param FloorMeshComponent Visual floor mesh for the preview
 */
class KAWAIIFLUIDEDITOR_API FKawaiiFluidPreviewScene : public FAdvancedPreviewScene,
                                                  public IKawaiiFluidDataProvider
{
public:
	FKawaiiFluidPreviewScene(FPreviewScene::ConstructionValues CVS);
	virtual ~FKawaiiFluidPreviewScene() override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	//========================================
	// IKawaiiFluidDataProvider Interface
	//========================================

	virtual const TArray<FKawaiiFluidParticle>& GetParticles() const override;
	virtual int32 GetParticleCount() const override;
	virtual float GetParticleRadius() const override;
	virtual bool IsDataValid() const override;
	virtual FString GetDebugName() const override { return TEXT("FluidPreviewScene"); }

	virtual bool IsGPUSimulationActive() const override;

	virtual int32 GetGPUParticleCount() const override;

	virtual FGPUFluidSimulator* GetGPUSimulator() const override;

	UKawaiiFluidRenderingModule* GetRenderingModule() const { return RenderingModule; }

	//========================================
	// Preset Management
	//========================================

	void SetPreset(UKawaiiFluidPresetDataAsset* InPreset);

	UKawaiiFluidPresetDataAsset* GetPreset() const { return CurrentPreset; }

	void RefreshFromPreset();

	//========================================
	// Simulation Control
	//========================================

	void StartSimulation();

	void StopSimulation();

	void ResetSimulation();

	void TickSimulation(float DeltaTime);

	bool IsSimulationActive() const { return bSimulationActive; }

	//========================================
	// Preview Settings
	//========================================

	FFluidPreviewSettings& GetPreviewSettings();

	UFluidPreviewSettingsObject* GetPreviewSettingsObject() const { return PreviewSettingsObject; }

	void ApplyPreviewSettings();

	//========================================
	// Environment
	//========================================

	void SetupFloor();

	void UpdateEnvironment();

	//========================================
	// Particle Access (GPU mode - limited)
	//========================================

	TArray<FKawaiiFluidParticle>& GetParticlesMutable();

	float GetSimulationTime() const;

private:
	void SpawnParticles(float DeltaTime);

	void CreateVisualizationComponents();

	void UpdateInstancedMesh();

	void HandleFloorCollision();

private:
	/** Current preset being previewed */
	TObjectPtr<UKawaiiFluidPresetDataAsset> CurrentPreset;

	/** Preview settings object (for Details Panel) */
	TObjectPtr<UFluidPreviewSettingsObject> PreviewSettingsObject;

	/** Simulation context - physics solver with GPU simulator */
	TObjectPtr<UKawaiiFluidSimulationContext> SimulationContext;

	/** Simulation module - uses same spawn logic as runtime */
	TObjectPtr<UKawaiiFluidSimulationModule> SimulationModule;

	/** Spawn accumulated time for continuous spawn */
	float SpawnAccumulatedTime;

	/** Total simulation time (accumulated) */
	float TotalSimulationTime;

	/** Is simulation running */
	bool bSimulationActive;

	/** Rendering module - same as runtime! (ISM + SSFR) */
	TObjectPtr<UKawaiiFluidRenderingModule> RenderingModule;

	//========================================
	// Visualization Components
	//========================================

	/** Preview actor to hold components */
	TObjectPtr<AActor> PreviewActor;

	/** Floor mesh */
	TObjectPtr<UStaticMeshComponent> FloorMeshComponent;

	/** Wall meshes */
	TArray<TObjectPtr<UStaticMeshComponent>> WallMeshComponents;

	/** Cached particle radius for visualization */
	float CachedParticleRadius;
};