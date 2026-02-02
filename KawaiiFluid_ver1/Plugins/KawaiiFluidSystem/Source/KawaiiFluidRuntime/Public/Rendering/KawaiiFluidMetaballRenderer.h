// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/SceneComponent.h"
#include "Core/FluidParticle.h"
#include "Core/KawaiiRenderParticle.h"
#include "Rendering/FluidRenderingParameters.h"
#include "Core/FluidAnisotropy.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "KawaiiFluidMetaballRenderer.generated.h"

class IKawaiiFluidDataProvider;
class UFluidRendererSubsystem;
class FKawaiiFluidRenderResource;
class IKawaiiMetaballRenderingPipeline;
class FGPUFluidSimulator;
class UKawaiiFluidSimulationContext;

/**
 * Metaball Renderer Settings (Editor Configuration)
 *
 * Lightweight struct for configuring Metaball renderer in Details panel.
 * These settings are copied to UKawaiiFluidMetaballRenderer at initialization.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidMetaballRendererSettings
{
	GENERATED_BODY()

	//========================================
	// Enable Control
	//========================================

	/** Enable/disable Metaball renderer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	bool bEnabled = false;

	//========================================
	// Rendering
	//========================================

	/** Use simulation particle radius for rendering (if true, ignores ParticleRenderRadius) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bEnabled"))
	bool bUseSimulationRadius = false;

	/** Particle render radius (screen space, cm) - only used when bUseSimulationRadius is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bEnabled && !bUseSimulationRadius", ClampMin = "0.5", ClampMax = "100.0"))
	float ParticleRenderRadius = 15.0f;

	/** Render only surface particles (for slime - reduces particle count while maintaining surface) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bEnabled"))
	bool bRenderSurfaceOnly = false;

	//========================================
	// Visual Appearance
	//========================================

	/** Fluid color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", HideAlphaChannel))
	FLinearColor FluidColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);

	/** Fresnel strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float FresnelStrength = 0.7f;

	/** Refractive index (IOR) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "1.0", ClampMax = "2.5"))
	float RefractiveIndex = 1.33f;

	/**
	 * Base light absorption strength.
	 * Combined with Thickness to determine final opacity.
	 * 0 = fully transparent, 1 = maximum absorption.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float AbsorptionStrength = 0.5f;

	/** Specular strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "2.0"))
	float SpecularStrength = 1.0f;

	/** Specular roughness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.01", ClampMax = "1.0"))
	float SpecularRoughness = 0.2f;

	//========================================
	// Smoothing (Narrow-Range Filter)
	//========================================

	/**
	 * Blur radius as a multiple of particle screen size.
	 * Automatically scales based on distance from camera.
	 * 2.0 = blur covers 2x the particle's screen area.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (EditCondition = "bEnabled", ClampMin = "0.5", ClampMax = "5.0"))
	float SmoothingWorldScale = 2.0f;

	/**
	 * Minimum blur radius in pixels (prevents over-sharpening at distance).
	 * Limited by GPU LDS cache size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "64"))
	int32 SmoothingMinRadius = 4;

	/**
	 * Maximum blur radius in pixels (performance limit for close objects).
	 * Limited by GPU LDS cache size (max 64 at full resolution).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (EditCondition = "bEnabled", ClampMin = "4", ClampMax = "64"))
	int32 SmoothingMaxRadius = 32;

	//========================================
	// Anisotropy
	//========================================

	/** Anisotropy parameters for ellipsoid rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anisotropy", meta = (EditCondition = "bEnabled"))
	FFluidAnisotropyParams AnisotropyParams;

	//========================================
	// Performance
	//========================================

	/** Maximum particles to render */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "100000"))
	int32 MaxRenderParticles = 50000;

	/** Thickness scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (EditCondition = "bEnabled", ClampMin = "0.1", ClampMax = "10.0"))
	float ThicknessScale = 1.0f;

};

// Backwards compatibility alias
using FKawaiiFluidSSFRRendererSettings = FKawaiiFluidMetaballRendererSettings;

/**
 * Metaball Renderer (UObject-based)
 *
 * Renders fluid particles using GPU-based metaball rendering with
 * screen-space surface reconstruction for realistic fluid appearance.
 *
 * Features:
 * - Realistic fluid surface rendering
 * - GPU Compute Shader based high performance
 * - Reflection/refraction/fresnel effects
 * - Custom rendering pipeline via ViewExtension
 *
 * Note: This is NOT an ActorComponent - it's owned internally by RenderingModule.
 * Pure UObject implementation (no component dependencies).
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidMetaballRenderer : public UObject
{
	GENERATED_BODY()

public:
	UKawaiiFluidMetaballRenderer();

	/**
	 * Initialize renderer with world, owner and preset
	 * @param InWorld World context for subsystem access
	 * @param InOwnerComponent Parent scene component (for consistency with ISM)
	 * @param InPreset Preset data asset for rendering parameters
	 */
	void Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, UKawaiiFluidPresetDataAsset* InPreset);

	/**
	 * Cleanup renderer resources
	 */
	void Cleanup();


	///////////////////////////////////////////////////////////////////////
	///////////////DEPRECATED(Using Preset Data)//////////////
	///////////////////////////////////////////////////////////////////////
	/**
	 * Apply settings from struct
	 * @param Settings Editor configuration to apply
	 */
	void ApplySettings(const FKawaiiFluidMetaballRendererSettings& Settings);

	/**
	 * Update rendering
	 * @param DataProvider Particle data provider
	 * @param DeltaTime Frame delta time
	 */
	void UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime);

	/** Check if rendering is enabled */
	bool IsEnabled() const { return bEnabled; }

	/** Enable or disable rendering */
	void SetEnabled(bool bInEnabled);

	//========================================
	// GPU Resource Access (for ViewExtension)
	//========================================

	/** Get GPU render resource (for ViewExtension access) */
	FKawaiiFluidRenderResource* GetFluidRenderResource() const;

	/**
	 * Set simulation context (for accessing shared RenderResource)
	 * Context owns the RenderResource for batched rendering
	 */
	void SetSimulationContext(UKawaiiFluidSimulationContext* InContext);

	/** Check if rendering is active and resource is valid */
	bool IsRenderingActive() const;

	/** Get cached particle radius (for ViewExtension access) */
	float GetCachedParticleRadius() const { return CachedParticleRadius; }

	/** Get rendering parameters from Preset */
	const FFluidRenderingParameters& GetLocalParameters() const
	{
		check(CachedPreset);
		return CachedPreset->RenderingParameters;
	}

	/** Get rendering pipeline (handles ShadingMode internally) */
	TSharedPtr<IKawaiiMetaballRenderingPipeline> GetPipeline() const { return Pipeline; }
	
	/** Get spawn position hint for initial bounds (from owner component location) */
	FVector GetSpawnPositionHint() const { return CachedOwnerComponent ? CachedOwnerComponent->GetComponentLocation() : FVector::ZeroVector; }

	//========================================
	// Preset Access (for batching by Preset)
	//========================================

	/** Get cached preset (for render thread preset-based batching) */
	UKawaiiFluidPresetDataAsset* GetPreset() const { return CachedPreset; }

	/** Set preset reference and sync Pipeline from Preset->RenderingParameters (called from game thread) */
	void SetPreset(UKawaiiFluidPresetDataAsset* InPreset);

	/** Update Pipeline based on Preset->RenderingParameters (Pipeline handles ShadingMode internally) */
	void UpdatePipeline();

	//========================================
	// Enable Control
	//========================================

	/** Enable/disable this renderer */
	bool bEnabled = true;

	/** Use simulation particle radius for rendering */
	bool bUseSimulationRadius = false;

	/** Render only surface particles (for slime) */
	bool bRenderSurfaceOnly = false;


	///////////////////////////////////////////////////////////////////////
	///////////////DEPRECATED(Using Preset Data)//////////////
	///////////////////////////////////////////////////////////////////////
	/** Local rendering parameters (per-renderer settings) */
	FFluidRenderingParameters LocalParameters;

	//========================================
	// Performance Options
	//========================================

	/** Maximum particles to render */
	int32 MaxRenderParticles = 50000;

	//========================================
	// Runtime Info
	//========================================

	/** Last frame rendered particle count */
	int32 LastRenderedParticleCount = 0;

	/** Metaball rendering active status */
	bool bIsRenderingActive = false;

