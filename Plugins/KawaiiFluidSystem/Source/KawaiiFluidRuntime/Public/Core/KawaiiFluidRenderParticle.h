// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @struct FKawaiiFluidRenderParticle
 * @brief Particle structure for rendering, aligned to 32 bytes for GPU compatibility.
 * 
 * @param Position World-space position of the particle.
 * @param Velocity Current velocity of the particle.
 * @param Radius Radius used for simulation and rendering.
 * @param Padding Padding to ensure 16-byte alignment (Total struct size: 32 bytes).
 */
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidRenderParticle
{
public:
	FVector3f Position;

	FVector3f Velocity;

	float Radius;

	float Padding;

	FKawaiiFluidRenderParticle()
		: Position(FVector3f::ZeroVector)
		, Velocity(FVector3f::ZeroVector)
		, Radius(1.0f)
		, Padding(0.0f)
	{
	}
};

// Verify 32-byte size for GPU buffer alignment.
static_assert(sizeof(FKawaiiFluidRenderParticle) == 32, "FKawaiiRenderParticle size is not 32 bytes.");

// Verify offset for Radius to ensure memory layout consistency.
static_assert(STRUCT_OFFSET(FKawaiiFluidRenderParticle, Radius) == 24, "Radius offset is incorrect.");
