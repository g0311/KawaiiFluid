// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * GPU Bone Delta Attachment Structure
 *
 * Per-particle attachment data for following WorldBoundaryParticles.
 * Uses BoundaryParticleIndex (original index before Z-Order sorting) for stable attachment.
 *
 * This structure mirrors the HLSL struct in FluidBoneDeltaAttachment.ush
 *
 * Used by:
 * - FluidApplyBoneTransform.usf: Read WorldBoundaryParticles[BoundaryParticleIndex].Position
 * - FluidUpdateBoneDeltaAttachment.usf: Find nearest boundary and store OriginalIndex
 *
 * Detach condition: distance from PreviousPosition > DetachDistance
 */
struct FGPUBoneDeltaAttachment
{
	int32 BoundaryParticleIndex;  // 4 bytes  - Index into WorldBoundaryParticles buffer (-1 = not attached)
	float Padding1;               // 4 bytes  - Alignment padding (total: 8)

	FVector3f Reserved;           // 12 bytes - Reserved for future use (was LocalOffset)
	float Padding2;               // 4 bytes  - Alignment padding (total: 24)

	FVector3f PreviousPosition;   // 12 bytes - Previous frame position (for detach check)
	float Padding3;               // 4 bytes  - Alignment padding (total: 40)

	// Add 8 bytes padding to reach 48 bytes (16-byte aligned)
	float Padding4;               // 4 bytes
	float Padding5;               // 4 bytes  (total: 48)

	FGPUBoneDeltaAttachment()
		: BoundaryParticleIndex(-1)
		, Padding1(0.0f)
		, Reserved(FVector3f::ZeroVector)
		, Padding2(0.0f)
		, PreviousPosition(FVector3f::ZeroVector)
		, Padding3(0.0f)
		, Padding4(0.0f)
		, Padding5(0.0f)
	{
	}

	/** Check if attached to a boundary particle */
	FORCEINLINE bool IsAttached() const
	{
		return BoundaryParticleIndex >= 0;
	}

	/** Clear attachment */
	FORCEINLINE void Clear()
	{
		BoundaryParticleIndex = -1;
		Reserved = FVector3f::ZeroVector;
	}
};

// Compile-time size validation
static_assert(sizeof(FGPUBoneDeltaAttachment) == 48, "FGPUBoneDeltaAttachment must be 48 bytes");
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
