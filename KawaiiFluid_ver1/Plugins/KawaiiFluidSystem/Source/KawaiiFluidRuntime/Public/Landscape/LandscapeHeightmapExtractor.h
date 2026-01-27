// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// LandscapeHeightmapExtractor - Utility for extracting heightmap data from Landscape

#pragma once

#include "CoreMinimal.h"
#include "GPU/GPUFluidParticle.h"

class ALandscapeProxy;

/**
 * FLandscapeHeightmapExtractor
 *
 * Utility class for extracting heightmap data from UE5 Landscape actors.
 * Generates R32F texture data for GPU heightmap collision.
 *
 * Usage:
 *   TArray<ALandscapeProxy*> Landscapes;
 *   // ... collect landscape actors ...
 *
 *   TArray<float> HeightData;
 *   FBox Bounds;
 *   int32 Width, Height;
 *
 *   FLandscapeHeightmapExtractor::ExtractCombinedHeightmap(
 *       Landscapes, HeightData, Width, Height, Bounds, 1024);
 *
 *   Simulator->SetHeightmapCollisionParams(Params);
 *   Simulator->UploadHeightmapData(HeightData, Width, Height);
 */
class KAWAIIFLUIDRUNTIME_API FLandscapeHeightmapExtractor
{
public:
	/**
	 * Extract heightmap data from a single landscape
	 * @param Landscape - Source landscape actor
	 * @param OutHeightData - Output array of normalized height values (0-1)
	 * @param OutWidth - Output texture width
	 * @param OutHeight - Output texture height
	 * @param OutBounds - Output world-space bounds of the heightmap
	 * @param Resolution - Desired resolution (will be clamped to power of 2)
	 * @return true if extraction succeeded
	 */
	static bool ExtractHeightmap(
		ALandscapeProxy* Landscape,
		TArray<float>& OutHeightData,
		int32& OutWidth,
		int32& OutHeight,
		FBox& OutBounds,
		int32 Resolution = 1024);

	/**
	 * Extract combined heightmap from multiple landscapes
	 * Merges all landscapes into a single heightmap with combined bounds
	 * @param Landscapes - Array of landscape actors
	 * @param OutHeightData - Output array of normalized height values (0-1)
	 * @param OutWidth - Output texture width
	 * @param OutHeight - Output texture height
	 * @param OutBounds - Output world-space bounds covering all landscapes
	 * @param Resolution - Desired resolution (will be clamped to power of 2)
	 * @return true if extraction succeeded
	 */
	static bool ExtractCombinedHeightmap(
		const TArray<ALandscapeProxy*>& Landscapes,
		TArray<float>& OutHeightData,
		int32& OutWidth,
		int32& OutHeight,
		FBox& OutBounds,
		int32 Resolution = 1024);

	/**
	 * Build collision parameters from extracted heightmap
	 * @param Bounds - World-space bounds from extraction
	 * @param Width - Texture width
	 * @param Height - Texture height
	 * @param ParticleRadius - Particle radius for collision
	 * @param Friction - Friction coefficient (0-1)
	 * @param Restitution - Restitution/bounciness (0-1)
	 * @return Configured heightmap collision parameters
	 */
	static FGPUHeightmapCollisionParams BuildCollisionParams(
		const FBox& Bounds,
		int32 Width,
		int32 Height,
		float ParticleRadius = 5.0f,
		float Friction = 0.3f,
		float Restitution = 0.1f);

	/**
	 * Find all landscape actors in a world
	 * @param World - World to search
	 * @param OutLandscapes - Output array of found landscapes
	 */
	static void FindLandscapesInWorld(UWorld* World, TArray<ALandscapeProxy*>& OutLandscapes);

private:
	/** Sample height at world XY position from landscape */
	static float SampleLandscapeHeight(ALandscapeProxy* Landscape, float WorldX, float WorldY);

	/** Clamp resolution to power of 2 */
	static int32 ClampToPowerOfTwo(int32 Value, int32 MinValue = 64, int32 MaxValue = 4096);
};
