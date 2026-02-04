// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "SceneTextureParameters.h"
#include "SceneView.h"

/** Maximum number of lights supported for fluid composite shading. */
static constexpr int32 FLUID_MAX_LIGHTS = 8;

/**
 * @brief Shader parameters for composite pass.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FFluidCompositeParameters, )
    // ------------------------------------------------------
    // Fluid Input Textures
    // ------------------------------------------------------
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidDepthTexture)     // Linear Depth (R32F)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidNormalTexture)    // Reconstructed Normal
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidThicknessTexture) // Thickness
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OcclusionMaskTexture)  // 1.0=visible, 0.0=occluded

    // ------------------------------------------------------
    // Scene Input
    // ------------------------------------------------------
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
    SHADER_PARAMETER_SAMPLER(SamplerState, PointClampSampler)  // Point sampling for depth textures

    // UV scaling for SceneColor/SceneDepth (ViewRect / TextureSize)
    // Needed when texture size differs from ViewRect (e.g., Screen Percentage)
    SHADER_PARAMETER(FVector2f, SceneUVScale)
    SHADER_PARAMETER(FVector2f, SceneUVOffset)
    SHADER_PARAMETER(FVector2f, FluidUVScale)
    SHADER_PARAMETER(FVector2f, FluidUVOffset)

    // ------------------------------------------------------
    // Matrices
    // ------------------------------------------------------
    SHADER_PARAMETER(FMatrix44f, InverseProjectionMatrix)
    SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
    SHADER_PARAMETER(FMatrix44f, ViewMatrix)

    // ------------------------------------------------------
    // Fluid Settings
    // ------------------------------------------------------
    SHADER_PARAMETER(FLinearColor, FluidColor)
    SHADER_PARAMETER(float, FresnelStrength)
    SHADER_PARAMETER(float, RefractiveIndex)
    SHADER_PARAMETER(float, Opacity)              // Fluid opacity (0 = transparent, 1 = opaque)
    SHADER_PARAMETER(FLinearColor, AbsorptionColorCoefficients)  // Per-channel absorption (Beer's Law)
    SHADER_PARAMETER(float, SpecularStrength)
    SHADER_PARAMETER(float, SpecularRoughness)
    SHADER_PARAMETER(float, AmbientIntensity)  // SkyLight contribution scale (default 0.15)
    SHADER_PARAMETER(float, LightingScale)    // Overall lighting scale for HDR compensation (default 0.2)

    // ------------------------------------------------------
    // Multi-Light Support
    // Packed as float4 arrays for shader compatibility:
    // - LightDirectionsAndIntensity[i] = (Direction.xyz, Intensity)
    // - LightColors[i] = (Color.rgb, unused)
    // ------------------------------------------------------
    SHADER_PARAMETER(int, NumLights)  // Number of active lights (0 = use View.DirectionalLight fallback)
    SHADER_PARAMETER_ARRAY(FVector4f, LightDirectionsAndIntensity, [FLUID_MAX_LIGHTS])
    SHADER_PARAMETER_ARRAY(FVector4f, LightColors, [FLUID_MAX_LIGHTS])

    // ------------------------------------------------------
    // Lighting Scale Parameters
    // ------------------------------------------------------
    SHADER_PARAMETER(float, ThicknessSensitivity)  // How much thickness affects transparency (0 = uniform, 1 = thickness-dependent)
    SHADER_PARAMETER(int, bEnableThicknessClamping)  // 1 = clamp thickness to min/max, 0 = no clamping
    SHADER_PARAMETER(float, ThicknessMin)  // Minimum thickness value (when clamping enabled)
    SHADER_PARAMETER(float, ThicknessMax)  // Maximum thickness value (when clamping enabled)
    SHADER_PARAMETER(float, FresnelReflectionBlend)

    // ------------------------------------------------------
    // Refraction
    // ------------------------------------------------------
    SHADER_PARAMETER(int, bEnableRefraction)  // 1 = enabled, 0 = disabled
    SHADER_PARAMETER(float, RefractionScale)

    // ------------------------------------------------------
    // Caustics
    // ------------------------------------------------------
    SHADER_PARAMETER(int, bEnableCaustics)    // 1 = enabled, 0 = disabled
    SHADER_PARAMETER(float, CausticIntensity) // Brightness multiplier for caustic patterns

    // ------------------------------------------------------
    // Reflection Cubemap
    // ------------------------------------------------------
    SHADER_PARAMETER_TEXTURE(TextureCube, ReflectionCubemap)
    SHADER_PARAMETER_SAMPLER(SamplerState, ReflectionCubemapSampler)
    SHADER_PARAMETER(float, ReflectionIntensity)
    SHADER_PARAMETER(float, ReflectionMipLevel)
    SHADER_PARAMETER(int, bUseReflectionCubemap)  // 1 = use Cubemap, 0 = fallback color

    // ------------------------------------------------------
    // Reflection Mode (0=None, 1=Cubemap, 2=SSR, 3=SSR+Cubemap)
    // ------------------------------------------------------
    SHADER_PARAMETER(int, ReflectionMode)                         // Reflection mode enum
    SHADER_PARAMETER(int, ScreenSpaceReflectionMaxSteps)          // Ray march max steps
    SHADER_PARAMETER(float, ScreenSpaceReflectionStepSize)        // Step size (pixels)
    SHADER_PARAMETER(float, ScreenSpaceReflectionThickness)       // Hit detection thickness
    SHADER_PARAMETER(float, ScreenSpaceReflectionIntensity)       // SSR intensity
    SHADER_PARAMETER(float, ScreenSpaceReflectionEdgeFade)        // Screen edge fade
    SHADER_PARAMETER(FVector2f, ViewportSize)   // Viewport size (pixels)

    // ------------------------------------------------------
    // Render Targets (Output)
    // ------------------------------------------------------
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FFluidCompositeVS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFluidCompositeVS);
    SHADER_USE_PARAMETER_STRUCT(FFluidCompositeVS, FGlobalShader);
    
    using FParameters = FFluidCompositeParameters;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

/**
 * Fluid Composite Pixel Shader
 */
class FFluidCompositePS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFluidCompositePS);
    SHADER_USE_PARAMETER_STRUCT(FFluidCompositePS, FGlobalShader);

    using FParameters = FFluidCompositeParameters;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};