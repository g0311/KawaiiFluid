// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/KawaiiFluidAnisotropy.h"
#include "KawaiiFluidSurfaceDecoration.h"
#include "Engine/TextureCube.h"
#include "KawaiiFluidRenderingParameters.generated.h"

/**
 * @enum EFluidReflectionMode
 * @brief Fluid Reflection Mode determining the technique used for surface reflections.
 */
UENUM(BlueprintType)
enum class EFluidReflectionMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No reflection (only base color and refraction)."),
	Cubemap UMETA(DisplayName = "Cubemap", ToolTip = "Cubemap-based reflection using a static environment map."),
	ScreenSpaceReflection UMETA(DisplayName = "Screen Space Reflection", ToolTip = "Real-time scene reflection (SSR)."),
	ScreenSpaceReflectionWithCubemap UMETA(DisplayName = "SSR + Cubemap", ToolTip = "SSR with Cubemap fallback for missed rays.")
};

/**
 * @struct FKawaiiFluidRenderingParameters
 * @brief Global fluid rendering parameters used throughout the SSFR pipeline.
 * 
 * @param FluidColor The base diffuse/absorption color of the fluid surface.
 * @param AbsorptionStrength Base light absorption strength (0=transparent, 1=maximum).
 * @param ThicknessScale Multiplier for computed thickness values to control depth darkness.
 * @param ThicknessSensitivity Controls how strongly thickness affects transmittance (0 to 1).
 * @param bEnableThicknessClamping Toggle for restricting thickness to a specific range.
 * @param ThicknessMin Minimum thickness floor. Values below this become fully transparent.
 * @param ThicknessMax Maximum thickness ceiling. Values above this are clamped.
 * @param FresnelStrength Multiplier for the physical Fresnel reflection effect at glancing angles.
 * @param SpecularStrength Intensity of specular highlights from light sources.
 * @param SpecularRoughness Sharpness of specular highlights (lower = sharp/glossy).
 * @param AmbientIntensity Scale for SkyLight contribution to prevent over-brightness.
 * @param LightingScale Global lighting multiplier for manual HDR compensation.
 * @param bEnableRefraction Toggle for background distortion through the fluid surface.
 * @param RefractiveIndex Index of Refraction (IOR) controlling light bending and Fresnel.
 * @param RefractionScale Magnitude of the UV offset for refraction.
 * @param bEnableCaustics Toggle for synthetic caustic light patterns on refracted background.
 * @param CausticIntensity Brightness multiplier for caustic patterns.
 * @param ParticleRenderRadius World-space radius of particles when rasterized (cm).
 * @param SmoothingWorldScale Blur radius multiplier relative to particle screen size.
 * @param SmoothingMinRadius Minimum blur radius floor in pixels.
 * @param SmoothingMaxRadius Maximum blur radius ceiling in pixels (LDS limited).
 * @param SmoothingIterations Number of narrow-range bilateral smoothing passes.
 * @param NarrowRangeThresholdRatio Distance threshold for particle blending.
 * @param NarrowRangeClampRatio Limit for depth displacement during smoothing.
 * @param NarrowRangeGrazingBoost Extra smoothing for steep viewing angles.
 * @param ReflectionMode Selected technique for rendering surface reflections.
 * @param FresnelReflectionBlend Multiplier for the Fresnel term when using Cubemap reflections.
 * @param ReflectionCubemap Environment map used for static reflections.
 * @param ReflectionIntensity Brightness multiplier for the reflection cubemap.
 * @param ReflectionMipLevel Mip level used for cubemap sampling (controls blur).
 * @param ScreenSpaceReflectionMaxSteps Max ray march iterations for SSR.
 * @param ScreenSpaceReflectionStepSize Pixels per step for SSR ray marching.
 * @param ScreenSpaceReflectionThickness Hit detection depth tolerance for SSR.
 * @param ScreenSpaceReflectionIntensity Magnitude multiplier for SSR results.
 * @param ScreenSpaceReflectionEdgeFade Fade-out factor near screen boundaries for SSR.
 * @param AnisotropyParams Parameters for anisotropic ellipsoid rendering.
 * @param SurfaceDecoration Settings for foam, emissive, and texture overlay effects.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidRenderingParameters
{
	GENERATED_BODY()

	//========================================
	// Color & Opacity
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Color", meta = (HideAlphaChannel, ToolTip = "Fluid color"))
	FLinearColor FluidColor = FLinearColor(0.2f, 0.5f, 0.8f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Color", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Base light absorption strength. Combined with Thickness to determine final opacity."))
	float AbsorptionStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Color", meta = (ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Multiplier for computed thickness values. Higher values make the fluid appear denser/darker."))
	float ThicknessScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Color", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How much thickness affects opacity. 0 = uniform opacity, 1 = thin areas are more transparent."))
	float ThicknessSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Color", meta = (ToolTip = "Enable min/max clamping for thickness values."))
	bool bEnableThicknessClamping = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Color", meta = (EditCondition = "bEnableThicknessClamping", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Minimum thickness value (scaled). Values below this become fully transparent."))
	float ThicknessMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Color", meta = (EditCondition = "bEnableThicknessClamping", ClampMin = "0.1", ClampMax = "50.0", ToolTip = "Maximum thickness value (scaled). Values above this are clamped."))
	float ThicknessMax = 10.0f;

	//========================================
	// Specular
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Specular", meta = (ClampMin = "0.0", ClampMax = "5.0", ToolTip = "Multiplier for edge reflection (Fresnel effect)."))
	float FresnelStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Specular", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Intensity of specular highlights from light sources."))
	float SpecularStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Specular", meta = (ClampMin = "0.01", ClampMax = "1.0", ToolTip = "Sharpness of specular highlights (lower = sharp/glossy)."))
	float SpecularRoughness = 0.2f;

	//========================================
	// Lighting
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Ambient light intensity from SkyLight. Scales SkyLightColor contribution."))
	float AmbientIntensity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Lighting", meta = (ClampMin = "0.01", ClampMax = "1.0", ToolTip = "Overall lighting scale for HDR compensation."))
	float LightingScale = 1.0f;

	//========================================
	// Refraction
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Refraction", meta = (ToolTip = "Enable background distortion through the fluid surface."))
	bool bEnableRefraction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Refraction", meta = (EditCondition = "bEnableRefraction", ClampMin = "1.0", ClampMax = "2.0", ToolTip = "Index of Refraction (IOR). Controls light bending magnitude."))
	float RefractiveIndex = 1.33f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Refraction", meta = (EditCondition = "bEnableRefraction", ClampMin = "0.0", ClampMax = "0.2", ToolTip = "Refraction offset scale."))
	float RefractionScale = 0.05f;

	//========================================
	// Caustics
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Caustics", meta = (EditCondition = "bEnableRefraction", ToolTip = "Enable caustic light patterns on refracted background."))
	bool bEnableCaustics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Caustics", meta = (EditCondition = "bEnableRefraction && bEnableCaustics", ClampMin = "0.0", ClampMax = "5.0", ToolTip = "Caustic brightness intensity multiplier."))
	float CausticIntensity = 1.5f;

	//========================================
	// Depth & Smoothing
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "0.5", ClampMax = "100.0", ToolTip = "World-space radius of each particle when rendered (in cm)."))
	float ParticleRenderRadius = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "0.5", ClampMax = "5.0", ToolTip = "Blur radius as a multiple of particle screen size."))
	float SmoothingWorldScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "1", ClampMax = "64", ToolTip = "Minimum blur radius in pixels (prevents over-sharpening at distance)."))
	int32 SmoothingMinRadius = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "4", ClampMax = "64", ToolTip = "Maximum blur radius in pixels (LDS limited)."))
	int32 SmoothingMaxRadius = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "1", ClampMax = "10", ToolTip = "Number of smoothing passes."))
	int32 SmoothingIterations = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "0.5", ClampMax = "20.0", DisplayName = "Threshold Ratio", ToolTip = "Controls distance-based particle blending."))
	float NarrowRangeThresholdRatio = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "0.1", ClampMax = "5.0", DisplayName = "Clamp Ratio", ToolTip = "Limits how much particles can move during smoothing."))
	float NarrowRangeClampRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth & Smoothing", meta = (ClampMin = "0.0", ClampMax = "2.0", DisplayName = "Grazing Angle Boost", ToolTip = "Extra smoothing when viewing the fluid at steep angles."))
	float NarrowRangeGrazingBoost = 1.0f;

	//========================================
	// Reflection
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (ToolTip = "Reflection technique used for the fluid surface."))
	EFluidReflectionMode ReflectionMode = EFluidReflectionMode::Cubemap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (EditCondition = "ReflectionMode != EFluidReflectionMode::None", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Fresnel-based blend ratio for Cubemap reflection."))
	float FresnelReflectionBlend = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (EditCondition = "ReflectionMode == EFluidReflectionMode::Cubemap || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ToolTip = "Static environment map for reflections."))
	TObjectPtr<UTextureCube> ReflectionCubemap = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (EditCondition = "ReflectionMode == EFluidReflectionMode::Cubemap || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Brightness multiplier for Cubemap reflection."))
	float ReflectionIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (EditCondition = "ReflectionMode == EFluidReflectionMode::Cubemap || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Cubemap mip level for sampling (blur control)."))
	float ReflectionMipLevel = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (DisplayName = "SSR Max Steps", EditCondition = "ReflectionMode == EFluidReflectionMode::ScreenSpaceReflection || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ClampMin = "64", ClampMax = "512", ToolTip = "Maximum ray marching steps for SSR."))
	int32 ScreenSpaceReflectionMaxSteps = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (DisplayName = "SSR Step Size", EditCondition = "ReflectionMode == EFluidReflectionMode::ScreenSpaceReflection || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ClampMin = "0.5", ClampMax = "20.0", ToolTip = "Step size in pixels for SSR ray marching."))
	float ScreenSpaceReflectionStepSize = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (DisplayName = "SSR Thickness", EditCondition = "ReflectionMode == EFluidReflectionMode::ScreenSpaceReflection || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ClampMin = "0.5", ClampMax = "5.0", ToolTip = "Depth tolerance for detecting ray hits in SSR."))
	float ScreenSpaceReflectionThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (DisplayName = "SSR Intensity", EditCondition = "ReflectionMode == EFluidReflectionMode::ScreenSpaceReflection || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "SSR magnitude multiplier."))
	float ScreenSpaceReflectionIntensity = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection", meta = (DisplayName = "SSR Edge Fade", EditCondition = "ReflectionMode == EFluidReflectionMode::ScreenSpaceReflection || ReflectionMode == EFluidReflectionMode::ScreenSpaceReflectionWithCubemap", ClampMin = "0.0", ClampMax = "0.5", ToolTip = "Fade-out factor near screen boundaries for SSR."))
	float ScreenSpaceReflectionEdgeFade = 0.1f;

	//========================================
	// Anisotropy
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy", meta = (ShowOnlyInnerProperties, ToolTip = "Anisotropy settings for ellipsoid rendering."))
	FKawaiiFluidAnisotropyParams AnisotropyParams;

	//========================================
	// Surface Decoration
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration", meta = (ShowOnlyInnerProperties, ToolTip = "Foam, emissive, and texture overlay parameters."))
	FSurfaceDecorationParams SurfaceDecoration;

	FKawaiiFluidRenderingParameters() = default;
};

/**
 * @brief Hash function for batching FFluidRenderingParameters in TMaps.
 */
