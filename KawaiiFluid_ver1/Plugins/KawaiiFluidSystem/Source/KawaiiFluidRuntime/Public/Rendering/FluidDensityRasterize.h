// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "Rendering/FluidDensityGrid.h"

class FRDGBuilder;

/**
 * @brief Input parameters for density rasterization.
 * @param ParticlePositionsBuffer Structured buffer containing particle positions (float4: xyz + unused).
 * @param NumParticles Number of particles to rasterize.
 * @param ParticleRadius Radius of influence for each particle in world units.
 */
struct FFluidDensityRasterizeInput
{
	/** Structured buffer of particle positions (xyz = position, w = unused). */
	FRDGBufferRef ParticlePositionsBuffer = nullptr;

	/** SRV for particle positions buffer. */
	FRDGBufferSRVRef ParticlePositionsSRV = nullptr;

	/** Number of particles. */
	int32 NumParticles = 0;

	/** Radius of influence for density splatting. */
	float ParticleRadius = 5.0f;

	/** Grid configuration. */
	FFluidDensityGridConfig GridConfig;
};

/**
 * @brief Output from density rasterization.
 * @param DensityGridTexture 3D texture containing rasterized density values.
 */
struct FFluidDensityRasterizeOutput
{
	/** 3D density grid texture (R16F). */
	FRDGTextureRef DensityGridTexture = nullptr;

	/** Whether the output is valid. */
	bool bIsValid = false;
};

/**
 * @brief Rasterize fluid particles into a 3D density grid.
 *
 * This function dispatches compute shaders to:
 * 1. Clear the density grid
 * 2. Rasterize particles into the grid using density splatting
 * 3. Optionally blur the result for smoother surfaces
 *
 * @param GraphBuilder RDG builder for pass registration.
 * @param Input Rasterization input parameters.
 * @param OutOutput Output containing the density grid texture.
 */
void RenderFluidDensityRasterize(
	FRDGBuilder& GraphBuilder,
	const FFluidDensityRasterizeInput& Input,
	FFluidDensityRasterizeOutput& OutOutput);

/**
 * @brief Create a particle positions buffer from CPU data.
 *
 * @param GraphBuilder RDG builder.
 * @param Positions Array of particle positions.
 * @param NumParticles Number of particles.
 * @return RDG buffer containing particle positions as float4.
 */
FRDGBufferRef CreateParticlePositionsBuffer(
	FRDGBuilder& GraphBuilder,
	const FVector* Positions,
	int32 NumParticles);
