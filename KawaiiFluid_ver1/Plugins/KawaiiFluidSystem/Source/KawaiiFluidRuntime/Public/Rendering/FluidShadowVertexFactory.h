// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LocalVertexFactory.h"
#include "ShaderParameters.h"
#include "Rendering/FluidDensityGrid.h"

/**
 * @brief Uniform buffer for fluid shadow vertex factory.
 *
 * Contains parameters needed for ray marching against the density grid
 * in the shadow depth pass.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FFluidShadowVertexFactoryParameters, KAWAIIFLUIDRUNTIME_API)
	SHADER_PARAMETER(FVector3f, GridBoundsMin)
	SHADER_PARAMETER(FVector3f, GridBoundsMax)
	SHADER_PARAMETER(FVector3f, GridResolution)
	SHADER_PARAMETER(FVector3f, InvGridSize)
	SHADER_PARAMETER(float, SurfaceDensityThreshold)
	SHADER_PARAMETER(int32, MaxRayMarchSteps)
	SHADER_PARAMETER(float, Padding0)
	SHADER_PARAMETER(float, Padding1)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FFluidShadowVertexFactoryParameters> FFluidShadowVertexFactoryParametersRef;

/**
 * @brief Custom vertex factory for fluid shadow rendering.
 *
 * Extends FLocalVertexFactory to provide additional parameters needed
 * for ray marching in the shadow depth pixel shader. The vertex factory
 * itself renders a bounding box, but the pixel shader uses the density
 * grid to compute actual fluid depth via ray marching.
 *
 * @param DensityGridTexture 3D texture containing fluid density.
 * @param UniformBuffer Parameters for ray marching.
 */
class FFluidShadowVertexFactory : public FLocalVertexFactory
{
	// Note: Vertex factory type registration disabled for now.
	// Using FLocalVertexFactory directly.
	// DECLARE_VERTEX_FACTORY_TYPE(FFluidShadowVertexFactory);

public:
	FFluidShadowVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName);

	virtual ~FFluidShadowVertexFactory();

	/**
	 * @brief Set the density grid texture for ray marching.
	 * @param InDensityGridTexture 3D density texture.
	 * @param InDensityGridSampler Sampler state.
	 */
	void SetDensityGrid(FRHITexture* InDensityGridTexture, FRHISamplerState* InDensityGridSampler);

	/**
	 * @brief Set uniform buffer parameters.
	 * @param InParameters Uniform buffer reference.
	 */
	void SetUniformBuffer(const FFluidShadowVertexFactoryParametersRef& InParameters);

	/**
	 * @brief Create uniform buffer from grid config.
	 * @param GridConfig Density grid configuration.
	 * @param SurfaceThreshold Surface density threshold.
	 * @param MaxSteps Maximum ray march steps.
	 * @return Uniform buffer reference.
	 */
	static FFluidShadowVertexFactoryParametersRef CreateUniformBuffer(
		const FFluidDensityGridConfig& GridConfig,
		float SurfaceThreshold,
		int32 MaxSteps);

	/** Get density grid texture. */
	FRHITexture* GetDensityGridTexture() const { return DensityGridTexture; }

	/** Get density grid sampler. */
	FRHISamplerState* GetDensityGridSampler() const { return DensityGridSampler; }

	/** Get uniform buffer. */
	FFluidShadowVertexFactoryParametersRef GetUniformBuffer() const { return UniformBuffer; }

	// FRenderResource interface
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	// Static functions for shader compilation
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment);

private:
	/** 3D density grid texture. */
	FRHITexture* DensityGridTexture = nullptr;

	/** Sampler for density grid. */
	FRHISamplerState* DensityGridSampler = nullptr;

	/** Uniform buffer with ray marching parameters. */
	FFluidShadowVertexFactoryParametersRef UniformBuffer;
};

// Note: Custom shader parameters disabled for now.
// TODO: Implement proper shader parameters if custom bindings are needed.
/*
class FFluidShadowVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FFluidShadowVertexFactoryShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap);

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;

private:
	LAYOUT_FIELD(FShaderResourceParameter, DensityGridTextureParam);
	LAYOUT_FIELD(FShaderResourceParameter, DensityGridSamplerParam);
};
*/