FORCEINLINE uint32 GetTypeHash(const FKawaiiFluidRenderingParameters& Params)
{
	uint32 Hash = GetTypeHash(Params.FluidColor.ToString());
	Hash = HashCombine(Hash, GetTypeHash(Params.FresnelStrength));
	Hash = HashCombine(Hash, GetTypeHash(Params.RefractiveIndex));
	Hash = HashCombine(Hash, GetTypeHash(Params.AbsorptionStrength));
	Hash = HashCombine(Hash, GetTypeHash(Params.SpecularStrength));
	Hash = HashCombine(Hash, GetTypeHash(Params.SpecularRoughness));
	Hash = HashCombine(Hash, GetTypeHash(Params.AmbientIntensity));
	Hash = HashCombine(Hash, GetTypeHash(Params.LightingScale));
	Hash = HashCombine(Hash, GetTypeHash(Params.ThicknessSensitivity));
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableThicknessClamping));
	Hash = HashCombine(Hash, GetTypeHash(Params.ThicknessMin));
	Hash = HashCombine(Hash, GetTypeHash(Params.ThicknessMax));
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableRefraction));
	Hash = HashCombine(Hash, GetTypeHash(Params.RefractionScale));
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableCaustics));
	Hash = HashCombine(Hash, GetTypeHash(Params.CausticIntensity));
	Hash = HashCombine(Hash, GetTypeHash(Params.FresnelReflectionBlend));
	Hash = HashCombine(Hash, GetTypeHash(Params.ReflectionCubemap.Get()));
	Hash = HashCombine(Hash, GetTypeHash(Params.ReflectionIntensity));
	Hash = HashCombine(Hash, GetTypeHash(Params.ReflectionMipLevel));
	Hash = HashCombine(Hash, GetTypeHash(Params.ParticleRenderRadius));
	Hash = HashCombine(Hash, GetTypeHash(Params.SmoothingWorldScale));
	Hash = HashCombine(Hash, GetTypeHash(Params.SmoothingMinRadius));
	Hash = HashCombine(Hash, GetTypeHash(Params.SmoothingMaxRadius));
	Hash = HashCombine(Hash, GetTypeHash(Params.SmoothingIterations));
	Hash = HashCombine(Hash, GetTypeHash(Params.NarrowRangeThresholdRatio));
	Hash = HashCombine(Hash, GetTypeHash(Params.NarrowRangeClampRatio));
	Hash = HashCombine(Hash, GetTypeHash(Params.NarrowRangeGrazingBoost));
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Params.AnisotropyParams.Mode)));
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.Strength));
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.MinStretch));
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.MaxStretch));
	Hash = HashCombine(Hash, GetTypeHash(Params.SurfaceDecoration.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(Params.SurfaceDecoration.Foam.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(Params.SurfaceDecoration.Emissive.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(Params.ThicknessScale));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Params.ReflectionMode)));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionMaxSteps));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionStepSize));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionThickness));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionIntensity));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionEdgeFade));
	return Hash;
}

