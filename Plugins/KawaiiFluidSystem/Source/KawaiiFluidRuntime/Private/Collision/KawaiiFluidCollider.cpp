// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Collision/KawaiiFluidCollider.h"
#include "Async/ParallelFor.h"

/**
 * @brief Default constructor for UKawaiiFluidCollider.
 * Sets default values for friction and restitution.
 */
UKawaiiFluidCollider::UKawaiiFluidCollider()
{
	PrimaryComponentTick.bCanEverTick = false;

	bColliderEnabled = true;
	Friction = 0.3f;
	Restitution = 0.2f;
}

/**
 * @brief Called when the game starts or when spawned.
 */
void UKawaiiFluidCollider::BeginPlay()
{
	Super::BeginPlay();
}

/**
 * @brief Resolves collisions for an entire array of particles in parallel.
 * @param Particles Array of fluid particles to process
 * @param SubstepDT Delta time for the current simulation substep
 */
void UKawaiiFluidCollider::ResolveCollisions(TArray<FKawaiiFluidParticle>& Particles, float SubstepDT)
{
	if (!bColliderEnabled)
	{
		return;
	}

	ParallelFor(Particles.Num(), [&](int32 i)
	{
		ResolveParticleCollision(Particles[i], SubstepDT);
	});
}

/**
 * @brief Finds the closest point on the collider surface.
 * @param Point Query point in world space
 * @param OutClosestPoint Closest point on the surface
 * @param OutNormal Surface normal at the closest point
 * @param OutDistance Distance to the closest point
 * @return True if a closest point was found
 */
bool UKawaiiFluidCollider::GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const
{
	return false;
}

/**
 * @brief Calculates the signed distance to the collider surface.
 * @param Point Query point in world space
 * @param OutGradient Surface normal pointing outward
 * @return Signed distance (negative if inside, positive if outside)
 */
float UKawaiiFluidCollider::GetSignedDistance(const FVector& Point, FVector& OutGradient) const
{
	// Default implementation using GetClosestPoint
	FVector ClosestPoint;
	FVector Normal;
	float Distance;

	if (!GetClosestPoint(Point, ClosestPoint, Normal, Distance))
	{
		OutGradient = FVector::UpVector;
		return MAX_FLT;
	}

	OutGradient = Normal;

	// Check if inside (IsPointInside) to return negative distance
	if (IsPointInside(Point))
	{
		return -Distance;
	}

	return Distance;
}

/**
 * @brief Finds the closest point including bone information for skeletal colliders.
 * @param Point Query point in world space
 * @param OutClosestPoint Closest point on the surface
 * @param OutNormal Surface normal at the closest point
 * @param OutDistance Distance to the closest point
 * @param OutBoneName Name of the closest bone
 * @param OutBoneTransform Transform of the closest bone
 * @return True if a closest point was found
 */
bool UKawaiiFluidCollider::GetClosestPointWithBone(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance, FName& OutBoneName, FTransform& OutBoneTransform) const
{
	// Default implementation: no bone information
	OutBoneName = NAME_None;
	OutBoneTransform = FTransform::Identity;
	return GetClosestPoint(Point, OutClosestPoint, OutNormal, OutDistance);
}

/**
 * @brief Checks if a point is inside the collider.
 * @param Point Point to check in world space
 * @return True if the point is inside
 */
bool UKawaiiFluidCollider::IsPointInside(const FVector& Point) const
{
	return false;
}

/**
 * @brief Resolves collision for a single particle using SDF.
 * @param Particle Fluid particle to process
 * @param SubstepDT Delta time for the current simulation substep
 */
void UKawaiiFluidCollider::ResolveParticleCollision(FKawaiiFluidParticle& Particle, float SubstepDT)
{
	// Use SDF-based collision
	FVector Gradient;
	float SignedDistance = GetSignedDistance(Particle.PredictedPosition, Gradient);

	// Collision margin (particle radius + safety margin)
	const float CollisionMargin = 5.0f;  // 5cm

	// Collision detected if inside or within margin
	if (SignedDistance < CollisionMargin)
	{
		// Push particle to surface + margin
		float Penetration = CollisionMargin - SignedDistance;
		FVector CollisionPos = Particle.PredictedPosition + Gradient * Penetration;

		// Only modify PredictedPosition
		Particle.PredictedPosition = CollisionPos;

		// Calculate desired velocity after collision response
		// Initialize to zero - particle stops on surface by default
		FVector DesiredVelocity = FVector::ZeroVector;
		float VelDotNormal = FVector::DotProduct(Particle.Velocity, Gradient);

		// Minimum velocity threshold for applying restitution bounce
		// Prevents "popcorn" oscillation for particles resting on surfaces
		const float MinBounceVelocity = 50.0f;  // cm/s

		if (VelDotNormal < 0.0f)
		{
			// Particle moving INTO surface - apply collision response
			FVector VelNormal = Gradient * VelDotNormal;
			FVector VelTangent = Particle.Velocity - VelNormal;

			if (VelDotNormal < -MinBounceVelocity)
			{
				// Significant impact - apply full collision response
				// Normal: Restitution (0 = stick, 1 = full bounce)
				// Tangent: Friction (0 = slide, 1 = stop)
				DesiredVelocity = VelTangent * (1.0f - Friction) - VelNormal * Restitution;
			}
			else
			{
				// Low velocity contact (resting on surface) - no bounce, just slide
				DesiredVelocity = VelTangent * (1.0f - Friction);
			}
		}

		// Back-calculate Position so FinalizePositions derives DesiredVelocity
		// FinalizePositions: Velocity = (PredictedPosition - Position) / dt
		// Therefore: Position = PredictedPosition - DesiredVelocity * dt
		Particle.Position = Particle.PredictedPosition - DesiredVelocity * SubstepDT;
	}
}