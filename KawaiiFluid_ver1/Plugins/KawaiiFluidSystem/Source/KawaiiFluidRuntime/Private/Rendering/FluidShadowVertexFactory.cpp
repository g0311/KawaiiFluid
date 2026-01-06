// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidShadowVertexFactory.h"
#include "MeshMaterialShader.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"

//=============================================================================
// Uniform Buffer Implementation
//=============================================================================

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FFluidShadowVertexFactoryParameters, "FluidShadowVF");

//=============================================================================
// Vertex Factory Implementation
//=============================================================================

// Note: Custom vertex factory registration disabled for now.
// Using FLocalVertexFactory directly in FluidShadowSceneProxy.
// TODO: Implement proper vertex factory if custom shader bindings are needed.

// IMPLEMENT_VERTEX_FACTORY_TYPE(FFluidShadowVertexFactory, "/Plugin/KawaiiFluidSystem/Private/FluidShadowVertexFactory.usf",
// 	EVertexFactoryFlags::UsedWithMaterials |
// 	EVertexFactoryFlags::SupportsDynamicLighting |
// 	EVertexFactoryFlags::SupportsCachingMeshDrawCommands);

// IMPLEMENT_TYPE_LAYOUT(FFluidShadowVertexFactoryShaderParameters);

/**
 * @brief Construct the vertex factory.
 * @param InFeatureLevel RHI feature level.
 * @param InDebugName Debug name for resource tracking.
 */
FFluidShadowVertexFactory::FFluidShadowVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName)
	: FLocalVertexFactory(InFeatureLevel, InDebugName)
{
}

FFluidShadowVertexFactory::~FFluidShadowVertexFactory()
{
}

/**
 * @brief Set the density grid texture.
 * @param InDensityGridTexture 3D density texture.
 * @param InDensityGridSampler Sampler state.
 */
void FFluidShadowVertexFactory::SetDensityGrid(FRHITexture* InDensityGridTexture, FRHISamplerState* InDensityGridSampler)
{
	DensityGridTexture = InDensityGridTexture;
	DensityGridSampler = InDensityGridSampler;
}

/**
 * @brief Set uniform buffer parameters.
 * @param InParameters Uniform buffer reference.
 */
void FFluidShadowVertexFactory::SetUniformBuffer(const FFluidShadowVertexFactoryParametersRef& InParameters)
{
	UniformBuffer = InParameters;
}

/**
 * @brief Create uniform buffer from grid config.
 * @param GridConfig Density grid configuration.
 * @param SurfaceThreshold Surface density threshold.
 * @param MaxSteps Maximum ray march steps.
 * @return Uniform buffer reference.
 */
FFluidShadowVertexFactoryParametersRef FFluidShadowVertexFactory::CreateUniformBuffer(
	const FFluidDensityGridConfig& GridConfig,
	float SurfaceThreshold,
	int32 MaxSteps)
{
	FFluidShadowVertexFactoryParameters Parameters;

	Parameters.GridBoundsMin = FVector3f(GridConfig.WorldBoundsMin);
	Parameters.GridBoundsMax = FVector3f(GridConfig.WorldBoundsMax);
	Parameters.GridResolution = FVector3f(
		static_cast<float>(GridConfig.Resolution.X),
		static_cast<float>(GridConfig.Resolution.Y),
		static_cast<float>(GridConfig.Resolution.Z));

	FVector WorldSize = GridConfig.GetWorldSize();
	Parameters.InvGridSize = FVector3f(
		WorldSize.X > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.X : 0.0f,
		WorldSize.Y > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.Y : 0.0f,
		WorldSize.Z > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.Z : 0.0f);

	Parameters.SurfaceDensityThreshold = SurfaceThreshold;
	Parameters.MaxRayMarchSteps = MaxSteps;
	Parameters.Padding0 = 0.0f;
	Parameters.Padding1 = 0.0f;

	return TUniformBufferRef<FFluidShadowVertexFactoryParameters>::CreateUniformBufferImmediate(
		Parameters, UniformBuffer_SingleFrame);
}

/**
 * @brief Initialize RHI resources.
 * @param RHICmdList Command list.
 */
void FFluidShadowVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	FLocalVertexFactory::InitRHI(RHICmdList);
}

/**
 * @brief Release RHI resources.
 */
void FFluidShadowVertexFactory::ReleaseRHI()
{
	DensityGridTexture = nullptr;
	DensityGridSampler = nullptr;
	UniformBuffer.SafeRelease();

	FLocalVertexFactory::ReleaseRHI();
}

/**
 * @brief Check if permutation should be compiled.
 * @param Parameters Permutation parameters.
 * @return True if should compile.
 */
bool FFluidShadowVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	// Only compile for shadow depth and base passes on SM5+
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
		FLocalVertexFactory::ShouldCompilePermutation(Parameters);
}

/**
 * @brief Modify shader compilation environment.
 * @param Parameters Permutation parameters.
 * @param OutEnvironment Output compilation environment.
 */
void FFluidShadowVertexFactory::ModifyCompilationEnvironment(
	const FVertexFactoryShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FLocalVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	// Enable fluid shadow features
	OutEnvironment.SetDefine(TEXT("FLUID_SHADOW_VERTEX_FACTORY"), 1);
}

//=============================================================================
// Shader Parameters Implementation (Disabled)
//=============================================================================

// Note: Custom shader parameters disabled for now.
// TODO: Implement proper shader parameters if custom bindings are needed.
/*
void FFluidShadowVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	DensityGridTextureParam.Bind(ParameterMap, TEXT("FluidDensityGridTexture"));
	DensityGridSamplerParam.Bind(ParameterMap, TEXT("FluidDensityGridSampler"));
}

void FFluidShadowVertexFactoryShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	const FFluidShadowVertexFactory* FluidVF = static_cast<const FFluidShadowVertexFactory*>(VertexFactory);

	if (DensityGridTextureParam.IsBound())
	{
		FRHITexture* Texture = FluidVF->GetDensityGridTexture();
		if (Texture)
		{
			ShaderBindings.Add(DensityGridTextureParam, Texture);
		}
	}

	if (DensityGridSamplerParam.IsBound())
	{
		FRHISamplerState* Sampler = FluidVF->GetDensityGridSampler();
		if (!Sampler)
		{
			Sampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		}
		ShaderBindings.Add(DensityGridSamplerParam, Sampler);
	}

	FFluidShadowVertexFactoryParametersRef UB = FluidVF->GetUniformBuffer();
	if (UB.IsValid())
	{
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FFluidShadowVertexFactoryParameters>(), UB);
	}
}
*/