protected:
	//========================================
	// Common State (from removed base class)
	//========================================

	/** Cached world reference (replaces GetWorld()) */
	UPROPERTY()
	TObjectPtr<UWorld> CachedWorld;

	/** Cached owner component reference */
	UPROPERTY()
	TObjectPtr<USceneComponent> CachedOwnerComponent;

private:
	/** Cached particle positions */
	TArray<FVector> CachedParticlePositions;

	/** Cached particle radius */
	float CachedParticleRadius = 5.0f;

	/** Cached renderer subsystem reference (for ViewExtension access) */
	TWeakObjectPtr<UFluidRendererSubsystem> RendererSubsystem;

	//========================================
	// GPU Resources
	//========================================

	/** Cached simulation context (owns the shared RenderResource) */
	TWeakObjectPtr<UKawaiiFluidSimulationContext> CachedSimulationContext;

	//========================================
	// Pipeline Architecture
	//========================================

	/** Rendering pipeline */
	TSharedPtr<IKawaiiMetaballRenderingPipeline> Pipeline;

	//========================================
	// Preset Reference
	//========================================

	/** Cached preset pointer (for preset-based batching and rendering params) */
	UPROPERTY()
	TObjectPtr<UKawaiiFluidPresetDataAsset> CachedPreset;
};

// Backwards compatibility alias
using UKawaiiFluidSSFRRenderer = UKawaiiFluidMetaballRenderer;