/**
 * @brief Equality operator for FFluidRenderingParameters.
 */
FORCEINLINE bool operator==(const FKawaiiFluidRenderingParameters& A, const FKawaiiFluidRenderingParameters& B)
{
	return A.FluidColor.Equals(B.FluidColor, 0.001f) &&
		FMath::IsNearlyEqual(A.FresnelStrength, B.FresnelStrength, 0.001f) &&
		FMath::IsNearlyEqual(A.RefractiveIndex, B.RefractiveIndex, 0.001f) &&
		FMath::IsNearlyEqual(A.AbsorptionStrength, B.AbsorptionStrength, 0.001f) &&
		FMath::IsNearlyEqual(A.SpecularStrength, B.SpecularStrength, 0.001f) &&
		FMath::IsNearlyEqual(A.SpecularRoughness, B.SpecularRoughness, 0.001f) &&
		FMath::IsNearlyEqual(A.AmbientIntensity, B.AmbientIntensity, 0.001f) &&
		FMath::IsNearlyEqual(A.LightingScale, B.LightingScale, 0.001f) &&
		FMath::IsNearlyEqual(A.ThicknessSensitivity, B.ThicknessSensitivity, 0.001f) &&
		A.bEnableThicknessClamping == B.bEnableThicknessClamping &&
		FMath::IsNearlyEqual(A.ThicknessMin, B.ThicknessMin, 0.001f) &&
		FMath::IsNearlyEqual(A.ThicknessMax, B.ThicknessMax, 0.001f) &&
		A.bEnableRefraction == B.bEnableRefraction &&
		FMath::IsNearlyEqual(A.RefractionScale, B.RefractionScale, 0.001f) &&
		A.bEnableCaustics == B.bEnableCaustics &&
		FMath::IsNearlyEqual(A.CausticIntensity, B.CausticIntensity, 0.001f) &&
		FMath::IsNearlyEqual(A.FresnelReflectionBlend, B.FresnelReflectionBlend, 0.001f) &&
		A.ReflectionCubemap == B.ReflectionCubemap &&
		FMath::IsNearlyEqual(A.ReflectionIntensity, B.ReflectionIntensity, 0.001f) &&
		FMath::IsNearlyEqual(A.ReflectionMipLevel, B.ReflectionMipLevel, 0.001f) &&
		FMath::IsNearlyEqual(A.ParticleRenderRadius, B.ParticleRenderRadius, 0.001f) &&
		FMath::IsNearlyEqual(A.SmoothingWorldScale, B.SmoothingWorldScale, 0.01f) &&
		A.SmoothingMinRadius == B.SmoothingMinRadius &&
		A.SmoothingMaxRadius == B.SmoothingMaxRadius &&
		A.SmoothingIterations == B.SmoothingIterations &&
		FMath::IsNearlyEqual(A.NarrowRangeThresholdRatio, B.NarrowRangeThresholdRatio, 0.01f) &&
		FMath::IsNearlyEqual(A.NarrowRangeClampRatio, B.NarrowRangeClampRatio, 0.01f) &&
		FMath::IsNearlyEqual(A.NarrowRangeGrazingBoost, B.NarrowRangeGrazingBoost, 0.01f) &&
		A.AnisotropyParams.bEnabled == B.AnisotropyParams.bEnabled &&
		A.AnisotropyParams.Mode == B.AnisotropyParams.Mode &&
		FMath::IsNearlyEqual(A.AnisotropyParams.Strength, B.AnisotropyParams.Strength, 0.001f) &&
		FMath::IsNearlyEqual(A.AnisotropyParams.MinStretch, B.AnisotropyParams.MinStretch, 0.001f) &&
		FMath::IsNearlyEqual(A.AnisotropyParams.MaxStretch, B.AnisotropyParams.MaxStretch, 0.001f) &&
		A.SurfaceDecoration.bEnabled == B.SurfaceDecoration.bEnabled &&
		A.SurfaceDecoration.Foam.bEnabled == B.SurfaceDecoration.Foam.bEnabled &&
		A.SurfaceDecoration.Emissive.bEnabled == B.SurfaceDecoration.Emissive.bEnabled &&
		FMath::IsNearlyEqual(A.ThicknessScale, B.ThicknessScale, 0.001f) &&
		A.ReflectionMode == B.ReflectionMode &&
		A.ScreenSpaceReflectionMaxSteps == B.ScreenSpaceReflectionMaxSteps &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionStepSize, B.ScreenSpaceReflectionStepSize, 0.01f) &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionThickness, B.ScreenSpaceReflectionThickness, 0.01f) &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionIntensity, B.ScreenSpaceReflectionIntensity, 0.01f) &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionEdgeFade, B.ScreenSpaceReflectionEdgeFade, 0.01f);
}
