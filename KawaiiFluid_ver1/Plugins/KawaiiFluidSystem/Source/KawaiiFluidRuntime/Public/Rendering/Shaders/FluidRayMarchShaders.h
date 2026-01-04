// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphDefinitions.h"
#include "Core/KawaiiRenderParticle.h"

/**
 * @brief Shared parameter structure for Ray Marching SDF shaders
 *
 * Used by both vertex and pixel shaders for ray marching fluid rendering.
 * Supports metaball SDF, SSS (subsurface scattering), and refraction.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FFluidRayMarchParameters, )
	//========================================
	// Particle Data (FKawaiiRenderParticle: Position, Velocity, Radius, Padding)
	//========================================
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FKawaiiRenderParticle>, RenderParticles)
	SHADER_PARAMETER(int32, ParticleCount)
	SHADER_PARAMETER(float, ParticleRadius)

	//========================================
	// Particle Bounding Box (카메라가 멀리 있을 때 빈 셀 건너뛰기 방지)
	//========================================
	SHADER_PARAMETER(FVector3f, ParticleBoundsMin)
	SHADER_PARAMETER(FVector3f, ParticleBoundsMax)

	//========================================
	// Ray Marching Parameters
	//========================================
	SHADER_PARAMETER(float, SDFSmoothness)
	SHADER_PARAMETER(int32, MaxRayMarchSteps)
	SHADER_PARAMETER(float, RayMarchHitThreshold)
	SHADER_PARAMETER(float, RayMarchMaxDistance)

	//========================================
	// Appearance Parameters
	//========================================
	SHADER_PARAMETER(FLinearColor, FluidColor)
	SHADER_PARAMETER(float, FresnelStrength)
	SHADER_PARAMETER(float, RefractiveIndex)
	SHADER_PARAMETER(float, AbsorptionCoefficient)
	SHADER_PARAMETER(float, SpecularStrength)
	SHADER_PARAMETER(float, SpecularRoughness)
	SHADER_PARAMETER(FLinearColor, EnvironmentLightColor)

	//========================================
	// SSS Parameters
	//========================================
	SHADER_PARAMETER(float, SSSIntensity)
	SHADER_PARAMETER(FLinearColor, SSSColor)

	//========================================
	// Scene Textures
	//========================================
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneTextureSampler)

	//========================================
	// SDF Volume (for optimized ray marching)
	//========================================
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D<float>, SDFVolumeTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SDFVolumeSampler)
	SHADER_PARAMETER(FVector3f, SDFVolumeMin)
	SHADER_PARAMETER(FVector3f, SDFVolumeMax)
	SHADER_PARAMETER(FIntVector, SDFVolumeResolution)

	//========================================
	// Spatial Hash (for O(k) SDF evaluation)
	// USE_SPATIAL_HASH=1 일 때 사용됨
	// 주의: RenderParticles 버퍼는 위에서 선언됨 (Particle Data 섹션)
	//========================================
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellCounts)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CellStartIndices)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ParticleIndices)
	SHADER_PARAMETER(float, SpatialHashCellSize)

	//========================================
	// SceneDepth UV Mapping
	//========================================
	SHADER_PARAMETER(FVector2f, SceneViewRect)
	SHADER_PARAMETER(FVector2f, SceneTextureSize)

	//========================================
	// View Matrices
	//========================================
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER(FMatrix44f, InverseViewMatrix)
	SHADER_PARAMETER(FMatrix44f, InverseProjectionMatrix)
	SHADER_PARAMETER(FMatrix44f, ViewMatrix)
	SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
	SHADER_PARAMETER(FVector2f, ViewportSize)

	// Light parameters (DirectionalLightDirection, DirectionalLightColor)
	// are accessed directly from View uniform buffer in shader

	//========================================
	// Render Target
	//========================================
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * @brief Vertex shader for Ray Marching (fullscreen triangle)
 *
 * Generates a fullscreen triangle for ray marching in pixel shader.
 */
class FFluidRayMarchVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFluidRayMarchVS);
	SHADER_USE_PARAMETER_STRUCT(FFluidRayMarchVS, FGlobalShader);

	using FParameters = FFluidRayMarchParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

/**
 * @brief Shader permutation dimension for SDF Volume optimization
 */
class FUseSDFVolumeDim : SHADER_PERMUTATION_BOOL("USE_SDF_VOLUME");

/**
 * @brief Shader permutation dimension for Spatial Hash acceleration
 */
class FUseSpatialHashDim : SHADER_PERMUTATION_BOOL("USE_SPATIAL_HASH");

/**
 * @brief Pixel shader for Ray Marching SDF fluid rendering
 *
 * Performs ray marching through metaball SDF field to render
 * smooth fluid surfaces with:
 * - Fresnel reflection
 * - Subsurface scattering (SSS) for jelly effect
 * - Refraction
 * - Specular highlights
 * - Beer's Law absorption
 *
 * Supports three SDF evaluation modes (mutually exclusive):
 * - USE_SDF_VOLUME=1: O(1) volume texture sampling (fastest, requires bake)
 * - USE_SPATIAL_HASH=1: O(k) spatial hash neighbor search (fast, realtime)
 * - Both=0: O(N) particle iteration (slowest, fallback)
 */
class FFluidRayMarchPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFluidRayMarchPS);
	SHADER_USE_PARAMETER_STRUCT(FFluidRayMarchPS, FGlobalShader);

	using FParameters = FFluidRayMarchParameters;
	using FPermutationDomain = TShaderPermutationDomain<FUseSDFVolumeDim, FUseSpatialHashDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// SDF Volume과 Spatial Hash는 상호 배타적
		// 둘 다 켜져 있으면 컴파일하지 않음
		bool bUseSDF = PermutationVector.Get<FUseSDFVolumeDim>();
		bool bUseHash = PermutationVector.Get<FUseSpatialHashDim>();
		if (bUseSDF && bUseHash)
		{
			return false;
		}

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RAY_MARCH_SDF"), 1);
	}
};
