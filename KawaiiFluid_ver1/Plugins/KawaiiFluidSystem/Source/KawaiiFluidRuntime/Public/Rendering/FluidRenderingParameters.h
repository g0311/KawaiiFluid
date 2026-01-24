// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/FluidAnisotropy.h"
#include "FluidSurfaceDecoration.h"
#include "Engine/TextureCube.h"
#include "FluidRenderingParameters.generated.h"

/**
 * Metaball Pipeline Type
 * Defines how the fluid surface is computed.
 */
UENUM(BlueprintType)
enum class EMetaballPipelineType : uint8
{
	/** Screen Space pipeline: Depth -> Smoothing -> Normal -> Thickness */
	ScreenSpace UMETA(DisplayName = "Screen Space"),

	/** Ray Marching pipeline: Volumetric rendering with 3D density volume */
	RayMarching UMETA(DisplayName = "Ray Marching")
};

// Removed: EMetaballShadingMode (all pipelines use PostProcess lighting only)
// Removed: EShadingPassTiming (no longer needed)
// Removed: ESSFRRenderingMode (deprecated)

/**
 * SSR Debug visualization mode.
 * Used to diagnose Screen Space Reflection issues at runtime.
 */
UENUM(BlueprintType)
enum class EScreenSpaceReflectionDebugMode : uint8
{
	/** No debug visualization (normal rendering) */
	None = 0 UMETA(DisplayName = "None"),

	/** Hit/Miss - Red = hit (intensity varies), Blue = miss */
	HitMiss = 1 UMETA(DisplayName = "Hit/Miss"),

	/** Reflection Direction - RGB = XYZ (0.5 = 0) */
	ReflectionDirection = 2 UMETA(DisplayName = "Reflection Direction"),

	/** Hit Color - Direct display of hit color (Magenta = miss) */
	HitColor = 3 UMETA(DisplayName = "Hit Color"),

	/** Reflection Z - Green = into scene (SSR possible), Red = toward camera (SSR not possible) */
	ReflectionZ = 4 UMETA(DisplayName = "Reflection Z"),

	/** Depth Compare - Red = ray in front, Green = ray behind, Blue = scene depth */
	DepthCompare = 5 UMETA(DisplayName = "Depth Compare"),

	/** Exit Reason - Red = behind camera, Green = off screen, Blue = max steps, Yellow = hit */
	ExitReason = 6 UMETA(DisplayName = "Exit Reason"),

	/** Normal - Normal direction visualization (RGB = XYZ) */
	Normal = 7 UMETA(DisplayName = "Normal"),

	/** ViewDir - View direction visualization (RGB = XYZ) */
	ViewDir = 8 UMETA(DisplayName = "ViewDir"),

	/** ViewPos.z - View position Z (Blue = in front, Red = behind) */
	ViewPosZ = 9 UMETA(DisplayName = "ViewPos.z")
};

