// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Core/KawaiiFluidSimulationTypes.h"

namespace GridResolutionPresetHelper
{
	/**
	 * @brief Get the number of bits per axis for Morton code encoding from a preset.
	 * @param Preset The grid resolution preset.
	 * @return Number of bits (6, 7, or 8).
	 */
	int32 GetAxisBits(EGridResolutionPreset Preset)
	{
		switch (Preset)
		{
		case EGridResolutionPreset::Small:  return 6;
		case EGridResolutionPreset::Medium: return 7;
		case EGridResolutionPreset::Large:  return 8;
		default: return 7;
		}
	}

	/**
	 * @brief Get the grid resolution per axis (2^bits).
	 * @param Preset The grid resolution preset.
	 * @return Number of cells per axis.
	 */
	int32 GetGridResolution(EGridResolutionPreset Preset)
	{
		return 1 << GetAxisBits(Preset);
	}

	/**
	 * @brief Get the total maximum number of cells for a preset (resolution^3).
	 * @param Preset The grid resolution preset.
	 * @return Total cell count.
	 */
	int32 GetMaxCells(EGridResolutionPreset Preset)
	{
		const int32 Res = GetGridResolution(Preset);
		return Res * Res * Res;
	}

	/**
	 * @brief Get the display name of a preset.
	 * @param Preset The grid resolution preset.
	 * @return FString representation of the preset name.
	 */
	FString GetDisplayName(EGridResolutionPreset Preset)
	{
		switch (Preset)
		{
		case EGridResolutionPreset::Small:  return TEXT("Small");
		case EGridResolutionPreset::Medium: return TEXT("Medium");
		case EGridResolutionPreset::Large:  return TEXT("Large");
		default: return TEXT("Unknown");
		}
	}

	/**
	 * @brief Get the maximum volume half-extent that a preset can support.
	 * @param Preset The grid resolution preset.
	 * @param CellSize Cell size in cm (typically SmoothingRadius).
	 * @return Maximum half-extent per axis in cm.
	 */
	float GetMaxExtentForPreset(EGridResolutionPreset Preset, float CellSize)
	{
		return static_cast<float>(GetGridResolution(Preset)) * CellSize * 0.5f;
	}

	/**
	 * @brief Automatically select the smallest preset that can contain the given volume extent.
	 * @param VolumeExtent User-defined volume half-extent (per axis).
	 * @param CellSize Cell size in cm (typically SmoothingRadius).
	 * @return Smallest EGridResolutionPreset that fits the extent.
	 */
	EGridResolutionPreset SelectPresetForExtent(const FVector& VolumeExtent, float CellSize)
	{
		const float MaxExtent = FMath::Max3(VolumeExtent.X, VolumeExtent.Y, VolumeExtent.Z);

		if (MaxExtent <= GetMaxExtentForPreset(EGridResolutionPreset::Small, CellSize))
		{
			return EGridResolutionPreset::Small;
		}
		else if (MaxExtent <= GetMaxExtentForPreset(EGridResolutionPreset::Medium, CellSize))
		{
			return EGridResolutionPreset::Medium;
		}
		else
		{
			return EGridResolutionPreset::Large;
		}
	}

	/**
	 * @brief Clamp a volume extent to the maximum supported by the Large preset.
	 * @param VolumeExtent User-defined volume half-extent.
	 * @param CellSize Cell size in cm.
	 * @return Clamped extent vector.
	 */
	FVector ClampExtentToMaxSupported(const FVector& VolumeExtent, float CellSize)
	{
		const float MaxSupported = GetMaxExtentForPreset(EGridResolutionPreset::Large, CellSize);
		return FVector(
			FMath::Min(VolumeExtent.X, MaxSupported),
			FMath::Min(VolumeExtent.Y, MaxSupported),
			FMath::Min(VolumeExtent.Z, MaxSupported)
		);
	}
}
