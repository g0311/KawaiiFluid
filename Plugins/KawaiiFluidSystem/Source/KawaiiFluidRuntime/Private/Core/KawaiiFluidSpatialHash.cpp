// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Core/KawaiiFluidSpatialHash.h"

/**
 * @brief Default constructor initializing with a default cell size of 1.0.
 */
FKawaiiFluidSpatialHash::FKawaiiFluidSpatialHash()
	: CellSize(1.0f)
{
}

/**
 * @brief Constructor with a custom cell size.
 * @param InCellSize The initial dimension for each grid cell.
 */
FKawaiiFluidSpatialHash::FKawaiiFluidSpatialHash(float InCellSize)
	: CellSize(InCellSize)
{
}

/**
 * @brief Clear all particles from the grid.
 * Periodically purges the entire map to prevent memory growth from empty cell keys.
 */
void FKawaiiFluidSpatialHash::Clear()
{
	++RebuildCounter;

	// Periodically purge empty cells (prevent memory leaks)
	if (RebuildCounter >= PurgeInterval)
	{
		Grid.Empty();
		CachedPositions.Empty();
		RebuildCounter = 0;
	}
	else
	{
		// Clear without reallocation (maintain capacity)
		for (auto& Pair : Grid)
		{
			Pair.Value.Reset();
		}
		CachedPositions.Reset();
	}
}

/**
 * @brief Update the dimension of the grid cells.
 * @param NewCellSize The new cell size in cm (clamped to a minimum of 0.01).
 */
void FKawaiiFluidSpatialHash::SetCellSize(float NewCellSize)
{
	CellSize = FMath::Max(NewCellSize, 0.01f);
}

/**
 * @brief Insert a single particle index into its corresponding cell.
 * @param ParticleIndex The index of the particle in its source array.
 * @param Position The world-space position of the particle.
 */
void FKawaiiFluidSpatialHash::Insert(int32 ParticleIndex, const FVector& Position)
{
	FIntVector CellCoord = GetCellCoord(Position);
	Grid.FindOrAdd(CellCoord).Add(ParticleIndex);
}

/**
 * @brief Retrieve indices of particles within a spherical radius of a position.
 * @param Position The center of the search sphere.
 * @param Radius The interaction radius.
 * @param OutNeighbors Output array to be populated with neighbor indices.
 */
void FKawaiiFluidSpatialHash::GetNeighbors(const FVector& Position, float Radius, TArray<int32>& OutNeighbors) const
{
	OutNeighbors.Reset();

	int32 CellRadius = FMath::CeilToInt(Radius / CellSize);
	FIntVector CenterCell = GetCellCoord(Position);

	const float RadiusSq = Radius * Radius;
	const bool bHasCachedPositions = CachedPositions.Num() > 0;

	for (int32 x = -CellRadius; x <= CellRadius; ++x)
	{
		for (int32 y = -CellRadius; y <= CellRadius; ++y)
		{
			for (int32 z = -CellRadius; z <= CellRadius; ++z)
			{
				FIntVector CellCoord = CenterCell + FIntVector(x, y, z);

				if (const TArray<int32>* CellParticles = Grid.Find(CellCoord))
				{
					// Distance filtering: only add particles within actual radius
					if (bHasCachedPositions)
					{
						for (int32 Idx : *CellParticles)
						{
							if (FVector::DistSquared(Position, CachedPositions[Idx]) <= RadiusSq)
							{
								OutNeighbors.Add(Idx);
							}
						}
					}
					else
					{
						OutNeighbors.Append(*CellParticles);
					}
				}
			}
		}
	}
}

/**
 * @brief Find all particle indices within an axis-aligned box region.
 * @param Box The world-space AABB to query.
 * @param OutIndices Output array to be populated with indices.
 */
void FKawaiiFluidSpatialHash::QueryBox(const FBox& Box, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();

	FIntVector MinCell = GetCellCoord(Box.Min);
	FIntVector MaxCell = GetCellCoord(Box.Max);

	for (int32 x = MinCell.X; x <= MaxCell.X; ++x)
	{
		for (int32 y = MinCell.Y; y <= MaxCell.Y; ++y)
		{
			for (int32 z = MinCell.Z; z <= MaxCell.Z; ++z)
			{
				if (const TArray<int32>* Cell = Grid.Find(FIntVector(x, y, z)))
				{
					OutIndices.Append(*Cell);
				}
			}
		}
	}
}

/**
 * @brief Rebuild the entire spatial grid from a provided array of particle positions.
 * @param Positions Array of world-space particle positions.
 */
void FKawaiiFluidSpatialHash::BuildFromPositions(const TArray<FVector>& Positions)
{
	Clear();

	// Cache position array (for distance filtering in GetNeighbors)
	CachedPositions = Positions;

	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		Insert(i, Positions[i]);
	}
}

/**
 * @brief Calculate grid integer coordinates for a world-space position.
 * @param Position The world-space position.
 * @return FIntVector containing the grid cell coordinates.
 */
FIntVector FKawaiiFluidSpatialHash::GetCellCoord(const FVector& Position) const
{
	return FIntVector(
		FMath::FloorToInt(Position.X / CellSize),
		FMath::FloorToInt(Position.Y / CellSize),
		FMath::FloorToInt(Position.Z / CellSize)
	);
}
