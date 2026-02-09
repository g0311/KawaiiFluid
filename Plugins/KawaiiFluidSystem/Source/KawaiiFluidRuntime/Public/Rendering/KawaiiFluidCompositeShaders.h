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
 * @struct FFluidCompositeParameters
 * @brief Shader parameters for the fluid composite post-process pass.
 * 
 * @param FluidDepthTexture Linear depth texture containing fluid surface distances (R32F).
 * @param FluidNormalTexture Texture containing reconstructed world-space normals.
 * @param FluidThicknessTexture Texture representing accumulated view-space thickness.
 * @param OcclusionMaskTexture Binary mask where 1.0 indicates visible fluid.
 * @param SceneDepthTexture The existing hardware scene depth texture.
 * @param SceneColorTexture The existing scene color texture used as background.
 * @param View Reference to the scene view uniform buffer.
 * @param InputSampler Bilinear sampler for textures.
 * @param PointClampSampler Point sampler for depth/mask textures.
 * @param SceneUVScale UV scaling vector for scene textures.
 * @param SceneUVOffset UV offset vector for scene textures.
 * @param FluidUVScale UV scaling vector for fluid textures.
 * @param FluidUVOffset UV offset vector for fluid textures.
 * @param InverseProjectionMatrix Inverse camera projection matrix.
 * @param ProjectionMatrix Camera projection matrix.
 * @param ViewMatrix Camera view matrix.
 * @param FluidColor Base diffuse/absorption color of the fluid.
 * @param FresnelStrength Intensity of the Fresnel reflection.
 * @param RefractiveIndex Index of refraction (IOR).
 * @param Opacity Global fluid opacity multiplier.
 * @param AbsorptionColorCoefficients RGB absorption coefficients (Beer's Law).
 * @param SpecularStrength Specular highlight intensity.
 * @param SpecularRoughness Specular BRDF roughness.
 * @param AmbientIntensity SkyLight contribution scale.
 * @param LightingScale Overall lighting multiplier for HDR.
 * @param NumLights Number of active lights (0 = directional fallback).
 * @param LightDirectionsAndIntensity Packed array of light directions and intensities.
 * @param LightColors Array of light RGB colors.
 * @param ThicknessSensitivity Controls thickness-dependent transmittance.
 * @param bEnableThicknessClamping Toggle for restricting thickness range.
 * @param ThicknessMin Minimum thickness floor.
 * @param ThicknessMax Maximum thickness ceiling.
 * @param FresnelReflectionBlend Fresnel reflection blend factor.
 * @param bEnableRefraction Toggle for screen-space refraction.
 * @param RefractionScale Refraction distortion magnitude.
 * @param bEnableCaustics Toggle for synthetic caustic projection.
 * @param CausticIntensity Caustic pattern brightness.
 * @param ReflectionCubemap Environment cubemap for reflections.
 * @param ReflectionCubemapSampler Sampler for the cubemap.
 * @param ReflectionIntensity Cubemap reflection strength.
 * @param ReflectionMipLevel Mip level used for reflection sampling.
 * @param bUseReflectionCubemap Flag for valid cubemap presence.
 * @param ReflectionMode Current reflection technique (SSR/Cubemap).
 * @param ScreenSpaceReflectionMaxSteps Max ray march steps for SSR.
 * @param ScreenSpaceReflectionStepSize Distance between SSR steps.
 * @param ScreenSpaceReflectionThickness Ray hit detection thickness.
 * @param ScreenSpaceReflectionIntensity Magnitude of the SSR effect.
 * @param ScreenSpaceReflectionEdgeFade Screen edge fade-out for SSR.
 * @param ViewportSize Current viewport dimensions in pixels.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FFluidCompositeParameters, )
    // ------------------------------------------------------
    // Fluid Input Textures
    // ------------------------------------------------------
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidDepthTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidNormalTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FluidThicknessTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OcclusionMaskTexture)

    // ------------------------------------------------------
    // Scene Input
    // ------------------------------------------------------
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
    SHADER_PARAMETER_SAMPLER(SamplerState, PointClampSampler)

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
    SHADER_PARAMETER(float, Opacity)
    SHADER_PARAMETER(FLinearColor, AbsorptionColorCoefficients)
    SHADER_PARAMETER(float, SpecularStrength)
    SHADER_PARAMETER(float, SpecularRoughness)
    SHADER_PARAMETER(float, AmbientIntensity)
    SHADER_PARAMETER(float, LightingScale)

    // ------------------------------------------------------
    // Multi-Light Support
    // ------------------------------------------------------
    SHADER_PARAMETER(int, NumLights)
    SHADER_PARAMETER_ARRAY(FVector4f, LightDirectionsAndIntensity, [FLUID_MAX_LIGHTS])
    SHADER_PARAMETER_ARRAY(FVector4f, LightColors, [FLUID_MAX_LIGHTS])

    // ------------------------------------------------------
    // Lighting Scale Parameters
    // ------------------------------------------------------
    SHADER_PARAMETER(float, ThicknessSensitivity)
    SHADER_PARAMETER(int, bEnableThicknessClamping)
    SHADER_PARAMETER(float, ThicknessMin)
    SHADER_PARAMETER(float, ThicknessMax)
    SHADER_PARAMETER(float, FresnelReflectionBlend)

    // ------------------------------------------------------
    // Refraction
    // ------------------------------------------------------
    SHADER_PARAMETER(int, bEnableRefraction)
    SHADER_PARAMETER(float, RefractionScale)

    // ------------------------------------------------------
    // Caustics
    // ------------------------------------------------------
    SHADER_PARAMETER(int, bEnableCaustics)
    SHADER_PARAMETER(float, CausticIntensity)

    // ------------------------------------------------------
    // Reflection Cubemap
    // ------------------------------------------------------
    SHADER_PARAMETER_TEXTURE(TextureCube, ReflectionCubemap)
    SHADER_PARAMETER_SAMPLER(SamplerState, ReflectionCubemapSampler)
    SHADER_PARAMETER(float, ReflectionIntensity)
    SHADER_PARAMETER(float, ReflectionMipLevel)
    SHADER_PARAMETER(int, bUseReflectionCubemap)

    // ------------------------------------------------------
    // Reflection Mode
    // ------------------------------------------------------
    SHADER_PARAMETER(int, ReflectionMode)
    SHADER_PARAMETER(int, ScreenSpaceReflectionMaxSteps)
    SHADER_PARAMETER(float, ScreenSpaceReflectionStepSize)
    SHADER_PARAMETER(float, ScreenSpaceReflectionThickness)
    SHADER_PARAMETER(float, ScreenSpaceReflectionIntensity)
    SHADER_PARAMETER(float, ScreenSpaceReflectionEdgeFade)
    SHADER_PARAMETER(FVector2f, ViewportSize)

    // ------------------------------------------------------
    // Render Targets (Output)
    // ------------------------------------------------------
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * @class FKawaiiFluidCompositeVS
 * @brief Vertex shader for the fluid composite post-process triangle.
 */
class FKawaiiFluidCompositeVS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FKawaiiFluidCompositeVS);
    SHADER_USE_PARAMETER_STRUCT(FKawaiiFluidCompositeVS, FGlobalShader);
    
    using FParameters = FFluidCompositeParameters;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

/**
 * @class FKawaiiFluidCompositePS
 * @brief Pixel shader implementing Blinn-Phong, Fresnel, and Beer's Law for fluid composite shading.
 */
class FKawaiiFluidCompositePS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FKawaiiFluidCompositePS);
    SHADER_USE_PARAMETER_STRUCT(FKawaiiFluidCompositePS, FGlobalShader);

    using FParameters = FFluidCompositeParameters;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};