/**
 * @brief Fluid rendering parameters.
 * Settings used throughout the SSFR pipeline.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFluidRenderingParameters
{
	GENERATED_BODY()

	/** Enable rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bEnableRendering = true;

	/** Pipeline type (how surface is computed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	EMetaballPipelineType PipelineType = EMetaballPipelineType::ScreenSpace;

	// Removed: ShadingMode (all pipelines now use PostProcess lighting only)

	/** Particle render radius (screen space, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Depth",
		meta = (ClampMin = "0.5", ClampMax = "100.0"))
	float ParticleRenderRadius = 15.0f;

	/**
	 * Depth smoothing filter radius in pixels.
	 * Controls how many neighboring pixels are sampled during smoothing.
	 * Larger values produce smoother surfaces but may blur fine details.
	 * Recommended: 10~30 for typical fluid, 30~50 for slime/gel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Smoothing",
		meta = (ClampMin = "1", ClampMax = "50"))
	int32 SmoothingRadius = 20;

	/**
	 * Depth difference threshold for including neighbor samples.
	 * Calculated as: threshold = ParticleRadius * ThresholdRatio.
	 * Samples with depth difference > threshold are excluded to preserve edges.
	 * Lower values (1~3): Sharp edges, visible particle boundaries.
	 * Higher values (5~10): Smoother surface, less edge definition.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Smoothing",
		meta = (ClampMin = "0.5", ClampMax = "20.0", DisplayName = "Threshold Ratio"))
	float NarrowRangeThresholdRatio = 3.0f;

	/**
	 * Clamp ratio for front-facing depth samples.
	 * Limits how much a pixel can be "pulled forward" toward the camera.
	 * Prevents holes while allowing surface to extend naturally.
	 * Lower values (0.5~1.0): Tighter clamping, fewer artifacts.
	 * Higher values (1.5~3.0): More aggressive smoothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Smoothing",
		meta = (ClampMin = "0.1", ClampMax = "5.0", DisplayName = "Clamp Ratio"))
	float NarrowRangeClampRatio = 1.0f;

	/**
	 * Boost smoothing at shallow viewing angles (grazing angles).
	 * Fluid surfaces viewed at steep angles often appear bumpy.
	 * This multiplies the threshold at grazing angles for more aggressive smoothing.
	 * 0.0: No boost (same smoothing everywhere).
	 * 1.0: 2x threshold at grazing angles.
	 * 2.0: 3x threshold at grazing angles.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Smoothing",
		meta = (ClampMin = "0.0", ClampMax = "2.0", DisplayName = "Grazing Angle Boost"))
	float NarrowRangeGrazingBoost = 1.0f;

	/** Fluid color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (HideAlphaChannel))
	FLinearColor FluidColor = FLinearColor(0.2f, 0.5f, 0.8f, 1.0f);

	/**
	 * F0 Override (base reflectivity at normal incidence).
	 * 0 = auto-calculate from IOR, >0 = use this value directly.
	 * 0.02 = water, 0.04 = glass, 0.1 = artistic stylized.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float F0Override = 0.0f;

	/**
	 * Fresnel strength multiplier (applied after F0 is auto-calculated from IOR).
	 * 1.0 = physically accurate reflection, 2.0 = exaggerated, 0.5 = weak.
	 * F0 = ((1-IOR)/(1+IOR))^2 * FresnelStrength.
	 * Ignored when F0Override > 0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float FresnelStrength = 1.0f;

	/** Index of Refraction (IOR). Ignored when F0Override > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float RefractiveIndex = 1.33f;

	/**
	 * Fluid opacity (0 = fully transparent, 1 = fully opaque).
	 * Controls overall light absorption strength through the fluid.
	 * 0 = see-through like clear water, 1 = opaque like thick paint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 0.5f;

	/**
	 * Per-channel absorption coefficients (Beer's Law).
	 * Water: R=0.4, G=0.1, B=0.05 (absorbs red, appears blue).
	 * Slime: R=0.1, G=0.3, B=0.4 (absorbs blue, appears green/yellow).
	 * Higher value = that color is absorbed faster (invisible in thick areas).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (HideAlphaChannel))
	FLinearColor AbsorptionColorCoefficients = FLinearColor(0.4f, 0.1f, 0.05f, 1.0f);

	/** Specular strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SpecularStrength = 1.0f;

	/** Specular roughness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SpecularRoughness = 0.2f;

	/**
	 * Thickness sensitivity (0 = uniform opacity, 1 = thickness-dependent).
	 * Controls how much fluid thickness affects transparency.
	 * 0 = same opacity everywhere, 1 = thin areas transparent, thick areas opaque.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ThicknessSensitivity = 0.5f;

	/**
	 * Refraction offset scale.
	 * UV offset strength from refraction.
	 * 0 = no refraction, higher = stronger distortion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float RefractionScale = 0.05f;

	/**
	 * Fresnel reflection blend ratio.
	 * How much Fresnel affects BaseColor/ReflectedColor blending.
	 * 0 = no reflection, 1 = strong reflection.
	 * 0.8+ recommended: at grazing angles, reflection naturally hides surface detail.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FresnelReflectionBlend = 0.8f;

	/**
	 * Absorption bias (for Ray Marching).
	 * Added to absorption contribution when blending BaseColor.
	 * Higher = FluidColor appears stronger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AbsorptionBias = 0.7f;

	//========================================
	// Reflection (SSR + Cubemap Fallback)
	//========================================

	/**
	 * Enable SSR (Screen Space Reflections).
	 * Actual scene objects are reflected on the fluid surface.
	 * Falls back to Cubemap on SSR miss.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection")
	bool bEnableScreenSpaceReflection = true;

	/**
	 * SSR ray march max steps.
	 * Higher = more accurate but more expensive.
	 * 8~16: low cost, 24~32: high quality.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (EditCondition = "bEnableScreenSpaceReflection", ClampMin = "4", ClampMax = "64"))
	int32 ScreenSpaceReflectionMaxSteps = 16;

	/**
	 * SSR step size (in pixels).
	 * Smaller = more precise but shorter reach.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (EditCondition = "bEnableScreenSpaceReflection", ClampMin = "0.5", ClampMax = "20.0"))
	float ScreenSpaceReflectionStepSize = 4.0f;

	/**
	 * SSR hit detection thickness.
	 * Higher = more lenient hit detection, lower = more precise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (EditCondition = "bEnableScreenSpaceReflection", ClampMin = "0.1", ClampMax = "5.0"))
	float ScreenSpaceReflectionThickness = 1.0f;

	/**
	 * SSR intensity (blended with Cubemap).
	 * 0 = Cubemap only, 1 = SSR only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (EditCondition = "bEnableScreenSpaceReflection", ClampMin = "0.0", ClampMax = "1.0"))
	float ScreenSpaceReflectionIntensity = 0.8f;

	/**
	 * SSR screen edge fade.
	 * Smoothly fades reflections going off screen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (EditCondition = "bEnableScreenSpaceReflection", ClampMin = "0.0", ClampMax = "0.5"))
	float ScreenSpaceReflectionEdgeFade = 0.1f;

	/**
	 * SSR debug visualization mode.
	 * Helps diagnose SSR issues at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (EditCondition = "bEnableScreenSpaceReflection"))
	EScreenSpaceReflectionDebugMode ScreenSpaceReflectionDebugMode = EScreenSpaceReflectionDebugMode::None;

	/**
	 * Fallback Cubemap (used on SSR miss).
	 * If not set, uses scene Sky Light color.
	 * Can assign scene Reflection Capture or HDRI Cubemap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection")
	TObjectPtr<UTextureCube> ReflectionCubemap = nullptr;

	/** Cubemap reflection intensity (0 = no reflection, 1 = full reflection) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ReflectionIntensity = 1.0f;

	/** Cubemap mip level (higher = blurrier reflection, linked to roughness) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Reflection",
		meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float ReflectionMipLevel = 2.0f;

	/** Thickness rendering scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Thickness",
		meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float ThicknessScale = 1.0f;

	/** Render target resolution scale (1.0 = screen resolution) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Performance",
		meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float RenderTargetScale = 1.0f;

	/** Anisotropy parameters for ellipsoid rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Anisotropy")
	FFluidAnisotropyParams AnisotropyParams;

	//========================================
	// Surface Decoration (Foam, Lava, etc.)
	//========================================

	/** Surface decoration parameters (foam, emissive, texture overlays) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration")
	FSurfaceDecorationParams SurfaceDecoration;

	/** Subsurface scattering intensity (jelly effect) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SubsurfaceScatteringIntensity = 1.0f;

	/** Subsurface scattering color (RGB only, alpha channel not used) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Appearance",
		meta = (HideAlphaChannel))
	FLinearColor SubsurfaceScatteringColor = FLinearColor(1.0f, 0.5f, 0.3f, 1.0f);

	// Removed: G-Buffer Mode Parameters (GBuffer/Opaque/Translucent modes removed)

	//========================================
	// Ray Marching Parameters
	//========================================

	/**
	 * Ray Marching volume resolution.
	 * Higher = more detail, more memory usage.
	 * 128: ~8MB, 256: ~64MB (with optimizations ~57MB total)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching",
			ClampMin = "64", ClampMax = "512"))
	int32 VolumeResolution = 256;

	/**
	 * Maximum ray march steps.
	 * Higher = more accurate but more expensive.
	 * 64~128 recommended with optimizations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching",
			ClampMin = "16", ClampMax = "512"))
	int32 RayMarchMaxSteps = 128;

	/**
	 * Density threshold for surface detection.
	 * When accumulated density exceeds this, consider surface found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching",
			ClampMin = "0.01", ClampMax = "2.0"))
	float DensityThreshold = 0.5f;

	/**
	 * Enable Occupancy Bitmask optimization.
	 * Uses 32³ bit mask (4KB) for O(1) empty block detection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching"))
	bool bEnableOccupancyMask = true;

	/**
	 * Enable Min-Max Mipmap optimization.
	 * Uses hierarchical volume for empty space skipping.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching"))
	bool bEnableMinMaxMipmap = true;

	/**
	 * Enable Tile-based Culling optimization.
	 * Skips tiles with no fluid intersection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching"))
	bool bEnableTileCulling = true;

	/**
	 * Enable Temporal Reprojection optimization.
	 * Reuses ~90% of previous frame data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching"))
	bool bEnableTemporalReprojection = true;

	/**
	 * Temporal blend factor (0 = current frame only, 1 = history only).
	 * 0.9 recommended for fluid motion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bEnableTemporalReprojection",
			ClampMin = "0.0", ClampMax = "0.99"))
	float TemporalBlendFactor = 0.9f;

	/**
	 * Adaptive step size multiplier.
	 * Step size increases in empty regions for faster traversal.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching",
			ClampMin = "1.0", ClampMax = "8.0"))
	float AdaptiveStepMultiplier = 4.0f;

	/**
	 * Early termination alpha threshold.
	 * Stop ray marching when accumulated alpha exceeds this value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching",
			ClampMin = "0.9", ClampMax = "1.0"))
	float EarlyTerminationAlpha = 0.99f;

	//========================================
	// SDF (Signed Distance Field) Parameters
	//========================================

	/**
	 * Use SDF (Signed Distance Field) mode instead of density volume.
	 * SDF uses Sphere Tracing for efficient empty space skipping.
	 * Better quality with translucency support.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching"))
	bool bUseSDF = true;

	/**
	 * SDF SmoothMin parameter (K).
	 * Controls the smoothness of fluid surface blending.
	 * Higher = smoother surface, lower = more defined particles.
	 * Recommended: 0.5x ~ 1.5x of ParticleRadius
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "0.5", ClampMax = "100.0"))
	float SDFSmoothK = 5.0f;

	/**
	 * SDF Surface offset.
	 * Negative = larger fluid volume, positive = smaller.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "-50.0", ClampMax = "50.0"))
	float SDFSurfaceOffset = 0.0f;

	/**
	 * SDF surface hit epsilon (in voxels).
	 * Smaller = more precise surface detection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "0.1", ClampMax = "5.0"))
	float SDFSurfaceEpsilon = 1.0f;

	/**
	 * SDF Sphere Tracing relaxation factor.
	 * 1.0 = standard Sphere Tracing, >1.0 = over-relaxation (faster but less stable).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "1.0", ClampMax = "2.0"))
	float SDFRelaxationFactor = 1.2f;

	/**
	 * SDF translucency maximum depth.
	 * How far light travels through the fluid for translucency calculation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "10.0", ClampMax = "500.0"))
	float SDFTranslucencyDepth = 100.0f;

	/**
	 * SDF translucency density.
	 * Controls internal light absorption for translucency.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "0.001", ClampMax = "0.1"))
	float SDFTranslucencyDensity = 0.02f;

	/**
	 * SDF subsurface scattering strength.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "0.0", ClampMax = "2.0"))
	float SDFSubsurfaceScatterStrength = 0.5f;

	/**
	 * SDF subsurface scattering color (RGB only, alpha channel not used).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF", HideAlphaChannel))
	FLinearColor SDFSubsurfaceColor = FLinearColor(0.8f, 0.6f, 0.4f, 1.0f);

	/**
	 * SDF reflection strength.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF",
			ClampMin = "0.0", ClampMax = "1.0"))
	float SDFReflectionStrength = 0.3f;

	/**
	 * Enable SDF Hybrid Mode.
	 * Uses SDF Volume for fast approach + Z-Order neighbor search for precise near-surface evaluation.
	 * Provides better quality normals with minimal performance overhead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF"))
	bool bEnableSDFHybridMode = true;

	/**
	 * Hybrid Mode threshold distance (in cm).
	 * Switch from SDF Volume to Z-Order when distance to surface is less than this value.
	 * Lower = less Z-Order usage (faster), Higher = more precise normals.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF && bEnableSDFHybridMode",
			ClampMin = "5.0", ClampMax = "100.0"))
	float SDFHybridThreshold = 30.0f;

	//========================================
	// SDF Optimization Options
	//========================================

	/**
	 * Use Tight AABB (computed from actual fluid particles) instead of simulation bounds.
	 * Dramatically reduces SDF volume build time by focusing only on fluid region.
	 * NOTE: Currently disabled - requires async GPU readback implementation.
	 * See: Docs/SDF_Optimization_TODO.md
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF"))
	bool bUseTightAABB = false;

	/**
	 * AABB padding multiplier (multiplied by particle radius).
	 * Higher values provide more safety margin for fast-moving fluids.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF && bUseTightAABB",
			ClampMin = "1.0", ClampMax = "5.0"))
	float AABBPaddingMultiplier = 2.0f;

	/**
	 * Debug visualization for Tight AABB.
	 * Shows: R = bUseTightAABB, G = bounds valid, B = bounds size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF && bUseTightAABB"))
	bool bDebugVisualizeTightAABB = false;

	/**
	 * Use Sparse Voxel structure (only compute SDF where particles exist).
	 * Additional optimization on top of Tight AABB.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF"))
	bool bUseSparseVoxel = false;

	/**
	 * Use Temporal Coherence (reuse previous frame's SDF, only update changed regions).
	 * Best for slow-moving or static fluids.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF"))
	bool bUseTemporalCoherence = false;

	/**
	 * Temporal dirty threshold (cm/frame).
	 * Particles moving faster than this threshold are marked dirty and recomputed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|RayMarching|SDF|Optimization",
		meta = (EditCondition = "PipelineType == EMetaballPipelineType::RayMarching && bUseSDF && bUseTemporalCoherence",
			ClampMin = "0.0", ClampMax = "50.0"))
	float TemporalDirtyThreshold = 5.0f;

	FFluidRenderingParameters() = default;
};

// Hash function for batching (TMap key)
FORCEINLINE uint32 GetTypeHash(const FFluidRenderingParameters& Params)
{
	uint32 Hash = GetTypeHash(Params.bEnableRendering);
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Params.PipelineType)));
	Hash = HashCombine(Hash, GetTypeHash(Params.FluidColor.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Params.F0Override));
	Hash = HashCombine(Hash, GetTypeHash(Params.FresnelStrength));
	Hash = HashCombine(Hash, GetTypeHash(Params.RefractiveIndex));
	Hash = HashCombine(Hash, GetTypeHash(Params.Opacity));
	Hash = HashCombine(Hash, GetTypeHash(Params.AbsorptionColorCoefficients.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Params.SpecularStrength));
	Hash = HashCombine(Hash, GetTypeHash(Params.SpecularRoughness));
	Hash = HashCombine(Hash, GetTypeHash(Params.ThicknessSensitivity));
	Hash = HashCombine(Hash, GetTypeHash(Params.RefractionScale));
	Hash = HashCombine(Hash, GetTypeHash(Params.FresnelReflectionBlend));
	Hash = HashCombine(Hash, GetTypeHash(Params.AbsorptionBias));
	// Reflection Cubemap parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.ReflectionCubemap.Get()));
	Hash = HashCombine(Hash, GetTypeHash(Params.ReflectionIntensity));
	Hash = HashCombine(Hash, GetTypeHash(Params.ReflectionMipLevel));
	Hash = HashCombine(Hash, GetTypeHash(Params.ParticleRenderRadius));
	Hash = HashCombine(Hash, GetTypeHash(Params.SmoothingRadius));
	// Narrow-Range parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.NarrowRangeThresholdRatio));
	Hash = HashCombine(Hash, GetTypeHash(Params.NarrowRangeClampRatio));
	Hash = HashCombine(Hash, GetTypeHash(Params.NarrowRangeGrazingBoost));
	// Anisotropy parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Params.AnisotropyParams.Mode)));
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.AnisotropyScale));
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.AnisotropyMin));
	Hash = HashCombine(Hash, GetTypeHash(Params.AnisotropyParams.AnisotropyMax));
	// Surface Decoration parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.SurfaceDecoration.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(Params.SurfaceDecoration.Foam.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(Params.SurfaceDecoration.Emissive.bEnabled));
	Hash = HashCombine(Hash, GetTypeHash(Params.RenderTargetScale));
	Hash = HashCombine(Hash, GetTypeHash(Params.ThicknessScale));
	// SSS parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.SubsurfaceScatteringIntensity));
	Hash = HashCombine(Hash, GetTypeHash(Params.SubsurfaceScatteringColor.ToString()));
	// SSR parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableScreenSpaceReflection));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionMaxSteps));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionStepSize));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionThickness));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionIntensity));
	Hash = HashCombine(Hash, GetTypeHash(Params.ScreenSpaceReflectionEdgeFade));
	// Ray Marching parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.VolumeResolution));
	Hash = HashCombine(Hash, GetTypeHash(Params.RayMarchMaxSteps));
	Hash = HashCombine(Hash, GetTypeHash(Params.DensityThreshold));
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableOccupancyMask));
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableMinMaxMipmap));
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableTileCulling));
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableTemporalReprojection));
	Hash = HashCombine(Hash, GetTypeHash(Params.TemporalBlendFactor));
	Hash = HashCombine(Hash, GetTypeHash(Params.AdaptiveStepMultiplier));
	Hash = HashCombine(Hash, GetTypeHash(Params.EarlyTerminationAlpha));
	// SDF Optimization parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.bUseTightAABB));
	Hash = HashCombine(Hash, GetTypeHash(Params.AABBPaddingMultiplier));
	Hash = HashCombine(Hash, GetTypeHash(Params.bDebugVisualizeTightAABB));
	Hash = HashCombine(Hash, GetTypeHash(Params.bUseSparseVoxel));
	Hash = HashCombine(Hash, GetTypeHash(Params.bUseTemporalCoherence));
	Hash = HashCombine(Hash, GetTypeHash(Params.TemporalDirtyThreshold));
	// SDF Hybrid Mode parameters
	Hash = HashCombine(Hash, GetTypeHash(Params.bEnableSDFHybridMode));
	Hash = HashCombine(Hash, GetTypeHash(Params.SDFHybridThreshold));
	return Hash;
}

// Equality operator for TMap key usage
FORCEINLINE bool operator==(const FFluidRenderingParameters& A, const FFluidRenderingParameters& B)
{
	return A.bEnableRendering == B.bEnableRendering &&
		A.PipelineType == B.PipelineType &&
		A.FluidColor.Equals(B.FluidColor, 0.001f) &&
		FMath::IsNearlyEqual(A.F0Override, B.F0Override, 0.001f) &&
		FMath::IsNearlyEqual(A.FresnelStrength, B.FresnelStrength, 0.001f) &&
		FMath::IsNearlyEqual(A.RefractiveIndex, B.RefractiveIndex, 0.001f) &&
		FMath::IsNearlyEqual(A.Opacity, B.Opacity, 0.001f) &&
		A.AbsorptionColorCoefficients.Equals(B.AbsorptionColorCoefficients, 0.001f) &&
		FMath::IsNearlyEqual(A.SpecularStrength, B.SpecularStrength, 0.001f) &&
		FMath::IsNearlyEqual(A.SpecularRoughness, B.SpecularRoughness, 0.001f) &&
		FMath::IsNearlyEqual(A.ThicknessSensitivity, B.ThicknessSensitivity, 0.001f) &&
		FMath::IsNearlyEqual(A.RefractionScale, B.RefractionScale, 0.001f) &&
		FMath::IsNearlyEqual(A.FresnelReflectionBlend, B.FresnelReflectionBlend, 0.001f) &&
		FMath::IsNearlyEqual(A.AbsorptionBias, B.AbsorptionBias, 0.001f) &&
		// Reflection Cubemap parameters
		A.ReflectionCubemap == B.ReflectionCubemap &&
		FMath::IsNearlyEqual(A.ReflectionIntensity, B.ReflectionIntensity, 0.001f) &&
		FMath::IsNearlyEqual(A.ReflectionMipLevel, B.ReflectionMipLevel, 0.001f) &&
		FMath::IsNearlyEqual(A.ParticleRenderRadius, B.ParticleRenderRadius, 0.001f) &&
		A.SmoothingRadius == B.SmoothingRadius &&
		// Narrow-Range parameters
		FMath::IsNearlyEqual(A.NarrowRangeThresholdRatio, B.NarrowRangeThresholdRatio, 0.01f) &&
		FMath::IsNearlyEqual(A.NarrowRangeClampRatio, B.NarrowRangeClampRatio, 0.01f) &&
		FMath::IsNearlyEqual(A.NarrowRangeGrazingBoost, B.NarrowRangeGrazingBoost, 0.01f) &&
		// Anisotropy parameters
		A.AnisotropyParams.bEnabled == B.AnisotropyParams.bEnabled &&
		A.AnisotropyParams.Mode == B.AnisotropyParams.Mode &&
		FMath::IsNearlyEqual(A.AnisotropyParams.AnisotropyScale, B.AnisotropyParams.AnisotropyScale,
		                     0.001f) &&
		FMath::IsNearlyEqual(A.AnisotropyParams.AnisotropyMin, B.AnisotropyParams.AnisotropyMin,
		                     0.001f) &&
		FMath::IsNearlyEqual(A.AnisotropyParams.AnisotropyMax, B.AnisotropyParams.AnisotropyMax,
		                     0.001f) &&
		// Surface Decoration parameters
		A.SurfaceDecoration.bEnabled == B.SurfaceDecoration.bEnabled &&
		A.SurfaceDecoration.Foam.bEnabled == B.SurfaceDecoration.Foam.bEnabled &&
		A.SurfaceDecoration.Emissive.bEnabled == B.SurfaceDecoration.Emissive.bEnabled &&
		FMath::IsNearlyEqual(A.RenderTargetScale, B.RenderTargetScale, 0.001f) &&
		FMath::IsNearlyEqual(A.ThicknessScale, B.ThicknessScale, 0.001f) &&
		// SSS parameters
		FMath::IsNearlyEqual(A.SubsurfaceScatteringIntensity, B.SubsurfaceScatteringIntensity, 0.001f) &&
		A.SubsurfaceScatteringColor.Equals(B.SubsurfaceScatteringColor, 0.001f) &&
		// SSR parameters
		A.bEnableScreenSpaceReflection == B.bEnableScreenSpaceReflection &&
		A.ScreenSpaceReflectionMaxSteps == B.ScreenSpaceReflectionMaxSteps &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionStepSize, B.ScreenSpaceReflectionStepSize, 0.01f) &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionThickness, B.ScreenSpaceReflectionThickness, 0.01f) &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionIntensity, B.ScreenSpaceReflectionIntensity, 0.01f) &&
		FMath::IsNearlyEqual(A.ScreenSpaceReflectionEdgeFade, B.ScreenSpaceReflectionEdgeFade, 0.01f) &&
		// Ray Marching parameters
		A.VolumeResolution == B.VolumeResolution &&
		A.RayMarchMaxSteps == B.RayMarchMaxSteps &&
		FMath::IsNearlyEqual(A.DensityThreshold, B.DensityThreshold, 0.01f) &&
		A.bEnableOccupancyMask == B.bEnableOccupancyMask &&
		A.bEnableMinMaxMipmap == B.bEnableMinMaxMipmap &&
		A.bEnableTileCulling == B.bEnableTileCulling &&
		A.bEnableTemporalReprojection == B.bEnableTemporalReprojection &&
		FMath::IsNearlyEqual(A.TemporalBlendFactor, B.TemporalBlendFactor, 0.01f) &&
		FMath::IsNearlyEqual(A.AdaptiveStepMultiplier, B.AdaptiveStepMultiplier, 0.01f) &&
		FMath::IsNearlyEqual(A.EarlyTerminationAlpha, B.EarlyTerminationAlpha, 0.001f) &&
		// SDF Optimization parameters
		A.bUseTightAABB == B.bUseTightAABB &&
		FMath::IsNearlyEqual(A.AABBPaddingMultiplier, B.AABBPaddingMultiplier, 0.01f) &&
		A.bDebugVisualizeTightAABB == B.bDebugVisualizeTightAABB &&
		A.bUseSparseVoxel == B.bUseSparseVoxel &&
		A.bUseTemporalCoherence == B.bUseTemporalCoherence &&
		FMath::IsNearlyEqual(A.TemporalDirtyThreshold, B.TemporalDirtyThreshold, 0.01f) &&
		// SDF Hybrid Mode parameters
		A.bEnableSDFHybridMode == B.bEnableSDFHybridMode &&
		FMath::IsNearlyEqual(A.SDFHybridThreshold, B.SDFHybridThreshold, 0.1f);
}
