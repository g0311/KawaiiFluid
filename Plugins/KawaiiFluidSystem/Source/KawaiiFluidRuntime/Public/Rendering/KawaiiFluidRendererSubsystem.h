// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "KawaiiFluidRenderingParameters.h"
#include "Core/KawaiiFluidRenderingTypes.h"
#include "KawaiiFluidRendererSubsystem.generated.h"

class FKawaiiFluidSceneViewExtension;
class UKawaiiFluidRenderingModule;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/** Number of shadow quality levels (Low, Medium, High) */
static constexpr int32 NUM_SHADOW_QUALITY_LEVELS = 3;

/**
 * @class UKawaiiFluidRendererSubsystem
 * @brief World subsystem responsible for coordinating fluid rendering and managing global shadow aggregation.
 * 
 * Integrates the SSFR rendering pipeline via SceneViewExtension and aggregates shadow particles 
 * from multiple volumes into shared instanced static meshes.
 * 
 * @param RenderingParameters Global rendering settings for the SSFR pipeline.
 * @param RegisteredRenderingModules List of active rendering modules being managed.
 * @param ViewExtension Scene view extension for pipeline injection.
 * @param bEnableISMShadow Toggle for particle-based shadow casting via ISM.
 * @param ParticleSkipFactor Optimization factor for shadow instance conversion.
 * @param ShadowProxyActor Internal actor holding the shadow ISM components.
 * @param ShadowInstanceComponents Array of ISM components per shadow quality level.
 * @param ShadowSphereMeshes Cached low-poly meshes for shadow spheres.
 * @param AggregatedPositions Buffered particle positions for the current frame's shadow pass.
 * @param AggregatedRadius Current frame's max particle radius per quality level.
 * @param bHasParticlesThisFrame Tracking flag for shadow buffer status.
 * @param CachedInstanceTransforms Reusable transform buffer for batch ISM updates.
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidRendererSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	//========================================
	// USubsystem interface
	//========================================

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//========================================
	// FTickableGameObject interface
	//========================================

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bEnableISMShadow && !IsTemplate(); }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	//========================================
	// RenderingModule Management
	//========================================

	void RegisterRenderingModule(UKawaiiFluidRenderingModule* Module);

	void UnregisterRenderingModule(UKawaiiFluidRenderingModule* Module);

	const TArray<TObjectPtr<UKawaiiFluidRenderingModule>>& GetAllRenderingModules() const { return RegisteredRenderingModules; }

	//========================================
	// Rendering Parameters
	//========================================

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Fluid Rendering", meta = (ToolTip = "Global rendering settings for the SSFR pipeline."))
	FKawaiiFluidRenderingParameters RenderingParameters;

	TSharedPtr<FKawaiiFluidSceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() const { return ViewExtension; }

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKawaiiFluidRenderingModule>> RegisteredRenderingModules;

	TSharedPtr<FKawaiiFluidSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;

	//========================================
	// ISM Shadow (Instanced Mesh Shadow)
	//========================================

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow|ISM", meta = (ToolTip = "Enable/disable ISM shadow via instanced spheres. When enabled, creates sphere instances at particle positions for shadow casting."))
	bool bEnableISMShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow|ISM", meta = (ClampMin = "1", ClampMax = "10", ToolTip = "Skip factor for particle-to-instance conversion. Higher values improve performance but reduce shadow detail."))
	int32 ParticleSkipFactor = 1;

	void RegisterShadowParticles(const FVector* ParticlePositions, int32 NumParticles, float ParticleRadius, EFluidShadowMeshQuality Quality);

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> ShadowProxyActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ShadowInstanceComponents[NUM_SHADOW_QUALITY_LEVELS];

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ShadowSphereMeshes[NUM_SHADOW_QUALITY_LEVELS];

	UStaticMesh* GetOrCreateShadowMesh(EFluidShadowMeshQuality Quality);

	UInstancedStaticMeshComponent* GetOrCreateShadowISM(EFluidShadowMeshQuality Quality);

	//========================================
	// Particle Aggregation Buffers
	//========================================

	TArray<FVector> AggregatedPositions[NUM_SHADOW_QUALITY_LEVELS];

	float AggregatedRadius[NUM_SHADOW_QUALITY_LEVELS] = { 0.0f, 0.0f, 0.0f };

	bool bHasParticlesThisFrame[NUM_SHADOW_QUALITY_LEVELS] = { false, false, false };

	TArray<FTransform> CachedInstanceTransforms;

	void FlushShadowInstances();

	void ClearAggregationBuffers();

	void CleanupShadowResources();
};
