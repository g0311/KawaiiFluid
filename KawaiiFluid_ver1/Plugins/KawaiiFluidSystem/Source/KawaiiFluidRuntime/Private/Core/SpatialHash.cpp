// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Core/SpatialHash.h"

FSpatialHash::FSpatialHash()
	: CellSize(1.0f)
{
}

FSpatialHash::FSpatialHash(float InCellSize)
	: CellSize(InCellSize)
{
}

void FSpatialHash::Clear()
{
	Grid.Empty();
}

void FSpatialHash::SetCellSize(float NewCellSize)
{
	CellSize = FMath::Max(NewCellSize, 0.01f);
}

void FSpatialHash::Insert(int32 ParticleIndex, const FVector& Position)
{
	FIntVector CellCoord = GetCellCoord(Position);
	Grid.FindOrAdd(CellCoord).Add(ParticleIndex);
}

void FSpatialHash::GetNeighbors(const FVector& Position, float Radius, TArray<int32>& OutNeighbors) const
{
	OutNeighbors.Reset();

	// 검색할 셀 범위 계산
	int32 CellRadius = FMath::CeilToInt(Radius / CellSize);
	FIntVector CenterCell = GetCellCoord(Position);

	// 주변 셀들 순회
	for (int32 x = -CellRadius; x <= CellRadius; ++x)
	{
		for (int32 y = -CellRadius; y <= CellRadius; ++y)
		{
			for (int32 z = -CellRadius; z <= CellRadius; ++z)
			{
				FIntVector CellCoord = CenterCell + FIntVector(x, y, z);

				if (const TArray<int32>* CellParticles = Grid.Find(CellCoord))
				{
					OutNeighbors.Append(*CellParticles);
				}
			}
		}
	}
}

void FSpatialHash::BuildFromPositions(const TArray<FVector>& Positions)
{
	Clear();

	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		Insert(i, Positions[i]);
	}
}

FIntVector FSpatialHash::GetCellCoord(const FVector& Position) const
{
	return FIntVector(
		FMath::FloorToInt(Position.X / CellSize),
		FMath::FloorToInt(Position.Y / CellSize),
		FMath::FloorToInt(Position.Z / CellSize)
	);
}

uint32 FSpatialHash::HashCoord(const FIntVector& Coord) const
{
	// 간단한 해시 함수
	const uint32 p1 = 73856093;
	const uint32 p2 = 19349663;
	const uint32 p3 = 83492791;

	return (Coord.X * p1) ^ (Coord.Y * p2) ^ (Coord.Z * p3);
}
