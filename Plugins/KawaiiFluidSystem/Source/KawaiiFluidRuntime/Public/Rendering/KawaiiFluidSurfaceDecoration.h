// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "KawaiiFluidSurfaceDecoration.generated.h"

/**
 * @enum ETextureAddressingMode
 * @brief Texture Addressing Mode for UV coordinates.
 */
UENUM(BlueprintType)
enum class ETextureAddressingMode : uint8
{
	Wrap UMETA(DisplayName = "Wrap (Repeat)", ToolTip = "Repeat the texture (default tiling)."),
	Mirror UMETA(DisplayName = "Mirror", ToolTip = "Mirror the texture at boundaries (like decals).")
};

/**
 * @struct FSurfaceDecorationLayer
 * @brief Texture Overlay Settings for adding custom layers on top of the fluid surface.
 * 
 * @param bEnabled Enable texture overlay on fluid surface.
 * @param Texture Overlay texture (color/pattern).
 * @param TilingScale Texture tiling density. Higher = smaller pattern.
 * @param AddressingMode UV wrap mode at texture boundaries.
 * @param NormalMap Normal map for surface detail (optional).
 * @param NormalStrength Normal map intensity. Higher = stronger bumps.
 * @param Opacity Overlay opacity (0 to 1).
 * @param bMultiply If true, uses multiply blend mode instead of additive.
 * @param NormalZThreshold Surface angle filter (-1 = all, 1 = upward only).
 * @param bUseFlowAnimation Move texture with fluid flow.
 * @param FlowInfluence Intensity of flow-based movement (0 to 1).
 * @param ScrollSpeed Constant scroll speed in UV units per second.
 * @param bJitterEnabled Enable organic UV jittering animation.
 * @param JitterStrength Magnitude of UV jitter displacement.
 * @param JitterSpeed Speed of the jitter animation.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FSurfaceDecorationLayer
{
	GENERATED_BODY()

	//========================================
	// Enable
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay", meta = (ToolTip = "Enable texture overlay on fluid surface"))
	bool bEnabled = false;

	//========================================
	// Texture
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled", ToolTip = "Overlay texture (color/pattern)"))
	TObjectPtr<UTexture2D> Texture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled", ClampMin = "0.0001", ClampMax = "1.0", ToolTip = "Texture tiling density. Higher = smaller pattern."))
	float TilingScale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled && Texture != nullptr", ToolTip = "UV wrap mode at texture boundaries"))
	ETextureAddressingMode AddressingMode = ETextureAddressingMode::Wrap;

	//========================================
	// Normal Map
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Normal", meta = (EditCondition = "bEnabled", ToolTip = "Normal map for surface detail (optional)"))
	TObjectPtr<UTexture2D> NormalMap = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Normal", meta = (EditCondition = "bEnabled && NormalMap != nullptr", ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Normal map intensity. Higher = stronger bumps."))
	float NormalStrength = 1.0f;

	//========================================
	// Blending
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Blending", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Overlay opacity. 0 = invisible, 1 = fully visible."))
	float Opacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Blending", meta = (EditCondition = "bEnabled", ToolTip = "Multiply blend mode. Off = additive, On = multiply with fluid color."))
	bool bMultiply = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Blending", meta = (EditCondition = "bEnabled", ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "Surface angle filter. -1 = all surfaces, 0 = horizontal only, 1 = upward only."))
	float NormalZThreshold = -1.0f;

	//========================================
	// Texture Animation
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled", ToolTip = "Move texture with fluid flow. Requires Flow Animation to be enabled globally."))
	bool bUseFlowAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled && bUseFlowAnimation", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How much flow affects texture movement. 0 = static, 1 = full flow speed."))
	float FlowInfluence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled", ToolTip = "Constant scroll speed (UV units per second). Applied on top of flow."))
	FVector2D ScrollSpeed = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled", ToolTip = "Animate texture with organic UV jittering"))
	bool bJitterEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled && bJitterEnabled", ClampMin = "0.0", ClampMax = "0.5", ToolTip = "Jitter displacement amount. Higher = more movement."))
	float JitterStrength = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Texture", meta = (EditCondition = "bEnabled && bJitterEnabled", ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Jitter animation speed. Higher = faster movement."))
	float JitterSpeed = 2.0f;
};

/**
 * @struct FFoamSettings
 * @brief Foam generation and appearance settings for water surfaces.
 * 
 * @param bEnabled Enable foam effect on the fluid surface.
 * @param FoamColor Base foam color.
 * @param Intensity Overall brightness multiplier (HDR supported).
 * @param FoamTexture Pattern texture for foam.
 * @param TilingScale Texture tiling density.
 * @param AddressingMode UV wrap mode for the foam texture.
 * @param bUseFlowAnimation Move foam texture with fluid velocity.
 * @param bJitterEnabled Apply organic UV jittering.
 * @param JitterStrength Magnitude of jitter.
 * @param JitterSpeed Speed of jitter animation.
 * @param VelocityThreshold Minimum speed required to generate foam.
 * @param bWaveCrestFoam Generate foam at wave peaks and breaking points.
 * @param WaveCrestFoamStrength Intensity of peak-based foam.
 * @param bThicknessFoam Generate foam in thin fluid regions.
 * @param ThicknessThreshold Thickness limit for foam generation.
 * @param ThicknessFoamStrength Intensity of thin-area foam.
 * @param bVelocitySmoothing Blur velocity texture to soften foam boundaries.
 * @param VelocitySmoothingRadius Pixel radius for velocity blur.
 * @param VelocitySmoothingIterations Number of blur passes for velocity.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFoamSettings
{
	GENERATED_BODY()

	//========================================
	// Enable
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam", meta = (ToolTip = "Enable foam effect on the fluid surface"))
	bool bEnabled = false;

	//========================================
	// Appearance
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Appearance", meta = (EditCondition = "bEnabled", ToolTip = "Foam color. Multiplied with texture if assigned."))
	FLinearColor FoamColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "5.0", ToolTip = "Overall brightness. Higher = brighter foam. Supports HDR (values > 1)."))
	float Intensity = 1.0f;

	//========================================
	// Texture
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Texture", meta = (EditCondition = "bEnabled", ToolTip = "Foam pattern texture. If not set, foam renders as solid color."))
	TObjectPtr<UTexture2D> FoamTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Texture", meta = (EditCondition = "bEnabled", ClampMin = "0.0001", ClampMax = "1.0", ToolTip = "Texture tiling density. Higher = smaller pattern."))
	float TilingScale = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Texture", meta = (EditCondition = "bEnabled && FoamTexture != nullptr", ToolTip = "UV wrap mode at texture boundaries"))
	ETextureAddressingMode AddressingMode = ETextureAddressingMode::Wrap;

	//========================================
	// Texture Animation
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Texture", meta = (EditCondition = "bEnabled", ToolTip = "Move foam texture with fluid flow."))
	bool bUseFlowAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Texture", meta = (EditCondition = "bEnabled", ToolTip = "Animate texture with organic UV jittering"))
	bool bJitterEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Texture", meta = (EditCondition = "bEnabled && bJitterEnabled", ClampMin = "0.0", ClampMax = "0.5", ToolTip = "Jitter displacement amount. Higher = more movement."))
	float JitterStrength = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Texture", meta = (EditCondition = "bEnabled && bJitterEnabled", ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Jitter animation speed. Higher = faster movement."))
	float JitterSpeed = 2.0f;

	//========================================
	// Generation: Velocity
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Generation", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1000.0", ToolTip = "Minimum velocity (cm/s) to generate foam."))
	float VelocityThreshold = 100.0f;

	//========================================
	// Generation: Wave Crest
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Generation", meta = (EditCondition = "bEnabled", ToolTip = "Generate foam at wave peaks and breaking points"))
	bool bWaveCrestFoam = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Generation", meta = (EditCondition = "bEnabled && bWaveCrestFoam", ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Wave crest foam intensity. Higher = more foam at wave peaks."))
	float WaveCrestFoamStrength = 1.0f;

	//========================================
	// Generation: Thin Areas
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Generation", meta = (EditCondition = "bEnabled", ToolTip = "Generate foam in thin fluid regions (spray, droplets, sheet edges)"))
	bool bThicknessFoam = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Generation", meta = (EditCondition = "bEnabled && bThicknessFoam", ClampMin = "0.01", ClampMax = "5.0", ToolTip = "Thickness threshold. Foam appears where fluid is thinner than this value."))
	float ThicknessThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Generation", meta = (EditCondition = "bEnabled && bThicknessFoam", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Thin-area foam intensity. Higher = more foam in thin regions."))
	float ThicknessFoamStrength = 0.3f;

	//========================================
	// Edge Softening
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Edge Softening", meta = (EditCondition = "bEnabled", ToolTip = "Blur velocity texture to soften foam boundaries. Removes sharp particle edges."))
	bool bVelocitySmoothing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Edge Softening", meta = (EditCondition = "bEnabled && bVelocitySmoothing", ClampMin = "1.0", ClampMax = "30.0", ToolTip = "Blur radius in pixels. Higher = softer edges."))
	float VelocitySmoothingRadius = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam|Edge Softening", meta = (EditCondition = "bEnabled && bVelocitySmoothing", ClampMin = "1", ClampMax = "5", ToolTip = "Blur iterations. Higher = smoother but slower."))
	int32 VelocitySmoothingIterations = 1;
};

/**
 * @struct FEmissiveSettings
 * @brief Glow and emission settings for dynamic fluid surfaces.
 * 
 * @param bEnabled Enable glow/emission effect.
 * @param EmissiveColor Base glow color (HDR supported).
 * @param MinEmissive Minimum constant glow brightness.
 * @param Intensity Dynamic multiplier for velocity and pulse effects.
 * @param bVelocityEmissive Faster flow increases glow brightness.
 * @param VelocitySensitivity Threshold speed for maximum glow.
 * @param PulsePeriod Cycle time for brightness pulsing (0 = none).
 * @param PulseAmplitude Intensity variation for pulsing.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FEmissiveSettings
{
	GENERATED_BODY()

	//========================================
	// Enable
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive", meta = (ToolTip = "Enable glow/emission effect"))
	bool bEnabled = false;

	//========================================
	// Appearance
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive|Appearance", meta = (EditCondition = "bEnabled", HDR, ToolTip = "Glow color (HDR supported)"))
	FLinearColor EmissiveColor = FLinearColor(1.0f, 0.3f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive|Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Minimum glow brightness. Always visible even when stationary."))
	float MinEmissive = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive|Appearance", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Dynamic glow multiplier. Scales velocity and pulse effects."))
	float Intensity = 10.0f;

	//========================================
	// Velocity Response
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive|Velocity", meta = (EditCondition = "bEnabled", ToolTip = "Faster flow = brighter glow. Good for lava."))
	bool bVelocityEmissive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive|Velocity", meta = (EditCondition = "bEnabled && bVelocityEmissive", ClampMin = "0.1", ClampMax = "5.0", ToolTip = "Velocity sensitivity. Higher = glows brighter at lower speeds."))
	float VelocitySensitivity = 1.0f;

	//========================================
	// Pulse Animation
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive|Pulse", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Pulse cycle time in seconds. 0 = no pulse."))
	float PulsePeriod = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive|Pulse", meta = (EditCondition = "bEnabled && PulsePeriod > 0", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Pulse brightness variation. 0 = steady, 1 = full range."))
	float PulseAmplitude = 0.2f;
};

/**
 * @struct FFlowMapSettings
 * @brief Settings for velocity-driven UV animation.
 * 
 * @param bEnabled Enable flow-based texture animation.
 * @param FlowSpeed Overall speed multiplier for texture movement.
 * @param DistortionStrength Magnitude of UV warping.
 * @param VelocityScale Ratio of particle velocity to UV displacement.
 * @param FlowDecay Rate of return to rest when flow stops.
 * @param MaxFlowOffset Cap for accumulated UV offset to prevent overflow.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFlowMapSettings
{
	GENERATED_BODY()

	//========================================
	// Enable
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Flow Animation", meta = (ToolTip = "Enable flow-based texture animation. Textures move with fluid velocity."))
	bool bEnabled = false;

	//========================================
	// Animation Speed
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Flow Animation", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Overall flow animation speed. Higher = faster texture movement."))
	float FlowSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Flow Animation", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "UV distortion amount. Higher = more warping."))
	float DistortionStrength = 0.1f;

	//========================================
	// Velocity Accumulation
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Flow Animation",
		meta = (EditCondition = "bEnabled", ClampMin = "0.01", ClampMax = "100.0", ToolTip = "Velocity to UV scale. Higher = faster movement per velocity unit."))
	float VelocityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Flow Animation",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "5.0", ToolTip = "Flow decay rate when velocity stops. 0 = no decay."))
	float FlowDecay = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Flow Animation",
		meta = (EditCondition = "bEnabled", ClampMin = "10.0", ClampMax = "10000.0", AdvancedDisplay, ToolTip = "Maximum flow offset (cm) before UV wrapping."))
	float MaxFlowOffset = 1000.0f;
};

/**
 * @struct FSurfaceDecorationParams
 * @brief Master parameters for fluid surface visual effects (foam, glow, texture overlays).
 * 
 * @param bEnabled Global toggle for surface decoration effects.
 * @param Foam Settings for foam and bubble effects.
 * @param Layer Settings for custom texture overlays.
 * @param LayerFinalOpacity Master opacity for the overlay layer.
 * @param LayerBlendWithFluidColor Blending factor with the base fluid color.
 * @param bApplyLightingToLayer Whether to apply scene lighting to the overlay texture.
 * @param LayerSpecularStrength Specular intensity for the overlay.
 * @param LayerSpecularRoughness Specular roughness for the overlay.
 * @param FlowMap Settings for flow-based UV animation.
 * @param Emissive Settings for glow and emission effects.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FSurfaceDecorationParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration", meta = (ToolTip = "Enable surface decoration effects"))
	bool bEnabled = false;

	//========================================
	// Foam
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Foam", meta = (EditCondition = "bEnabled", ShowOnlyInnerProperties, ToolTip = "Foam/bubble effect for water surfaces"))
	FFoamSettings Foam;

	//========================================
	// Overlay
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay", meta = (EditCondition = "bEnabled", ShowOnlyInnerProperties, ToolTip = "Custom texture overlay (caustics, dirt, patterns)"))
	FSurfaceDecorationLayer Layer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Blending", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Master opacity for overlay."))
	float LayerFinalOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Blending", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Blend with fluid color. 0 = overlay only, 1 = tinted by fluid color."))
	float LayerBlendWithFluidColor = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Lighting", meta = (EditCondition = "bEnabled", ToolTip = "Apply scene lighting to overlay texture"))
	bool bApplyLightingToLayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Lighting", meta = (EditCondition = "bEnabled && bApplyLightingToLayer", ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Overlay specular intensity."))
	float LayerSpecularStrength = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Overlay|Lighting", meta = (EditCondition = "bEnabled && bApplyLightingToLayer", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Overlay specular roughness."))
	float LayerSpecularRoughness = 0.5f;

	//========================================
	// Flow Animation
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Flow Animation", meta = (EditCondition = "bEnabled", ShowOnlyInnerProperties, ToolTip = "Flow animation settings."))
	FFlowMapSettings FlowMap;

	//========================================
	// Emissive
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering|Surface Decoration|Emissive", meta = (EditCondition = "bEnabled", ShowOnlyInnerProperties, ToolTip = "Glow/emission effect for lava, magic, or radioactive fluids"))
	FEmissiveSettings Emissive;

	FSurfaceDecorationParams() = default;
};
