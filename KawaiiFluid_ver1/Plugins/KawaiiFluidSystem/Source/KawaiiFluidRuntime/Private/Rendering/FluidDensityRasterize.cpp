// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidDensityRasterize.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCore.h"

//=============================================================================
// Clear Density Grid Compute Shader
//=============================================================================

class FFluidClearDensityGridCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidClearDensityGridCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidClearDensityGridCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, GridResolution)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, DensityGrid)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 4);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 4);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), 4);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFluidClearDensityGridCS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidDensityRasterize.usf",
                        "ClearDensityGridCS",
                        SF_Compute);

//=============================================================================
// Rasterize Particles Compute Shader
//=============================================================================

class FFluidRasterizeParticlesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidRasterizeParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidRasterizeParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, ParticlePositions)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, DensityGrid)
		SHADER_PARAMETER(FVector3f, GridBoundsMin)
		SHADER_PARAMETER(FVector3f, GridBoundsMax)
		SHADER_PARAMETER(FVector3f, GridResolution)
		SHADER_PARAMETER(FVector3f, InvGridSize)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER(int32, NumParticles)
		SHADER_PARAMETER(float, DensityFalloff)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		// Uses [numthreads(64, 1, 1)] defined in shader
	}
};

IMPLEMENT_GLOBAL_SHADER(FFluidRasterizeParticlesCS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidDensityRasterize.usf",
                        "RasterizeParticlesCS",
                        SF_Compute);

//=============================================================================
// Voxel-Centric Rasterization (Alternative)
//=============================================================================

class FFluidRasterizeVoxelCentricCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFluidRasterizeVoxelCentricCS);
	SHADER_USE_PARAMETER_STRUCT(FFluidRasterizeVoxelCentricCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, ParticlePositions)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, DensityGrid)
		SHADER_PARAMETER(FVector3f, GridBoundsMin)
		SHADER_PARAMETER(FVector3f, GridBoundsMax)
		SHADER_PARAMETER(FVector3f, GridResolution)
		SHADER_PARAMETER(FVector3f, InvGridSize)
		SHADER_PARAMETER(float, ParticleRadius)
		SHADER_PARAMETER(int32, NumParticles)
		SHADER_PARAMETER(float, DensityFalloff)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 4);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 4);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Z"), 4);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFluidRasterizeVoxelCentricCS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidDensityRasterize.usf",
                        "RasterizeVoxelCentricCS",
                        SF_Compute);

//=============================================================================
// Render Function Implementation
//=============================================================================

/**
 * @brief Rasterize fluid particles into a 3D density grid.
 * @param GraphBuilder RDG builder for pass registration.
 * @param Input Rasterization input parameters.
 * @param OutOutput Output containing the density grid texture.
 */
