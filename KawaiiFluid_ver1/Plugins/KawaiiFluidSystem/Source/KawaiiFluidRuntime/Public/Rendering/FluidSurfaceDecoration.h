// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "FluidSurfaceDecoration.generated.h"

/**
 * Surface Decoration Layer
 * Defines a single texture layer to overlay on the fluid surface.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FSurfaceDecorationLayer
{
	GENERATED_BODY()

	/** Enable this layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	bool bEnabled = false;

	/** Layer texture (albedo/pattern) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	TObjectPtr<UTexture2D> Texture = nullptr;

	/** Normal map for this layer (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	TObjectPtr<UTexture2D> NormalMap = nullptr;

	/** Texture tiling scale (0.01 = 1 tile per 100 units/1m, 0.1 = 1 tile per 10 units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "0.0001", ClampMax = "1.0"))
	float TilingScale = 0.01f;

	/** Layer opacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 1.0f;

	/** Blend mode with base color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	bool bMultiply = false;

	/** Apply only to surfaces above this normal Z threshold (-1 = all surfaces, 0.5 = mostly upward) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float NormalZThreshold = -1.0f;

	/** Flow map influence (0 = static, 1 = full flow animation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlowInfluence = 0.5f;

	/** Scroll speed for animated textures */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	FVector2D ScrollSpeed = FVector2D::ZeroVector;
};

/**
 * Foam Settings
 * Controls foam generation and appearance.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFoamSettings
{
	GENERATED_BODY()

	/** Enable foam effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam")
	bool bEnabled = false;

	/** Foam texture (noise-based pattern) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UTexture2D> FoamTexture = nullptr;

	/** Foam color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled"))
	FLinearColor FoamColor = FLinearColor::White;

	/** Velocity threshold to start generating foam */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1000.0"))
	float VelocityThreshold = 100.0f;

	/** Foam intensity multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "5.0"))
	float Intensity = 1.0f;

	/** Foam decay rate (how fast foam disappears) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "10.0"))
	float DecayRate = 1.0f;

	/** Foam appears at wave crests (depth gradient) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled"))
	bool bWaveCrestFoam = true;

	/** Foam appears at collision points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled"))
	bool bCollisionFoam = true;

	/** Texture tiling scale (0.01 = 1 tile per 100 units/1m, 0.1 = 1 tile per 10 units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foam", meta = (EditCondition = "bEnabled", ClampMin = "0.0001", ClampMax = "1.0"))
	float TilingScale = 0.02f;
};

/**
 * Emissive Settings (for lava, magical fluids, etc.)
 * Controls glowing/emissive effects.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FEmissiveSettings
{
	GENERATED_BODY()

	/** Enable emissive effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive")
	bool bEnabled = false;

	/** Emissive color (HDR) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled", HDR))
	FLinearColor EmissiveColor = FLinearColor(1.0f, 0.3f, 0.05f, 1.0f);

	/** Emissive intensity multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "100.0"))
	float Intensity = 10.0f;

	/** Crack/pattern texture that reveals emissive underneath */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UTexture2D> CrackTexture = nullptr;

	/** Crack texture tiling scale (0.01 = 1 tile per 100 units/1m, 0.1 = 1 tile per 10 units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled", ClampMin = "0.0001", ClampMax = "1.0"))
	float CrackTilingScale = 0.02f;

	/**
	 * Temperature mode: emissive based on velocity/turbulence
	 * High velocity = hot = more emissive
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled"))
	bool bTemperatureMode = true;

	/** Velocity for maximum temperature (emissive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled && bTemperatureMode", ClampMin = "1.0", ClampMax = "1000.0"))
	float MaxTemperatureVelocity = 200.0f;

	/** Minimum emissive (even when stationary) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float MinEmissive = 0.2f;

	/** Pulsation frequency (0 = no pulse) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "10.0"))
	float PulseFrequency = 0.0f;

	/** Pulsation amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float PulseAmplitude = 0.2f;
};

/**
 * Flow Map Settings
 * Controls flow-based UV distortion for animated surface effects.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFlowMapSettings
{
	GENERATED_BODY()

	/** Enable flow-based animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	bool bEnabled = false;

	/** Use particle velocity to generate flow map (runtime) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow", meta = (EditCondition = "bEnabled"))
	bool bUseParticleVelocity = true;

	/** Static flow map texture (if not using particle velocity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow", meta = (EditCondition = "bEnabled && !bUseParticleVelocity"))
	TObjectPtr<UTexture2D> FlowMapTexture = nullptr;

	/** Flow speed multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "10.0"))
	float FlowSpeed = 1.0f;

	/** Flow distortion strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float DistortionStrength = 0.1f;

	//========================================
	// Flow Accumulation (Velocity-based)
	// When bUseParticleVelocity is true, these control how velocity is accumulated into UV offset
	// Still water: no accumulation (texture stays static)
	// Flowing water: accumulates offset (texture moves)
	//========================================

	/**
	 * Scale factor for velocity contribution to flow
	 * Higher = faster texture movement for same velocity
	 * Note: Particle velocity is in cm/sec, so 1.0 means 1 cm offset per 1 cm/sec per second
	 * Increase this if flow effect is too subtle
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow",
		meta = (EditCondition = "bEnabled && bUseParticleVelocity", ClampMin = "0.01", ClampMax = "100.0"))
	float VelocityScale = 1.0f;

	/**
	 * How quickly flow decays when velocity is zero
	 * 0 = no decay (offset accumulates indefinitely)
	 * Higher = texture slowly returns to original position when water stops
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow",
		meta = (EditCondition = "bEnabled && bUseParticleVelocity", ClampMin = "0.0", ClampMax = "5.0"))
	float FlowDecay = 0.1f;

	/**
	 * Maximum accumulated flow offset in world units (cm) before wrapping
	 * Used to prevent numerical overflow in long simulations
	 * Should be large enough to cover multiple texture tiles (e.g., 1000cm = 10m)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow",
		meta = (EditCondition = "bEnabled && bUseParticleVelocity", ClampMin = "10.0", ClampMax = "10000.0"))
	float MaxFlowOffset = 1000.0f;
};

/**
 * Surface Decoration Parameters
 * Main parameter struct for fluid surface decoration effects.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FSurfaceDecorationParams
{
	GENERATED_BODY()

	/** Enable surface decoration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration")
	bool bEnabled = false;

	//========================================
	// Foam (Water)
	//========================================

	/** Foam settings (primarily for water) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration|Foam", meta = (EditCondition = "bEnabled"))
	FFoamSettings Foam;

	//========================================
	// Emissive (Lava, Magic)
	//========================================

	/** Emissive/glow settings (for lava, magical fluids) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration|Emissive", meta = (EditCondition = "bEnabled"))
	FEmissiveSettings Emissive;

	//========================================
	// Flow Animation
	//========================================

	/** Flow map settings for animated surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration|Flow", meta = (EditCondition = "bEnabled"))
	FFlowMapSettings FlowMap;

	//========================================
	// Custom Layers
	//========================================

	/** Primary surface texture layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration|Layers", meta = (EditCondition = "bEnabled"))
	FSurfaceDecorationLayer PrimaryLayer;

	/** Secondary surface texture layer (for detail) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration|Layers", meta = (EditCondition = "bEnabled"))
	FSurfaceDecorationLayer SecondaryLayer;

	//========================================
	// Global Settings
	//========================================

	/** Global opacity for all decoration effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float GlobalOpacity = 1.0f;

	/** Blend decoration with base fluid color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Decoration", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float BlendWithFluidColor = 0.5f;

	FSurfaceDecorationParams() = default;
};
