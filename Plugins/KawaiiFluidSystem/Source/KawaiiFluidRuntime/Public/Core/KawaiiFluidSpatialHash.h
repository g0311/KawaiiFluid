// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @class FKawaiiFluidSpatialHash
 * @brief CPU-side spatial hashing implementation for O(n) neighbor lookup.
 * 
 * This class optimizes neighbor particle search by partitioning 3D space into discrete cells.
 * It uses a TMap-based hash table for efficient lookup of particles within specific coordinates.
 * Note: GPU simulation uses Z-Order Morton code sorting instead of this traditional approach.
 * 
 * @param CellSize The dimension of each cube cell in the spatial grid (typically set to SmoothingRadius).
 * @param Grid The hash grid mapping 3D integer coordinates to arrays of particle indices.
 * @param CachedPositions Internal cache of particle positions used for distance-based filtering.
 * @param RebuildCounter Accumulator used to trigger periodic memory cleanup of empty cells.
 * @param PurgeInterval Number of Clear calls between full grid purges (default: 300).
 */
class KAWAIIFLUIDRUNTIME_API FKawaiiFluidSpatialHash
{
public:
	FKawaiiFluidSpatialHash();
	FKawaiiFluidSpatialHash(float InCellSize);

	void Clear();

	void SetCellSize(float NewCellSize);

	void Insert(int32 ParticleIndex, const FVector& Position);

	void GetNeighbors(const FVector& Position, float Radius, TArray<int32>& OutNeighbors) const;

	void QueryBox(const FBox& Box, TArray<int32>& OutIndices) const;

	void BuildFromPositions(const TArray<FVector>& Positions);

	const TMap<FIntVector, TArray<int32>>& GetGrid() const { return Grid; }

	float GetCellSize() const { return CellSize; }

private:
	float CellSize;

	TMap<FIntVector, TArray<int32>> Grid;

	TArray<FVector> CachedPositions;

	int32 RebuildCounter = 0;

	static constexpr int32 PurgeInterval = 300;

	FIntVector GetCellCoord(const FVector& Position) const;
};