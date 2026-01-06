// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidDensityGrid.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "RHI.h"

FFluidDensityGrid::FFluidDensityGrid()
	: bIsInitialized(false)
{
}

FFluidDensityGrid::~FFluidDensityGrid()
{
	Release();
}

/**
 * @brief Initialize the density grid with given configuration.
 * @param InConfig Grid configuration including resolution and bounds.
 * @return True if initialization successful.
 */
bool FFluidDensityGrid::Initialize(const FFluidDensityGridConfig& InConfig)
{
	if (bIsInitialized && !NeedsReallocation(InConfig))
	{
		// Just update config without reallocating
		Config = InConfig;
		return true;
	}

	Release();
	Config = InConfig;

	if (!CreateRenderTarget())
	{
		return false;
	}

	bIsInitialized = true;
	return true;
}

/**
 * @brief Release all GPU resources.
 */
void FFluidDensityGrid::Release()
{
	DensityRT.SafeRelease();
	bIsInitialized = false;
}

/**
 * @brief Update grid bounds based on particle positions.
 * @param ParticlePositions Array of particle world positions.
 * @param NumParticles Number of particles.
 */
void FFluidDensityGrid::UpdateBoundsFromParticles(const FVector* ParticlePositions, int32 NumParticles)
{
	if (NumParticles <= 0 || ParticlePositions == nullptr)
	{
		return;
	}

	// Calculate AABB from particles
	FVector MinBounds = ParticlePositions[0];
	FVector MaxBounds = ParticlePositions[0];

	for (int32 i = 1; i < NumParticles; ++i)
	{
		MinBounds = MinBounds.ComponentMin(ParticlePositions[i]);
		MaxBounds = MaxBounds.ComponentMax(ParticlePositions[i]);
	}

	// Add padding
	MinBounds -= FVector(Config.BoundsPadding);
	MaxBounds += FVector(Config.BoundsPadding);

	// Update config
	Config.WorldBoundsMin = MinBounds;
	Config.WorldBoundsMax = MaxBounds;
}

/**
 * @brief Check if the grid needs reallocation due to config changes.
 * @param NewConfig New configuration to compare against.
 * @return True if reallocation is needed.
 */
bool FFluidDensityGrid::NeedsReallocation(const FFluidDensityGridConfig& NewConfig) const
{
	// Only need to reallocate if resolution changes
	return Config.Resolution != NewConfig.Resolution;
}

/**
 * @brief Register the density texture with RDG for rendering.
 * @param GraphBuilder RDG builder.
 * @return RDG texture reference, or nullptr if invalid.
 */
FRDGTextureRef FFluidDensityGrid::RegisterWithRDG(FRDGBuilder& GraphBuilder) const
{
	if (!DensityRT.IsValid())
	{
		return nullptr;
	}

	return GraphBuilder.RegisterExternalTexture(DensityRT, TEXT("FluidDensityGrid"));
}

/**
 * @brief Get world-to-grid transform matrix.
 * @return Matrix that transforms world coordinates to normalized grid coordinates [0,1].
 */
FMatrix FFluidDensityGrid::GetWorldToGridMatrix() const
{
	FVector WorldSize = Config.GetWorldSize();
	FVector InvSize = FVector(
		WorldSize.X > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.X : 0.0f,
		WorldSize.Y > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.Y : 0.0f,
		WorldSize.Z > KINDA_SMALL_NUMBER ? 1.0f / WorldSize.Z : 0.0f
	);

	// Scale and translate: (WorldPos - BoundsMin) / WorldSize
	FMatrix ScaleMatrix = FScaleMatrix(InvSize);
	FMatrix TranslateMatrix = FTranslationMatrix(-Config.WorldBoundsMin);

	return TranslateMatrix * ScaleMatrix;
}

/**
 * @brief Get grid-to-world transform matrix.
 * @return Matrix that transforms normalized grid coordinates [0,1] to world coordinates.
 */
FMatrix FFluidDensityGrid::GetGridToWorldMatrix() const
{
	FVector WorldSize = Config.GetWorldSize();

	// Scale and translate: GridPos * WorldSize + BoundsMin
	FMatrix ScaleMatrix = FScaleMatrix(WorldSize);
	FMatrix TranslateMatrix = FTranslationMatrix(Config.WorldBoundsMin);

	return ScaleMatrix * TranslateMatrix;
}

/**
 * @brief Get shader parameters for binding.
 * @param OutBoundsMin Output bounds min corner in world space.
 * @param OutBoundsMax Output bounds max corner in world space.
 * @param OutResolution Output grid resolution as float3.
 * @param OutVoxelSize Output voxel size in world units.
 */
void FFluidDensityGrid::GetShaderParameters(
	FVector3f& OutBoundsMin,
	FVector3f& OutBoundsMax,
	FVector3f& OutResolution,
	FVector3f& OutVoxelSize) const
{
	OutBoundsMin = FVector3f(Config.WorldBoundsMin);
	OutBoundsMax = FVector3f(Config.WorldBoundsMax);
	OutResolution = FVector3f(
		static_cast<float>(Config.Resolution.X),
		static_cast<float>(Config.Resolution.Y),
		static_cast<float>(Config.Resolution.Z)
	);

	FVector VoxelSize = Config.GetVoxelSize();
	OutVoxelSize = FVector3f(VoxelSize);
}

/**
 * @brief Create the 3D render target for density storage.
 * @return True if creation successful.
 */
bool FFluidDensityGrid::CreateRenderTarget()
{
	// Validate resolution
	if (Config.Resolution.X <= 0 || Config.Resolution.Y <= 0 || Config.Resolution.Z <= 0)
	{
		return false;
	}

	// Create 3D texture description
	FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::CreateVolumeDesc(
		Config.Resolution.X,
		Config.Resolution.Y,
		Config.Resolution.Z,
		PF_R16F,  // Single channel float for density
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV,
		TexCreate_None,  // TargetableFlags
		false,  // bInForceSeparateTargetAndShaderResource
		1,      // NumMips
		false   // bInAutowritable
	);

	GRenderTargetPool.FindFreeElement(
		FRHICommandListImmediate::Get(),
		Desc,
		DensityRT,
		TEXT("FluidDensityGrid3D")
	);

	return DensityRT.IsValid();
}
