// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Core/KawaiiSlimeSimulationContext.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "Core/SpatialHash.h"

UKawaiiSlimeSimulationContext::UKawaiiSlimeSimulationContext()
{
}

//========================================
// Override: SimulateSubstep
//========================================

void UKawaiiSlimeSimulationContext::SimulateSubstep(
	TArray<FFluidParticle>& Particles,
	const UKawaiiFluidPresetDataAsset* Preset,
	const FKawaiiFluidSimulationParams& Params,
	FSpatialHash& SpatialHash,
	float SubstepDT)
{
	// 1. Predict positions (gravity, external forces)
	PredictPositions(Particles, Preset, Params.ExternalForce, SubstepDT);

	// 2. Update neighbors
	UpdateNeighbors(Particles, SpatialHash, Preset->SmoothingRadius);

	// 2.5. Update surface particles (needed before density constraints)
	UpdateSurfaceParticles(Particles, Params);

	// 3. Solve density constraints (push/pull for proper density)
	SolveDensityConstraints(Particles, Preset, SubstepDT);

	// 4. Apply shape matching (slime form preservation)
	if (Params.bEnableShapeMatching)
	{
		ApplyShapeMatchingConstraint(Particles, Params);
	}

	// 5. Apply nucleus attraction (cohesion - slime specific)
	//ApplyNucleusAttraction(Particles, Params, SubstepDT);

	// 6. Relax surface particles for uniform distribution (improves SSFR rendering)
	RelaxSurfaceParticles(Particles, Preset, SubstepDT);
	//ApplySurfaceTension(Particles, Preset, Params);

	// 7. Handle collisions with registered colliders
	HandleCollisions(Particles, Params.Colliders);

	// 8. World collision
	if (Params.bUseWorldCollision && Params.World)
	{
		HandleWorldCollision(Particles, Params, SpatialHash, Params.ParticleRadius);
	}

	// 9. Finalize positions and update velocities
	FinalizePositions(Particles, SubstepDT);

	// 10. Apply viscosity
	ApplyViscosity(Particles, Preset);

	// 11. Apply adhesion
	ApplyAdhesion(Particles, Preset, Params.Colliders);
}

//========================================
// Slime-specific Physics
//========================================