void RenderFluidDensityRasterize(
	FRDGBuilder& GraphBuilder,
	const FFluidDensityRasterizeInput& Input,
	FFluidDensityRasterizeOutput& OutOutput)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FluidDensityRasterize");

	// Validate input
	if (Input.NumParticles <= 0 || !Input.ParticlePositionsSRV)
	{
		OutOutput.bIsValid = false;
		return;
	}

	const FFluidDensityGridConfig& GridConfig = Input.GridConfig;
	FIntVector Resolution = GridConfig.Resolution;

	// Create 3D density grid texture
	FRDGTextureDesc DensityGridDesc = FRDGTextureDesc::Create3D(
		FIntVector(Resolution.X, Resolution.Y, Resolution.Z),
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureRef DensityGridTexture = GraphBuilder.CreateTexture(
		DensityGridDesc,
		TEXT("FluidDensityGrid3D"));

	FRDGTextureUAVRef DensityGridUAV = GraphBuilder.CreateUAV(DensityGridTexture);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// Calculate common shader parameters
	FVector3f GridBoundsMin = FVector3f(GridConfig.WorldBoundsMin);
	FVector3f GridBoundsMax = FVector3f(GridConfig.WorldBoundsMax);
	FVector3f GridResolution = FVector3f(
		static_cast<float>(Resolution.X),
		static_cast<float>(Resolution.Y),
		static_cast<float>(Resolution.Z));

	FVector WorldSize = GridConfig.GetWorldSize();
	FVector3f InvGridSize = FVector3f(
		WorldSize.X > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.X : 0.0f,
		WorldSize.Y > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.Y : 0.0f,
		WorldSize.Z > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.Z : 0.0f);

	//=========================================================================
	// Pass 1: Clear Density Grid
	//=========================================================================
	{
		TShaderMapRef<FFluidClearDensityGridCS> ComputeShader(GlobalShaderMap);

		auto* PassParameters = GraphBuilder.AllocParameters<FFluidClearDensityGridCS::FParameters>();
		PassParameters->GridResolution = GridResolution;
		PassParameters->DensityGrid = DensityGridUAV;

		FIntVector GroupCount = FIntVector(
			FMath::DivideAndRoundUp(Resolution.X, 4),
			FMath::DivideAndRoundUp(Resolution.Y, 4),
			FMath::DivideAndRoundUp(Resolution.Z, 4));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearDensityGrid"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	//=========================================================================
	// Pass 2: Rasterize Particles
	// Choose between particle-centric or voxel-centric based on particle count
	//=========================================================================
	const int32 VoxelCentricThreshold = 1000;  // Use voxel-centric for fewer particles
	bool bUseVoxelCentric = (Input.NumParticles < VoxelCentricThreshold);

	if (bUseVoxelCentric)
	{
		// Voxel-centric: each thread processes one voxel
		TShaderMapRef<FFluidRasterizeVoxelCentricCS> ComputeShader(GlobalShaderMap);

		auto* PassParameters = GraphBuilder.AllocParameters<FFluidRasterizeVoxelCentricCS::FParameters>();
		PassParameters->ParticlePositions = Input.ParticlePositionsSRV;
		PassParameters->DensityGrid = DensityGridUAV;
		PassParameters->GridBoundsMin = GridBoundsMin;
		PassParameters->GridBoundsMax = GridBoundsMax;
		PassParameters->GridResolution = GridResolution;
		PassParameters->InvGridSize = InvGridSize;
		PassParameters->ParticleRadius = Input.ParticleRadius;
		PassParameters->NumParticles = Input.NumParticles;
		PassParameters->DensityFalloff = 1.0f;

		FIntVector GroupCount = FIntVector(
			FMath::DivideAndRoundUp(Resolution.X, 4),
			FMath::DivideAndRoundUp(Resolution.Y, 4),
			FMath::DivideAndRoundUp(Resolution.Z, 4));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RasterizeVoxelCentric"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}
	else
	{
		// Particle-centric: each thread processes one particle
		TShaderMapRef<FFluidRasterizeParticlesCS> ComputeShader(GlobalShaderMap);

		auto* PassParameters = GraphBuilder.AllocParameters<FFluidRasterizeParticlesCS::FParameters>();
		PassParameters->ParticlePositions = Input.ParticlePositionsSRV;
		PassParameters->DensityGrid = DensityGridUAV;
		PassParameters->GridBoundsMin = GridBoundsMin;
		PassParameters->GridBoundsMax = GridBoundsMax;
		PassParameters->GridResolution = GridResolution;
		PassParameters->InvGridSize = InvGridSize;
		PassParameters->ParticleRadius = Input.ParticleRadius;
		PassParameters->NumParticles = Input.NumParticles;
		PassParameters->DensityFalloff = 1.0f;

		int32 GroupCount = FMath::DivideAndRoundUp(Input.NumParticles, 64);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RasterizeParticles"),
			ComputeShader,
			PassParameters,
			FIntVector(GroupCount, 1, 1));
	}

	// Set output
	OutOutput.DensityGridTexture = DensityGridTexture;
	OutOutput.bIsValid = true;
}

/**
 * @brief Create a particle positions buffer from CPU data.
 * @param GraphBuilder RDG builder.
 * @param Positions Array of particle positions.
 * @param NumParticles Number of particles.
 * @return RDG buffer containing particle positions as float4.
 */
FRDGBufferRef CreateParticlePositionsBuffer(
	FRDGBuilder& GraphBuilder,
	const FVector* Positions,
	int32 NumParticles)
{
	if (NumParticles <= 0 || Positions == nullptr)
	{
		return nullptr;
	}

	// Create buffer description
	FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), NumParticles);

	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(
		BufferDesc,
		TEXT("FluidParticlePositions"));

	// Upload data
	TArray<FVector4f> PackedPositions;
	PackedPositions.SetNum(NumParticles);

	for (int32 i = 0; i < NumParticles; ++i)
	{
		PackedPositions[i] = FVector4f(
			static_cast<float>(Positions[i].X),
			static_cast<float>(Positions[i].Y),
			static_cast<float>(Positions[i].Z),
			1.0f);
	}

	GraphBuilder.QueueBufferUpload(
		Buffer,
		PackedPositions.GetData(),
		NumParticles * sizeof(FVector4f),
		ERDGInitialDataFlags::None);

	return Buffer;
}
