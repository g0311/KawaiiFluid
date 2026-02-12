// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/SceneComponent.h"
#include "Core/KawaiiFluidParticle.h"
#include "Core/KawaiiFluidRenderParticle.h"
#include "Rendering/Parameters/KawaiiFluidRenderingParameters.h"
#include "Core/KawaiiFluidAnisotropy.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "KawaiiFluidRenderer.generated.h"

class IKawaiiFluidDataProvider;
class UKawaiiFluidRendererSubsystem;
class FKawaiiFluidRenderResource;
class IKawaiiFluidRenderingPipeline;
class FGPUFluidSimulator;
class UKawaiiFluidSimulationContext;

/**
 * @struct FKawaiiFluidMetaballRendererSettings
 * @brief Editor-facing configuration for the Metaball renderer.
 * 
 * @param bEnabled Toggle for activating the Metaball renderer.
 * @param bUseSimulationRadius Whether to use the physical simulation radius for rendering.
 * @param ParticleRenderRadius Override radius used if bUseSimulationRadius is false (cm).
 * @param bRenderSurfaceOnly Optimization to render only the outer layer of particles.
 * @param FluidColor The base diffuse/absorption color.
 * @param FresnelStrength Intensity of the glancing reflection.
 * @param RefractiveIndex Material index of refraction (IOR).
 * @param AbsorptionStrength Light absorption magnitude.
 * @param SpecularStrength Intensity of light source highlights.
 * @param SpecularRoughness Surface finish (lower = glossier).
 * @param SmoothingWorldScale Distance-adaptive blur magnitude.
 * @param SmoothingMinRadius Minimum blur pixel radius.
 * @param SmoothingMaxRadius Maximum blur pixel radius.
 * @param AnisotropyParams Ellipsoid stretching settings.
 * @param MaxRenderParticles Budget for the total number of particles rasterized.
 * @param ThicknessScale Depth-based darkness multiplier.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidMetaballRendererSettings
{
	GENERATED_BODY()

	//========================================
	// Enable Control
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ToolTip = "Enable/disable Metaball renderer"))
	bool bEnabled = false;

	//========================================
	// Rendering
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bEnabled", ToolTip = "Use simulation particle radius for rendering"))
	bool bUseSimulationRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bEnabled && !bUseSimulationRadius", ClampMin = "0.5", ClampMax = "100.0", ToolTip = "Particle render radius override (cm)"))
	float ParticleRenderRadius = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bEnabled", ToolTip = "Render only surface particles (slime mode)"))
	bool bRenderSurfaceOnly = false;

	//========================================
	// Visual Appearance
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", HideAlphaChannel, ToolTip = "Fluid base color"))
	FLinearColor FluidColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Fresnel term strength"))
	float FresnelStrength = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "1.0", ClampMax = "2.5", ToolTip = "Index of refraction (IOR)"))
	float RefractiveIndex = 1.33f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Light absorption magnitude (Beer's Law)"))
	float AbsorptionStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Specular highlight intensity"))
	float SpecularStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.01", ClampMax = "1.0", ToolTip = "Surface roughness (lower = shinier)"))
	float SpecularRoughness = 0.2f;

	//========================================
	// Smoothing (Narrow-Range Filter)
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (EditCondition = "bEnabled", ClampMin = "0.5", ClampMax = "5.0", ToolTip = "Distance-adaptive blur magnitude"))
	float SmoothingWorldScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "64", ToolTip = "Minimum blur pixel radius"))
	int32 SmoothingMinRadius = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (EditCondition = "bEnabled", ClampMin = "4", ClampMax = "64", ToolTip = "Maximum blur pixel radius"))
	int32 SmoothingMaxRadius = 32;

	//========================================
	// Anisotropy
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anisotropy", meta = (EditCondition = "bEnabled", ToolTip = "Ellipsoid stretching parameters"))
	FKawaiiFluidAnisotropyParams AnisotropyParams;

	//========================================
	// Performance
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "100000", ToolTip = "Budget for particles rasterized"))
	int32 MaxRenderParticles = 50000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (EditCondition = "bEnabled", ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Depth-based darkness multiplier"))
	float ThicknessScale = 1.0f;
};

/**
 * @class UKawaiiFluidRenderer
 * @brief Renderer that computes high-quality fluid surfaces using screen-space reconstruction.
 * 
 * Implements the SSFR pipeline including bilateral filtering, normal reconstruction, and volumetric 
 * shading via Beer's Law.
 * 
 * @param bEnabled Activation toggle.
 * @param bUseSimulationRadius Whether to inherit the physical simulation radius.
 * @param bRenderSurfaceOnly If true, culls internal particles before rendering.
 * @param LocalParameters Cached copy of rendering settings from the preset.
 * @param MaxRenderParticles Particle budget for the renderer.
 * @param LastRenderedParticleCount Statistics for the previous frame.
 * @param bIsRenderingActive Internal visibility/validity state.
 * @param CachedWorld Context pointer.
 * @param CachedOwnerComponent Parent component for reference.
 * @param CachedParticlePositions Buffered positions for interpolation or debug.
 * @param CachedParticleRadius Base radius used for pass parameters.
 * @param RendererSubsystem Weak reference to the global orchestrator.
 * @param CachedSimulationContext Context owning the shared RDG resources.
 * @param Pipeline Selected rendering strategy (ScreenSpace).
 * @param CachedPreset Reference to the source data asset.
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidRenderer : public UObject
{
	GENERATED_BODY()

public:
	UKawaiiFluidRenderer();

	void Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, UKawaiiFluidPresetDataAsset* InPreset);

	void Cleanup();

	void ApplySettings(const FKawaiiFluidMetaballRendererSettings& Settings);

	void UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime);

	bool IsEnabled() const { return bEnabled; }

	void SetEnabled(bool bInEnabled);

	FKawaiiFluidRenderResource* GetFluidRenderResource() const;

	void SetSimulationContext(UKawaiiFluidSimulationContext* InContext);

	bool IsRenderingActive() const;

	float GetCachedParticleRadius() const { return CachedParticleRadius; }

	const FKawaiiFluidRenderingParameters& GetLocalParameters() const
	{
		check(CachedPreset);
		return CachedPreset->RenderingParameters;
	}

	TSharedPtr<IKawaiiFluidRenderingPipeline> GetPipeline() const { return Pipeline; }
	
	FVector GetSpawnPositionHint() const { return CachedOwnerComponent ? CachedOwnerComponent->GetComponentLocation() : FVector::ZeroVector; }

	UKawaiiFluidPresetDataAsset* GetPreset() const { return CachedPreset; }

	void SetPreset(UKawaiiFluidPresetDataAsset* InPreset);

	void UpdatePipeline();

	//========================================
	// Enable Control
	//========================================

	bool bEnabled = true;

	bool bUseSimulationRadius = false;

	bool bRenderSurfaceOnly = false;

	//========================================
	// Performance Options
	//========================================

	int32 MaxRenderParticles = 50000;

	//========================================
	// Runtime Info
	//========================================

	int32 LastRenderedParticleCount = 0;

	bool bIsRenderingActive = false;

protected:
	//========================================
	// Common State
	//========================================

	UPROPERTY()
	TObjectPtr<UWorld> CachedWorld;

	UPROPERTY()
	TObjectPtr<USceneComponent> CachedOwnerComponent;

private:
	TArray<FVector> CachedParticlePositions;

	float CachedParticleRadius = 5.0f;

	TWeakObjectPtr<UKawaiiFluidRendererSubsystem> RendererSubsystem;

	//========================================
	// GPU Resources
	//========================================

	TWeakObjectPtr<UKawaiiFluidSimulationContext> CachedSimulationContext;

	//========================================
	// Pipeline Architecture
	//========================================

	TSharedPtr<IKawaiiFluidRenderingPipeline> Pipeline;

	//========================================
	// Preset Reference
	//========================================

	UPROPERTY()
	TObjectPtr<UKawaiiFluidPresetDataAsset> CachedPreset;
};
