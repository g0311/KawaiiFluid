// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @struct FGPUBoneDeltaAttachment
 * @brief Per-particle attachment data for following WorldBoundaryParticles.
 * 
 * Uses BoundaryParticleIndex (original index before Z-Order sorting) for stable attachment.
 * 
 * @param BoundaryParticleIndex Index into WorldBoundaryParticles buffer (-1 = not attached).
 * @param Padding1 Alignment padding.
 * @param LocalNormal Surface normal in world space (for anisotropy).
 * @param Padding2 Alignment padding.
 * @param PreviousPosition Previous frame position (for detach check).
 * @param Padding3 Alignment padding.
 * @param LocalOffset Offset from boundary position (physics drift).
 * @param Padding4 Alignment padding.
 * @param Padding5 Alignment padding.
 * @param Padding6 Alignment padding.
 */
struct FGPUBoneDeltaAttachment
{
	int32 BoundaryParticleIndex;
	float Padding1;

	FVector3f LocalNormal;
	float Padding2;

	FVector3f PreviousPosition;
	float Padding3;

	FVector3f LocalOffset;
	float Padding4;

	float Padding5;
	float Padding6;

	FGPUBoneDeltaAttachment()
		: BoundaryParticleIndex(-1)
		, Padding1(0.0f)
		, LocalNormal(FVector3f::ZeroVector)
		, Padding2(0.0f)
		, PreviousPosition(FVector3f::ZeroVector)
		, Padding3(0.0f)
		, LocalOffset(FVector3f::ZeroVector)
		, Padding4(0.0f)
		, Padding5(0.0f)
		, Padding6(0.0f)
	{
	}

	bool IsAttached() const
	{
		return BoundaryParticleIndex >= 0;
	}

	void Clear()
	{
		BoundaryParticleIndex = -1;
		LocalNormal = FVector3f::ZeroVector;
		LocalOffset = FVector3f::ZeroVector;
	}
};

// Compile-time size validation
static_assert(sizeof(FGPUBoneDeltaAttachment) == 64, "FGPUBoneDeltaAttachment must be 64 bytes");
static_assert(alignof(FGPUBoneDeltaAttachment) <= 16, "FGPUBoneDeltaAttachment alignment must not exceed 16 bytes");

/**
 * Boundary Attachment Constants
 *
 * NOTE: DetachDistance is now calculated dynamically as SmoothingRadius * 3.0f
 * in GPUFluidSimulator_SimPasses.cpp (UpdateBoneDeltaAttachmentPass)
 */
namespace EBoundaryAttachment
{
	// DetachDistance is no longer used - now dynamic (SmoothingRadius * 3.0f)
	// constexpr float DetachDistance = 100000.0f;  // DEPRECATED
	constexpr int32 InvalidBoneIndex = -1;
}