void UKawaiiSlimeSimulationContext::ApplyNucleusAttraction(
	TArray<FFluidParticle>& Particles,
	const FKawaiiFluidSimulationParams& Params,
	float DeltaTime)
{
	if (Particles.Num() == 0)
	{
		return;
	}

	// TODO: Get these from Params or Preset (currently using defaults)
	const float NucleusAttractionStrength = 15.0f;
	const float AttractionFalloff = 0.3f;
	const int32 MainClusterID = 0;

	// Calculate cluster center
	FVector Center = CalculateClusterCenter(Particles, MainClusterID);
	if (Center.IsZero())
	{
		return;
	}

	// Calculate max distance for falloff
	float MaxDistance = CalculateMaxDistanceFromCenter(Particles, Center, MainClusterID);
	if (MaxDistance < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Apply attraction force to each particle
	for (FFluidParticle& P : Particles)
	{
		if (P.ClusterID != MainClusterID)
		{
			continue;
		}

		FVector ToCenter = Center - P.PredictedPosition;
		float DistFromCenter = ToCenter.Size();

		if (DistFromCenter < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FVector Direction = ToCenter / DistFromCenter;

		// Compute falloff based on distance from center
		float DistanceRatio = FMath::Clamp(DistFromCenter / MaxDistance, 0.0f, 1.0f);
		float Falloff = 1.0f - (DistanceRatio * AttractionFalloff);

		// Apply attraction force
		float ForceMagnitude = NucleusAttractionStrength * Falloff;
		P.PredictedPosition += Direction * ForceMagnitude * DeltaTime;
	}
}

void UKawaiiSlimeSimulationContext::UpdateSurfaceParticles(
	TArray<FFluidParticle>& Particles,
	const FKawaiiFluidSimulationParams& Params)
{
	if (Particles.Num() == 0)
	{
		return;
	}

	// Neighbor count based surface detection
	// Surface particles have fewer neighbors (exposed to outside)
	// Interior particles have many neighbors (surrounded on all sides)
	const int32 NeighborCountThreshold = Params.SurfaceNeighborThreshold;

	for (FFluidParticle& P : Particles)
	{
		int32 NeighborCount = P.NeighborIndices.Num();

		if (NeighborCount < NeighborCountThreshold)
		{
			P.bIsSurfaceParticle = true;

			// Compute surface normal (pointing away from neighbors)
			FVector Normal = FVector::ZeroVector;
			for (int32 NeighborIdx : P.NeighborIndices)
			{
				if (NeighborIdx >= 0 && NeighborIdx < Particles.Num())
				{
					FVector ToNeighbor = Particles[NeighborIdx].PredictedPosition - P.PredictedPosition;
					Normal -= ToNeighbor.GetSafeNormal();
				}
			}
			P.SurfaceNormal = Normal.GetSafeNormal();
		}
		else
		{
			P.bIsSurfaceParticle = false;
			P.SurfaceNormal = FVector::ZeroVector;
		}
	}
}

void UKawaiiSlimeSimulationContext::ApplySurfaceTension(
	TArray<FFluidParticle>& Particles,
	const UKawaiiFluidPresetDataAsset* Preset,
	const FKawaiiFluidSimulationParams& Params)
{
	// TODO: Get surface tension coefficient from Params or Preset
	const float SurfaceTensionCoefficient = 0.5f;

	if (SurfaceTensionCoefficient <= 0.0f)
	{
		return;
	}

	for (FFluidParticle& P : Particles)
	{
		if (!P.bIsSurfaceParticle)
		{
			continue;
		}

		// Surface tension pulls surface particles inward
		FVector TensionForce = -P.SurfaceNormal * SurfaceTensionCoefficient;
		P.PredictedPosition += TensionForce;
	}
}

void UKawaiiSlimeSimulationContext::ApplyAntiGravity(
	TArray<FFluidParticle>& Particles,
	const UKawaiiFluidPresetDataAsset* Preset,
	const FKawaiiFluidSimulationParams& Params,
	float DeltaTime)
{
	// TODO: Get anti-gravity strength from Params
	const float AntiGravityStrength = 0.5f;
	const int32 MainClusterID = 0;

	if (AntiGravityStrength <= 0.0f || !Preset)
	{
		return;
	}

	FVector Gravity = Preset->Gravity;
	FVector AntiGravityForce = -Gravity * AntiGravityStrength * DeltaTime;

	for (FFluidParticle& P : Particles)
	{
		if (P.ClusterID != MainClusterID)
		{
			continue;
		}

		P.PredictedPosition += AntiGravityForce;
	}
}

void UKawaiiSlimeSimulationContext::RelaxSurfaceParticles(
	TArray<FFluidParticle>& Particles,
	const UKawaiiFluidPresetDataAsset* Preset,
	float DeltaTime)
{
	if (!Preset || Particles.Num() < 2)
	{
		return;
	}

	const float SmoothingRadius = Preset->SmoothingRadius;
	const float IdealSpacing = SmoothingRadius * 0.8f;  // Target spacing between particles
	const float RelaxStrength = 0.3f;  // How strongly to push/pull (0-1)

	// Collect displacement for each particle (to avoid order-dependent artifacts)
	TArray<FVector> Displacements;
	Displacements.SetNumZeroed(Particles.Num());

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		FFluidParticle& P = Particles[i];

		// Only process surface particles (set by UpdateSurfaceParticles)
		if (!P.bIsSurfaceParticle)
		{
			continue;
		}

		FVector TotalDisplacement = FVector::ZeroVector;
		int32 NeighborCount = 0;

		// Check neighbors and compute relaxation displacement
		for (int32 NeighborIdx : P.NeighborIndices)
		{
			if (NeighborIdx < 0 || NeighborIdx >= Particles.Num() || NeighborIdx == i)
			{
				continue;
			}

			const FFluidParticle& Neighbor = Particles[NeighborIdx];

			// Only consider neighbors that are also on the surface
			if (!Neighbor.bIsSurfaceParticle)
			{
				continue;
			}

			FVector ToNeighbor = Neighbor.PredictedPosition - P.PredictedPosition;
			float Dist = ToNeighbor.Size();

			if (Dist < KINDA_SMALL_NUMBER || Dist > SmoothingRadius)
			{
				continue;
			}

			FVector Direction = ToNeighbor / Dist;

			// If too close, push apart; if too far, pull together
			float DistError = Dist - IdealSpacing;

			// Apply displacement proportional to error
			// Positive error = too far apart = pull together (move toward neighbor)
			// Negative error = too close = push apart (move away from neighbor)
			TotalDisplacement += Direction * DistError * RelaxStrength;
			NeighborCount++;
		}

		// Average the displacement
		if (NeighborCount > 0)
		{
			Displacements[i] = TotalDisplacement / NeighborCount;
		}
	}

	// Apply displacements (no re-projection - works with any shape)
	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		if (!Displacements[i].IsNearlyZero())
		{
			Particles[i].PredictedPosition += Displacements[i] * DeltaTime;
		}
	}
}

void UKawaiiSlimeSimulationContext::SolveDensityConstraints(
	TArray<FFluidParticle>& Particles,
	const UKawaiiFluidPresetDataAsset* Preset,
	float DeltaTime)
{
	// 1. Save surface particle positions (they will be restored after density solve)
	TArray<FVector> SurfacePositions;
	TArray<int32> SurfaceIndices;

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		if (Particles[i].bIsSurfaceParticle)
		{
			SurfaceIndices.Add(i);
			SurfacePositions.Add(Particles[i].PredictedPosition);
		}
	}

	// 2. Call base class density constraint (applies to all particles)
	Super::SolveDensityConstraints(Particles, Preset, DeltaTime);

	// 3. Restore surface particle positions (exclude from density constraint effect)
	for (int32 i = 0; i < SurfaceIndices.Num(); ++i)
	{
		Particles[SurfaceIndices[i]].PredictedPosition = SurfacePositions[i];
	}
}

//========================================
// Helper Functions
//========================================

FVector UKawaiiSlimeSimulationContext::CalculateClusterCenter(
	const TArray<FFluidParticle>& Particles,
	int32 ClusterID) const
{
	FVector Center = FVector::ZeroVector;
	float TotalMass = 0.0f;

	for (const FFluidParticle& P : Particles)
	{
		if (P.ClusterID == ClusterID)
		{
			Center += P.PredictedPosition * P.Mass;
			TotalMass += P.Mass;
		}
	}

	if (TotalMass > KINDA_SMALL_NUMBER)
	{
		Center /= TotalMass;
	}

	return Center;
}

float UKawaiiSlimeSimulationContext::CalculateMaxDistanceFromCenter(
	const TArray<FFluidParticle>& Particles,
	const FVector& Center,
	int32 ClusterID) const
{
	float MaxDist = 0.0f;

	for (const FFluidParticle& P : Particles)
	{
		if (P.ClusterID == ClusterID)
		{
			float Dist = FVector::Dist(P.PredictedPosition, Center);
			MaxDist = FMath::Max(MaxDist, Dist);
		}
	}

	return MaxDist;
}
