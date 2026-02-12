// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/Resources/GPUFluidParticle.h"

class ALandscapeProxy;

/**
 * @class FKawaiiFluidLandscapeHeightmapExtractor
 * @brief Utility class for extracting heightmap data from UE5 Landscape actors for GPU collision.
 */
class KAWAIIFLUIDRUNTIME_API FKawaiiFluidLandscapeHeightmapExtractor
{
public:
	static bool ExtractHeightmap(
		ALandscapeProxy* Landscape,
		TArray<float>& OutHeightData,
		int32& OutWidth,
		int32& OutHeight,
		FBox& OutBounds,
		int32 Resolution = 1024);

	static bool ExtractCombinedHeightmap(
		const TArray<ALandscapeProxy*>& Landscapes,
		TArray<float>& OutHeightData,
		int32& OutWidth,
		int32& OutHeight,
		FBox& OutBounds,
		int32 Resolution = 1024);

	static FGPUHeightmapCollisionParams BuildCollisionParams(
		const FBox& Bounds,
		int32 Width,
		int32 Height,
		float ParticleRadius = 5.0f,
		float Friction = 0.3f,
		float Restitution = 0.1f);

	static void FindLandscapesInWorld(UWorld* World, TArray<ALandscapeProxy*>& OutLandscapes);

private:
	static float SampleLandscapeHeight(ALandscapeProxy* Landscape, float WorldX, float WorldY);

	static int32 ClampToPowerOfTwo(int32 Value, int32 MinValue = 64, int32 MaxValue = 4096);
};
