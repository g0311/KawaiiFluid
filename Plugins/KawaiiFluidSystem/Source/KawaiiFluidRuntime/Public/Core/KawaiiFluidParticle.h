// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KawaiiFluidParticle.generated.h"

/**
 * @struct FKawaiiFluidParticle
 * @brief Fluid particle structure representing the base unit of PBF (Position-Based Fluids) simulation.
 * 
 * @param Position Current world-space position of the particle.
 * @param PredictedPosition Predicted position used by the solver.
 * @param Velocity Current velocity of the particle.
 * @param Mass Mass of the particle.
 * @param Density Calculated density of the particle.
 * @param Lambda Lagrange multiplier for density constraint.
 * @param bIsAttached Whether the particle is currently attached to a surface.
 * @param AttachedActor Weak reference to the actor this particle is attached to.
 * @param AttachedBoneName Name of the bone this particle is attached to.
 * @param AttachedLocalOffset Relative position in bone local coordinates.
 * @param AttachedSurfaceNormal Surface normal of the attached surface.
 * @param bJustDetached Flag to prevent immediate reattachment after detaching.
 * @param bNearGround Flag indicating proximity to world geometry.
 * @param bNearBoundary Flag indicating proximity to boundary particles.
 * @param ParticleID Unique identifier for the particle.
 * @param NeighborIndices List of neighbor particle indices found in the spatial hash.
 * @param SourceID Combined identifier for preset and component source.
 * @param bIsSurfaceParticle Flag for rendering optimization.
 * @param SurfaceNormal Normal vector used for surface tension calculation.
 * @param bTrailSpawned Flag for trail VFX spawning.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidParticle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Particle")
	FVector Position;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	FVector PredictedPosition;

	UPROPERTY(BlueprintReadWrite, Category = "Particle")
	FVector Velocity;

	UPROPERTY(BlueprintReadWrite, Category = "Particle")
	float Mass;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	float Density;

	float Lambda;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	bool bIsAttached;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	TWeakObjectPtr<AActor> AttachedActor;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	FName AttachedBoneName;

	FVector AttachedLocalOffset;

	FVector AttachedSurfaceNormal;

	bool bJustDetached;

	bool bNearGround;

	bool bNearBoundary;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	int32 ParticleID;

	TArray<int32> NeighborIndices;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	int32 SourceID;

	UPROPERTY(BlueprintReadOnly, Category = "Particle")
	bool bIsSurfaceParticle;

	FVector SurfaceNormal;

	bool bTrailSpawned;

	FKawaiiFluidParticle()
		: Position(FVector::ZeroVector)
		, PredictedPosition(FVector::ZeroVector)
		, Velocity(FVector::ZeroVector)
		, Mass(1.0f)
		, Density(0.0f)
		, Lambda(0.0f)
		, bIsAttached(false)
		, AttachedBoneName(NAME_None)
		, AttachedLocalOffset(FVector::ZeroVector)
		, AttachedSurfaceNormal(FVector::UpVector)
		, bJustDetached(false)
		, bNearGround(false)
		, bNearBoundary(false)
		, ParticleID(-1)
		, SourceID(-1)
		, bIsSurfaceParticle(false)
		, SurfaceNormal(FVector::ZeroVector)
		, bTrailSpawned(false)
	{
	}

	FKawaiiFluidParticle(const FVector& InPosition, int32 InID)
		: Position(InPosition)
		, PredictedPosition(InPosition)
		, Velocity(FVector::ZeroVector)
		, Mass(1.0f)
		, Density(0.0f)
		, Lambda(0.0f)
		, bIsAttached(false)
		, AttachedBoneName(NAME_None)
		, AttachedLocalOffset(FVector::ZeroVector)
		, AttachedSurfaceNormal(FVector::UpVector)
		, bJustDetached(false)
		, bNearGround(false)
		, bNearBoundary(false)
		, ParticleID(InID)
		, SourceID(-1)
		, bIsSurfaceParticle(false)
		, SurfaceNormal(FVector::ZeroVector)
		, bTrailSpawned(false)
	{
	}
};
