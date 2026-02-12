// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

// TODO(KHJ): 이거 Core인지 아니면 여기 파라미터에 둬도 되는지 확인할 것

/**
 * @struct FGPUFluidParticle
 * @brief GPU Fluid Particle structure mirroring the HLSL struct.
 * 
 * @param Position Current position in world space.
 * @param Mass Particle mass.
 * @param PredictedPosition XPBD predicted position for the current substep.
 * @param Density Current fluid density at particle position.
 * @param Velocity Current velocity vector.
 * @param Lambda Lagrange multiplier for density constraint.
 * @param ParticleID Unique persistent particle ID.
 * @param SourceID ID of the source component that spawned this particle.
 * @param Flags Bitfield flags for particle state (see EGPUParticleFlags).
 * @param NeighborCount Number of nearby particles (for stats and surface detection).
 */
struct FGPUFluidParticle
{
	FVector3f Position;
	float Mass;

	FVector3f PredictedPosition;
	float Density;

	FVector3f Velocity;
	float Lambda;

	int32 ParticleID;
	int32 SourceID;
	uint32 Flags;
	uint32 NeighborCount;

	FGPUFluidParticle()
		: Position(FVector3f::ZeroVector)
		, Mass(1.0f)
		, PredictedPosition(FVector3f::ZeroVector)
		, Density(0.0f)
		, Velocity(FVector3f::ZeroVector)
		, Lambda(0.0f)
		, ParticleID(0)
		, SourceID(-1)
		, Flags(0)
		, NeighborCount(0)
	{
	}
};

// Compile-time size validation
static_assert(sizeof(FGPUFluidParticle) == 64, "FGPUFluidParticle must be 64 bytes");
static_assert(alignof(FGPUFluidParticle) <= 16, "FGPUFluidParticle alignment must not exceed 16 bytes");

/**
 * GPU Particle Flags (stored in FGPUFluidParticle::Flags)
 */
namespace EGPUParticleFlags
{
	constexpr uint32 None = 0;
	constexpr uint32 IsAttached = 1 << 0;        // Particle is attached to a surface (skips physics)
	constexpr uint32 IsSurface = 1 << 1;         // Particle is on the fluid surface
	constexpr uint32 JustDetached = 1 << 2;      // Particle just detached this frame
	constexpr uint32 NearGround = 1 << 3;        // Particle is near the ground
	constexpr uint32 HasCollided = 1 << 4;       // Particle collided this frame
	constexpr uint32 IsSleeping = 1 << 5;        // Particle is in sleep state (low velocity)
	constexpr uint32 NearBoundary = 1 << 6;      // Particle is near boundary (for visualization, doesn't skip physics)
}

/**
 * GPU Particle Source Identification
 * SourceID = Component's unique ID (int32), -1 = invalid
 */
namespace EGPUParticleSource
{
	constexpr int32 InvalidSourceID = -1;
	constexpr int32 MaxSourceCount = 64;  // Maximum number of unique sources (components) for GPU counter tracking
}

/** Check if SourceID is valid */
FORCEINLINE bool HasValidSource(int32 SourceID)
{
	return SourceID != EGPUParticleSource::InvalidSourceID;
}

/**
 * @struct FGPUDespawnBrushRequest
 * @brief Defines a spherical region for particle removal.
 * 
 * @param Center World position of brush center.
 * @param RadiusSq Squared radius pre-computed on CPU.
 */
struct FGPUDespawnBrushRequest
{
	FVector3f Center;
	float RadiusSq;

	FGPUDespawnBrushRequest() : Center(FVector3f::ZeroVector), RadiusSq(0.0f) {}
	FGPUDespawnBrushRequest(const FVector3f& InCenter, float InRadius)
		: Center(InCenter), RadiusSq(InRadius * InRadius) {}
};

static_assert(sizeof(FGPUDespawnBrushRequest) == 16, "FGPUDespawnBrushRequest must be 16 bytes");

/**
 * @struct FGPUFluidSimulationParams
 * @brief GPU Fluid Simulation Parameters passed to compute shaders.
 * 
 * @param RestDensity Target rest density (kg/m³).
 * @param SmoothingRadius SPH smoothing radius (cm).
 * @param Compliance XPBD compliance (softness).
 * @param ParticleRadius Particle collision radius (cm).
 * @param Gravity Gravity vector (cm/s²).
 * @param ViscosityCoefficient XSPH viscosity coefficient (0-1).
 * @param CohesionStrength Legacy force-based cohesion (unused).
 * @param GlobalDamping Velocity damping per substep (1.0 = no damping).
 * @param Poly6Coeff Precomputed Poly6 kernel coefficient.
 * @param SpikyCoeff Precomputed Spiky kernel coefficient.
 * @param Poly6GradCoeff Precomputed Poly6 gradient coefficient.
 * @param SpikyGradCoeff Precomputed Spiky gradient coefficient.
 * @param CellSize Spatial hash cell size (typically = SmoothingRadius).
 * @param ParticleCount Number of active particles.
 * @param ParticleMass Uniform particle mass (kg).
 * @param DeltaTime Simulation substep delta time.
 * @param DeltaTimeSq Precomputed DeltaTime squared.
 * @param BoundsCenter OBB center in world space.
 * @param BoundsRestitution Collision restitution (bounciness).
 * @param BoundsExtent OBB half-extents (local space).
 * @param BoundsFriction Collision friction.
 * @param BoundsRotation OBB rotation quaternion (x, y, z, w).
 * @param BoundsMin World bounds minimum (AABB mode).
 * @param Padding1 Alignment padding.
 * @param BoundsMax World bounds maximum (AABB mode).
 * @param bUseOBB 1 = OBB mode, 0 = AABB mode.
 * @param SubstepIndex Current substep index.
 * @param TotalSubsteps Total substeps per frame.
 * @param SolverIterations Number of XPBD constraint solver iterations.
 * @param CurrentTime Current game time (seconds).
 * @param bEnableTensileInstability 1 if Artificial Pressure is enabled.
 * @param TensileK Scaled strength k for tensile stability.
 * @param TensileN Exponent n for tensile stability.
 * @param TensileDeltaQ Precomputed Δq ratio.
 * @param InvW_DeltaQ Precomputed 1/W(Δq, h).
 * @param StackPressureScale Strength of weight transfer from stacked particles.
 * @param bEnableRelativeVelocityDamping Enable relative velocity pressure damping.
 * @param RelativeVelocityDampingStrength Factor for pressure reduction near boundaries.
 * @param bEnableBoundaryVelocityTransfer Enable boundary velocity transfer.
 * @param BoundaryVelocityTransferStrength Factor for following boundary movement.
 * @param BoundaryDetachSpeedThreshold Speed where detachment begins.
 * @param BoundaryMaxDetachSpeed Speed for full detachment.
 * @param BoundaryAdhesionStrength Adhesion strength for velocity transfer.
 * @param bEnableParticleSleeping Enable particle sleeping for stability.
 * @param SleepVelocityThreshold Speed below which particles may sleep.
 * @param SleepFrameThreshold Frames required to enter sleep state.
 * @param WakeVelocityThreshold Speed required to wake up.
 * @param bEnableBoundaryAttachment Enable boundary attachment system.
 * @param BoundaryAttachRadius Distance threshold for attachment.
 * @param BoundaryDetachDistanceMultiplier Multiplier for detachment distance.
 * @param BoundaryAttachDetachSpeedThreshold Speed threshold for detachment.
 * @param BoundaryAttachCooldown Cooldown after detachment.
 * @param BoundaryAttachConstraintBlend Position constraint strength.
 * @param bEnablePositionBasedSurfaceTension Enable surface tension.
 * @param SurfaceTensionStrength Intensity of surface tension effect.
 * @param SurfaceTensionActivationRatio Distance ratio where ST activates.
 * @param SurfaceTensionFalloffRatio Distance ratio where ST starts fading.
 * @param SurfaceTensionSurfaceThreshold Stronger ST for surface particles.
 * @param SurfaceTensionVelocityDamping Reduces velocity from ST correction.
 * @param SurfaceTensionTolerance Dead zone around activation distance.
 * @param MaxSurfaceTensionCorrectionPerIteration Limit for position correction.
 * @param bSkipBoundsCollision Skip volume bounds collision.
 */
struct FGPUFluidSimulationParams
{
	float RestDensity;
	float SmoothingRadius;
	float Compliance;
	float ParticleRadius;

	FVector3f Gravity;
	float ViscosityCoefficient;
	float CohesionStrength;
	float GlobalDamping;

	float Poly6Coeff;
	float SpikyCoeff;
	float Poly6GradCoeff;
	float SpikyGradCoeff;

	float CellSize;
	int32 ParticleCount;

	float ParticleMass;

	float DeltaTime;
	float DeltaTimeSq;

	FVector3f BoundsCenter;
	float BoundsRestitution;
	FVector3f BoundsExtent;
	float BoundsFriction;
	FVector4f BoundsRotation;

	FVector3f BoundsMin;
	float Padding1;
	FVector3f BoundsMax;
	int32 bUseOBB;

	int32 SubstepIndex;
	int32 TotalSubsteps;
	int32 SolverIterations;
	float CurrentTime;

	int32 bEnableTensileInstability;
	float TensileK;
	int32 TensileN;
	float TensileDeltaQ;
	float InvW_DeltaQ;

	float StackPressureScale;

	int32 bEnableRelativeVelocityDamping;
	float RelativeVelocityDampingStrength;
	int32 bEnableBoundaryVelocityTransfer;
	float BoundaryVelocityTransferStrength;
	float BoundaryDetachSpeedThreshold;
	float BoundaryMaxDetachSpeed;
	float BoundaryAdhesionStrength;

	int32 bEnableParticleSleeping;
	float SleepVelocityThreshold;
	int32 SleepFrameThreshold;
	float WakeVelocityThreshold;

	int32 bEnableBoundaryAttachment;
	float BoundaryAttachRadius;
	float BoundaryDetachDistanceMultiplier;
	float BoundaryAttachDetachSpeedThreshold;
	float BoundaryAttachCooldown;
	float BoundaryAttachConstraintBlend;

	int32 bEnablePositionBasedSurfaceTension;
	float SurfaceTensionStrength;
	float SurfaceTensionActivationRatio;
	float SurfaceTensionFalloffRatio;
	int32 SurfaceTensionSurfaceThreshold;
	float SurfaceTensionVelocityDamping;
	float SurfaceTensionTolerance;

	float MaxSurfaceTensionCorrectionPerIteration;

	int32 bSkipBoundsCollision;

	FGPUFluidSimulationParams()
		: RestDensity(1000.0f)
		, SmoothingRadius(20.0f)
		, Compliance(0.01f)
		, ParticleRadius(5.0f)
		, Gravity(FVector3f(0.0f, 0.0f, -980.0f))
		, ViscosityCoefficient(0.01f)
		, CohesionStrength(0.0f)
		, GlobalDamping(1.0f)
		, Poly6Coeff(0.0f)
		, SpikyCoeff(0.0f)
		, Poly6GradCoeff(0.0f)
		, SpikyGradCoeff(0.0f)
		, CellSize(20.0f)
		, ParticleCount(0)
		, ParticleMass(1.0f)
		, DeltaTime(0.016f)
		, DeltaTimeSq(0.000256f)
		, BoundsCenter(FVector3f::ZeroVector)
		, BoundsRestitution(0.3f)
		, BoundsExtent(FVector3f(1000.0f))
		, BoundsFriction(0.1f)
		, BoundsRotation(FVector4f(0.0f, 0.0f, 0.0f, 1.0f))
		, BoundsMin(FVector3f(-1000.0f))
		, Padding1(0.0f)
		, BoundsMax(FVector3f(1000.0f))
		, bUseOBB(0)
		, SubstepIndex(0)
		, TotalSubsteps(1)
		, SolverIterations(1)
		, CurrentTime(0.0f)
		, bEnableTensileInstability(1)
		, TensileK(10.0f)
		, TensileN(4)
		, TensileDeltaQ(0.0f)
		, InvW_DeltaQ(0.0f)
		, StackPressureScale(0.0f)
		, bEnableRelativeVelocityDamping(1)
		, RelativeVelocityDampingStrength(0.6f)
		, bEnableBoundaryVelocityTransfer(1)
		, BoundaryVelocityTransferStrength(0.8f)
		, BoundaryDetachSpeedThreshold(500.0f)
		, BoundaryMaxDetachSpeed(1500.0f)
		, BoundaryAdhesionStrength(0.5f)
		, bEnableParticleSleeping(0)
		, SleepVelocityThreshold(5.0f)
		, SleepFrameThreshold(30)
		, WakeVelocityThreshold(20.0f)
		, bEnableBoundaryAttachment(1)
		, BoundaryAttachRadius(25.0f)
		, BoundaryDetachDistanceMultiplier(3.0f)
		, BoundaryAttachDetachSpeedThreshold(500.0f)
		, BoundaryAttachCooldown(0.2f)
		, BoundaryAttachConstraintBlend(0.8f)
		, bEnablePositionBasedSurfaceTension(1)
		, SurfaceTensionStrength(0.3f)
		, SurfaceTensionActivationRatio(0.5f)
		, SurfaceTensionFalloffRatio(0.7f)
		, SurfaceTensionSurfaceThreshold(0)
		, SurfaceTensionVelocityDamping(0.7f)
		, SurfaceTensionTolerance(1.0f)
		, MaxSurfaceTensionCorrectionPerIteration(5.0f)
		, bSkipBoundsCollision(0)
	{
	}

	void PrecomputeKernelCoefficients()
	{
		// IMPORTANT: Convert cm to m for kernel calculations to match CPU physics
		// Unreal uses centimeters, but SPH kernels are designed for meters
		constexpr float CmToMeters = 0.01f;
		const float h = SmoothingRadius * CmToMeters;
		const float h2 = h * h;
		const float h3 = h2 * h;
		const float h6 = h3 * h3;
		const float h9 = h6 * h3;

		Poly6Coeff = 315.0f / (64.0f * PI * h9);
		SpikyCoeff = -45.0f / (PI * h6);
		Poly6GradCoeff = -945.0f / (32.0f * PI * h9);
		SpikyGradCoeff = -45.0f / (PI * h6);

		DeltaTimeSq = DeltaTime * DeltaTime;

		if (bEnableTensileInstability && TensileDeltaQ >= 0.0f)
		{
			const float DeltaQ = TensileDeltaQ * h;
			const float DeltaQ2 = DeltaQ * DeltaQ;
			const float Diff = h2 - DeltaQ2;
			if (Diff > 0.0f)
			{
				const float W_DeltaQ = Diff * Diff * Diff;
				InvW_DeltaQ = (W_DeltaQ > 1e-30f) ? (1.0f / W_DeltaQ) : 0.0f;
			}
			else
			{
				InvW_DeltaQ = 0.0f;
			}
		}
		else
		{
			InvW_DeltaQ = 0.0f;
		}
	}
};

/**
 * @struct FGPUHeightmapCollisionParams
 * @brief Parameters for GPU collision detection against Landscape terrain.
 * 
 * @param WorldMin Heightmap coverage minimum world position.
 * @param HeightScale Height scale factor.
 * @param WorldMax Heightmap coverage maximum world position.
 * @param ParticleRadius Radius for collision detection.
 * @param InvWorldExtent Inverse extent for UV transform.
 * @param Friction Friction coefficient.
 * @param Restitution Restitution coefficient.
 * @param TextureWidth Heightmap texture width.
 * @param TextureHeight Heightmap texture height.
 * @param InvTextureWidth Inverse texture width.
 * @param InvTextureHeight Inverse texture height.
 * @param bEnabled Whether heightmap collision is enabled.
 * @param NormalStrength Surface normal scale factor.
 * @param CollisionOffset Extra offset for detection.
 * @param Padding Alignment padding.
 */
struct FGPUHeightmapCollisionParams
{
	FVector3f WorldMin;
	float HeightScale;

	FVector3f WorldMax;
	float ParticleRadius;

	FVector2f InvWorldExtent;
	float Friction;
	float Restitution;

	int32 TextureWidth;
	int32 TextureHeight;
	float InvTextureWidth;
	float InvTextureHeight;

	int32 bEnabled;
	float NormalStrength;
	float CollisionOffset;
	int32 Padding;

	FGPUHeightmapCollisionParams()
		: WorldMin(FVector3f(-10000.0f, -10000.0f, -10000.0f))
		, HeightScale(1.0f)
		, WorldMax(FVector3f(10000.0f, 10000.0f, 10000.0f))
		, ParticleRadius(5.0f)
		, InvWorldExtent(FVector2f(0.00005f, 0.00005f))
		, Friction(0.3f)
		, Restitution(0.1f)
		, TextureWidth(1024)
		, TextureHeight(1024)
		, InvTextureWidth(1.0f / 1024.0f)
		, InvTextureHeight(1.0f / 1024.0f)
		, bEnabled(0)
		, NormalStrength(1.0f)
		, CollisionOffset(0.0f)
		, Padding(0)
	{
	}

	void UpdateInverseValues()
	{
		FVector3f Extent = WorldMax - WorldMin;
		if (Extent.X > SMALL_NUMBER && Extent.Y > SMALL_NUMBER)
		{
			InvWorldExtent = FVector2f(1.0f / Extent.X, 1.0f / Extent.Y);
		}
		if (TextureWidth > 0 && TextureHeight > 0)
		{
			InvTextureWidth = 1.0f / static_cast<float>(TextureWidth);
			InvTextureHeight = 1.0f / static_cast<float>(TextureHeight);
		}
	}
};
static_assert(sizeof(FGPUHeightmapCollisionParams) == 80, "FGPUHeightmapCollisionParams must be 80 bytes");

//=============================================================================
// GPU Collision Primitives
// Uploaded from FluidCollider system for GPU-based collision detection
//=============================================================================

/** Collision primitive types */
namespace EGPUCollisionPrimitiveType
{
	constexpr uint32 Sphere = 0;
	constexpr uint32 Capsule = 1;
	constexpr uint32 Box = 2;
	constexpr uint32 Convex = 3;
}

/**
 * @struct FGPUCollisionSphere
 * @brief GPU Sphere Primitive.
 * 
 * @param Center Center position in world space.
 * @param Radius Sphere radius.
 * @param Friction Friction coefficient.
 * @param Restitution Restitution coefficient.
 * @param BoneIndex Index into bone transform buffer (-1 = no bone).
 * @param FluidTagID Tag hash for event identification (e.g., "Water", "Lava").
 * @param OwnerID ID of collider owner (actor/component).
 * @param bHasFluidInteraction 1 if from FluidInteraction component, 0 if from WorldCollision.
 */
struct FGPUCollisionSphere
{
	FVector3f Center;
	float Radius;
	float Friction;
	float Restitution;
	int32 BoneIndex;
	int32 FluidTagID;
	int32 OwnerID;
	int32 bHasFluidInteraction;

	FGPUCollisionSphere()
		: Center(FVector3f::ZeroVector)
		, Radius(10.0f)
		, Friction(0.1f)
		, Restitution(0.3f)
		, BoneIndex(-1)
		, FluidTagID(0)
		, OwnerID(0)
		, bHasFluidInteraction(0)
	{
	}
};
static_assert(sizeof(FGPUCollisionSphere) == 40, "FGPUCollisionSphere must be 40 bytes");

/**
 * @struct FGPUCollisionCapsule
 * @brief GPU Capsule Primitive.
 * 
 * @param Start Start position of segment.
 * @param Radius Capsule radius.
 * @param End End position of segment.
 * @param Friction Friction coefficient.
 * @param Restitution Restitution coefficient.
 * @param BoneIndex Index into bone transform buffer (-1 = no bone).
 * @param FluidTagID Tag hash for event identification.
 * @param OwnerID ID of collider owner (actor/component).
 * @param bHasFluidInteraction 1 if from FluidInteraction component, 0 if from WorldCollision.
 * @param Padding Alignment padding.
 */
struct FGPUCollisionCapsule
{
	FVector3f Start;
	float Radius;
	FVector3f End;
	float Friction;
	float Restitution;
	int32 BoneIndex;
	int32 FluidTagID;
	int32 OwnerID;
	int32 bHasFluidInteraction;
	int32 Padding;

	FGPUCollisionCapsule()
		: Start(FVector3f::ZeroVector)
		, Radius(10.0f)
		, End(FVector3f(0.0f, 0.0f, 100.0f))
		, Friction(0.1f)
		, Restitution(0.3f)
		, BoneIndex(-1)
		, FluidTagID(0)
		, OwnerID(0)
		, bHasFluidInteraction(0)
		, Padding(0)
	{
	}
};
static_assert(sizeof(FGPUCollisionCapsule) == 56, "FGPUCollisionCapsule must be 56 bytes");

/**
 * @struct FGPUCollisionBox
 * @brief GPU Box Primitive.
 * 
 * @param Center Center position in world space.
 * @param Friction Friction coefficient.
 * @param Extent Local half extents.
 * @param Restitution Restitution coefficient.
 * @param Rotation Rotation quaternion.
 * @param BoneIndex Index into bone transform buffer (-1 = no bone).
 * @param FluidTagID Tag hash for event identification.
 * @param OwnerID ID of collider owner (actor/component).
 * @param bHasFluidInteraction 1 if from FluidInteraction component, 0 if from WorldCollision.
 */
struct FGPUCollisionBox
{
	FVector3f Center;
	float Friction;
	FVector3f Extent;
	float Restitution;
	FVector4f Rotation;
	int32 BoneIndex;
	int32 FluidTagID;
	int32 OwnerID;
	int32 bHasFluidInteraction;

	FGPUCollisionBox()
		: Center(FVector3f::ZeroVector)
		, Friction(0.1f)
		, Extent(FVector3f(50.0f))
		, Restitution(0.3f)
		, Rotation(FVector4f(0.0f, 0.0f, 0.0f, 1.0f))
		, BoneIndex(-1)
		, FluidTagID(0)
		, OwnerID(0)
		, bHasFluidInteraction(0)
	{
	}
};
static_assert(sizeof(FGPUCollisionBox) == 64, "FGPUCollisionBox must be 64 bytes");

/**
 * @struct FGPUConvexPlane
 * @brief GPU Convex Plane for hull representation.
 * 
 * @param Normal Unit normal pointing outward.
 * @param Distance Signed distance from origin.
 */
struct FGPUConvexPlane
{
	FVector3f Normal;
	float Distance;

	FGPUConvexPlane()
		: Normal(FVector3f(0.0f, 0.0f, 1.0f))
		, Distance(0.0f)
	{
	}
};
static_assert(sizeof(FGPUConvexPlane) == 16, "FGPUConvexPlane must be 16 bytes");

/**
 * @struct FGPUCollisionConvex
 * @brief GPU Convex Primitive Header.
 * 
 * @param Center Center position for bounds check.
 * @param BoundingRadius Bounding sphere radius.
 * @param PlaneStartIndex Start index in plane buffer.
 * @param PlaneCount Number of planes in hull.
 * @param Friction Friction coefficient.
 * @param Restitution Restitution coefficient.
 * @param BoneIndex Index into bone transform buffer (-1 = no bone).
 * @param FluidTagID Tag hash for event identification.
 * @param OwnerID ID of collider owner (actor/component).
 * @param bHasFluidInteraction 1 if from FluidInteraction component, 0 if from WorldCollision.
 */
struct FGPUCollisionConvex
{
	FVector3f Center;
	float BoundingRadius;
	int32 PlaneStartIndex;
	int32 PlaneCount;
	float Friction;
	float Restitution;
	int32 BoneIndex;
	int32 FluidTagID;
	int32 OwnerID;
	int32 bHasFluidInteraction;

	FGPUCollisionConvex()
		: Center(FVector3f::ZeroVector)
		, BoundingRadius(100.0f)
		, PlaneStartIndex(0)
		, PlaneCount(0)
		, Friction(0.1f)
		, Restitution(0.3f)
		, BoneIndex(-1)
		, FluidTagID(0)
		, OwnerID(0)
		, bHasFluidInteraction(0)
	{
	}
};
static_assert(sizeof(FGPUCollisionConvex) == 48, "FGPUCollisionConvex must be 48 bytes");

/**
 * @struct FGPUBoneTransform
 * @brief GPU Bone Transform storage for attachment tracking.
 * 
 * @param Position Bone world position.
 * @param Scale Uniform scale factor.
 * @param Rotation World rotation quaternion.
 * @param PreviousPos Previous frame world position.
 * @param Padding1 Alignment padding.
 * @param PreviousRot Previous frame world rotation.
 */
struct FGPUBoneTransform
{
	FVector3f Position;
	float Scale;
	FVector4f Rotation;
	FVector3f PreviousPos;
	float Padding1;
	FVector4f PreviousRot;

	FGPUBoneTransform()
		: Position(FVector3f::ZeroVector)
		, Scale(1.0f)
		, Rotation(FVector4f(0.0f, 0.0f, 0.0f, 1.0f))
		, PreviousPos(FVector3f::ZeroVector)
		, Padding1(0.0f)
		, PreviousRot(FVector4f(0.0f, 0.0f, 0.0f, 1.0f))
	{
	}

	void SetFromTransform(const FTransform& InTransform)
	{
		Position = FVector3f(InTransform.GetLocation());
		Scale = InTransform.GetScale3D().GetMax();
		FQuat Q = InTransform.GetRotation();
		Rotation = FVector4f(Q.X, Q.Y, Q.Z, Q.W);
	}

	void UpdatePrevious()
	{
		PreviousPos = Position;
		PreviousRot = Rotation;
	}
};
static_assert(sizeof(FGPUBoneTransform) == 64, "FGPUBoneTransform must be 64 bytes");

/**
 * @struct FGPUParticleAttachment
 * @brief GPU Particle Attachment data managed as a separate buffer.
 * 
 * @param PrimitiveType Type of primitive (0=Sphere, 1=Capsule, 2=Box, 3=Convex, -1=None).
 * @param PrimitiveIndex Index in primitive buffer.
 * @param BoneIndex Index in bone transform buffer.
 * @param AdhesionStrength Current strength of adhesion.
 * @param LocalOffset Position in bone-local space.
 * @param AttachmentTime Time when attachment occurred.
 * @param RelativeVelocity Velocity relative to the attached bone.
 * @param Padding Alignment padding.
 */
struct FGPUParticleAttachment
{
	int32 PrimitiveType;
	int32 PrimitiveIndex;
	int32 BoneIndex;
	float AdhesionStrength;
	FVector3f LocalOffset;
	float AttachmentTime;
	FVector3f RelativeVelocity;
	float Padding;

	FGPUParticleAttachment()
		: PrimitiveType(-1)
		, PrimitiveIndex(-1)
		, BoneIndex(-1)
		, AdhesionStrength(0.0f)
		, LocalOffset(FVector3f::ZeroVector)
		, AttachmentTime(0.0f)
		, RelativeVelocity(FVector3f::ZeroVector)
		, Padding(0.0f)
	{
	}

	bool IsAttached() const { return PrimitiveType >= 0 && BoneIndex >= 0; }

	void Clear()
	{
		PrimitiveType = -1;
		PrimitiveIndex = -1;
		BoneIndex = -1;
		AdhesionStrength = 0.0f;
		LocalOffset = FVector3f::ZeroVector;
		AttachmentTime = 0.0f;
		RelativeVelocity = FVector3f::ZeroVector;
		Padding = 0.0f;
	}
};
static_assert(sizeof(FGPUParticleAttachment) == 48, "FGPUParticleAttachment must be 48 bytes");

/**
 * @struct FGPUAdhesionParams
 * @brief Parameters for the particle adhesion system.
 * 
 * @param AdhesionStrength Base strength factor.
 * @param AdhesionRadius Max distance for interaction.
 * @param DetachAccelThreshold Acceleration limit for attachment.
 * @param DetachDistanceThreshold Distance limit for attachment.
 * @param ColliderContactOffset Extra offset for distance checks.
 * @param BoneVelocityScale Proportion of bone velocity inherited.
 * @param SlidingFriction Friction during surface sliding.
 * @param CurrentTime Current game time.
 * @param bEnableAdhesion Enable flag.
 * @param GravitySlidingScale Gravity influence on sliding.
 * @param Gravity World gravity vector.
 * @param Padding Alignment padding.
 */
struct FGPUAdhesionParams
{
	float AdhesionStrength;
	float AdhesionRadius;
	float DetachAccelThreshold;
	float DetachDistanceThreshold;
	float ColliderContactOffset;
	float BoneVelocityScale;
	float SlidingFriction;
	float CurrentTime;
	int32 bEnableAdhesion;
	float GravitySlidingScale;
	FVector3f Gravity;
	int32 Padding;

	FGPUAdhesionParams()
		: AdhesionStrength(1.0f)
		, AdhesionRadius(10.0f)
		, DetachAccelThreshold(1000.0f)
		, DetachDistanceThreshold(15.0f)
		, SlidingFriction(0.1f)
		, CurrentTime(0.0f)
		, bEnableAdhesion(0)
		, GravitySlidingScale(1.0f)
		, Gravity(0.0f, 0.0f, -980.0f)
		, Padding(0)
	{
	}
};
static_assert(sizeof(FGPUAdhesionParams) == 56, "FGPUAdhesionParams must be 56 bytes");

/**
 * @struct FGPUCollisionFeedback
 * @brief Records collision information for CPU-side force calculation.
 * 
 * @param ParticleIndex Index of the colliding particle.
 * @param ColliderIndex Index of the collider primitive.
 * @param ColliderType Primitive type.
 * @param Density Local density at collision.
 * @param ImpactNormal Normal at impact point.
 * @param Penetration Depth of penetration.
 * @param ParticleVelocity Velocity at impact.
 * @param ColliderOwnerID ID of the collider owner.
 * @param ParticleSourceID Source ID of the particle.
 * @param ParticleActorID Actor ID of the particle source.
 * @param BoneIndex Index of the attached bone.
 * @param Padding1 Alignment padding.
 * @param ImpactOffset Offset in bone-local space.
 * @param Padding2 Alignment padding.
 * @param ParticlePosition World position at impact.
 * @param Padding3 Alignment padding.
 */
struct FGPUCollisionFeedback
{
	int32 ParticleIndex;
	int32 ColliderIndex;
	int32 ColliderType;
	float Density;

	FVector3f ImpactNormal;
	float Penetration;

	FVector3f ParticleVelocity;
	int32 ColliderOwnerID;

	int32 ParticleSourceID;
	int32 ParticleActorID;
	int32 BoneIndex;
	int32 Padding1;

	FVector3f ImpactOffset;
	int32 Padding2;

	FVector3f ParticlePosition;
	int32 Padding3;

	FGPUCollisionFeedback()
		: ParticleIndex(0)
		, ColliderIndex(0)
		, ColliderType(0)
		, Density(0.0f)
		, ImpactNormal(FVector3f::UpVector)
		, Penetration(0.0f)
		, ParticleVelocity(FVector3f::ZeroVector)
		, ColliderOwnerID(0)
		, ParticleSourceID(EGPUParticleSource::InvalidSourceID)
		, ParticleActorID(0)
		, BoneIndex(-1)
		, Padding1(0)
		, ImpactOffset(FVector3f::ZeroVector)
		, Padding2(0)
		, ParticlePosition(FVector3f::ZeroVector)
		, Padding3(0)
	{
	}

	bool HasValidParticleSource() const { return ::HasValidSource(ParticleSourceID); }

	int32 GetSourceComponentID() const { return ParticleSourceID; }
};
static_assert(sizeof(FGPUCollisionFeedback) == 96, "FGPUCollisionFeedback must be 96 bytes for GPU alignment");

/**
 * @struct FGPUCollisionPrimitives
 * @brief Collection of all collision primitives for GPU upload.
 * 
 * @param Spheres List of sphere primitives.
 * @param Capsules List of capsule primitives.
 * @param Boxes List of box primitives.
 * @param Convexes List of convex headers.
 * @param ConvexPlanes List of plane data for convexes.
 * @param BoneTransforms List of bone world transforms.
 */
struct FGPUCollisionPrimitives
{
	TArray<FGPUCollisionSphere> Spheres;
	TArray<FGPUCollisionCapsule> Capsules;
	TArray<FGPUCollisionBox> Boxes;
	TArray<FGPUCollisionConvex> Convexes;
	TArray<FGPUConvexPlane> ConvexPlanes;
	TArray<FGPUBoneTransform> BoneTransforms;

	void Reset()
	{
		Spheres.Reset();
		Capsules.Reset();
		Boxes.Reset();
		Convexes.Reset();
		ConvexPlanes.Reset();
		BoneTransforms.Reset();
	}

	bool IsEmpty() const
	{
		return Spheres.Num() == 0 && Capsules.Num() == 0 &&
		       Boxes.Num() == 0 && Convexes.Num() == 0;
	}

	int32 GetTotalPrimitiveCount() const
	{
		return Spheres.Num() + Capsules.Num() + Boxes.Num() + Convexes.Num();
	}
};

//=============================================================================
// GPU Particle Spawn System
// CPU sends spawn requests, GPU creates particles via atomic counter
//=============================================================================

/**
 * @struct FGPUSpawnRequest
 * @brief Atomic particle spawn request from CPU to GPU.
 * 
 * @param Position World position for spawn.
 * @param Radius Initial particle radius.
 * @param Velocity Initial velocity vector.
 * @param Mass Particle mass.
 * @param SourceID Source identification.
 * @param ActorID Optional actor ID.
 * @param Reserved1 Reserved for future use.
 * @param Reserved2 Reserved for future use.
 */
struct FGPUSpawnRequest
{
	FVector3f Position;
	float Radius;

	FVector3f Velocity;
	float Mass;

	int32 SourceID;
	int32 ActorID;
	int32 Reserved1;
	int32 Reserved2;

	FGPUSpawnRequest()
		: Position(FVector3f::ZeroVector)
		, Radius(0.0f)
		, Velocity(FVector3f::ZeroVector)
		, Mass(1.0f)
		, SourceID(EGPUParticleSource::InvalidSourceID)
		, ActorID(0)
		, Reserved1(0)
		, Reserved2(0)
	{
	}

	FGPUSpawnRequest(const FVector3f& InPosition, const FVector3f& InVelocity, float InMass = 1.0f)
		: Position(InPosition)
		, Radius(0.0f)
		, Velocity(InVelocity)
		, Mass(InMass)
		, SourceID(EGPUParticleSource::InvalidSourceID)
		, ActorID(0)
		, Reserved1(0)
		, Reserved2(0)
	{
	}

	FGPUSpawnRequest(const FVector3f& InPosition, const FVector3f& InVelocity, int32 InSourceID, float InMass = 1.0f)
		: Position(InPosition)
		, Radius(0.0f)
		, Velocity(InVelocity)
		, Mass(InMass)
		, SourceID(InSourceID)
		, ActorID(0)
		, Reserved1(0)
		, Reserved2(0)
	{
	}
};
static_assert(sizeof(FGPUSpawnRequest) == 48, "FGPUSpawnRequest must be 48 bytes");

/**
 * @struct FGPUSpawnParams
 * @brief Constant buffer for spawn compute shader.
 * 
 * @param SpawnRequestCount Number of requests.
 * @param MaxParticleCount Maximum capacity.
 * @param CurrentParticleCount Pre-spawn count.
 * @param NextParticleID Starting ID for allocation.
 * @param DefaultRadius Radius if not specified.
 * @param DefaultMass Mass if not specified.
 * @param Padding1 Alignment padding.
 * @param Padding2 Alignment padding.
 */
struct FGPUSpawnParams
{
	int32 SpawnRequestCount;
	int32 MaxParticleCount;
	int32 CurrentParticleCount;
	int32 NextParticleID;

	float DefaultRadius;
	float DefaultMass;
	int32 Padding1;
	int32 Padding2;

	FGPUSpawnParams()
		: SpawnRequestCount(0)
		, MaxParticleCount(0)
		, CurrentParticleCount(0)
		, NextParticleID(0)
		, DefaultRadius(5.0f)
		, DefaultMass(1.0f)
		, Padding1(0)
		, Padding2(0)
	{
	}
};

/**
 * @struct FGPUFluidSimulationResources
 * @brief Orchestrates RDG buffers for a simulation frame.
 * 
 * @param ParticleBuffer Active particle data.
 * @param ParticleSRV Read-only particle access.
 * @param ParticleUAV Read-write particle access.
 * @param PositionBuffer Temporary position storage.
 * @param PositionSRV Read-only position access.
 * @param TempBuffer Auxiliary buffer for algorithms.
 * @param TempUAV Read-write auxiliary access.
 * @param ParticleCount Current number of particles.
 * @param CellSize Spatial lookup cell size.
 */
struct FGPUFluidSimulationResources
{
	FRDGBufferRef ParticleBuffer;
	FRDGBufferSRVRef ParticleSRV;
	FRDGBufferUAVRef ParticleUAV;

	FRDGBufferRef PositionBuffer;
	FRDGBufferSRVRef PositionSRV;

	FRDGBufferRef TempBuffer;
	FRDGBufferUAVRef TempUAV;

	int32 ParticleCount;
	float CellSize;

	FGPUFluidSimulationResources()
		: ParticleBuffer(nullptr)
		, ParticleSRV(nullptr)
		, ParticleUAV(nullptr)
		, PositionBuffer(nullptr)
		, PositionSRV(nullptr)
		, TempBuffer(nullptr)
		, TempUAV(nullptr)
		, ParticleCount(0)
		, CellSize(20.0f)
	{
	}

	bool IsValid() const { return ParticleBuffer != nullptr && ParticleCount > 0; }
};

//=============================================================================
// GPU Boundary Particles (Flex-style Adhesion)
// Surface-sampled particles for adhesion interaction
//=============================================================================

/**
 * @struct FGPUBoundaryOwnerTransform
 * @brief Matrix pair for world/local space conversions.
 * 
 * @param WorldMatrix Local to World transform.
 * @param InverseWorldMatrix World to Local transform.
 */
struct FGPUBoundaryOwnerTransform
{
	FMatrix44f WorldMatrix;
	FMatrix44f InverseWorldMatrix;

	FGPUBoundaryOwnerTransform()
		: WorldMatrix(FMatrix44f::Identity)
		, InverseWorldMatrix(FMatrix44f::Identity)
	{
	}

	FGPUBoundaryOwnerTransform(const FMatrix44f& InWorldMatrix)
		: WorldMatrix(InWorldMatrix)
		, InverseWorldMatrix(InWorldMatrix.Inverse())
	{
	}
};
static_assert(sizeof(FGPUBoundaryOwnerTransform) == 128, "FGPUBoundaryOwnerTransform must be 128 bytes");

/**
 * @struct FGPUBoundaryAttachment
 * @brief Per-particle state for position-constrained attachment.
 * 
 * @param OwnerID Boundary owner ID.
 * @param BoneIndex Index of the bone for segment tracking.
 * @param AdhesionStrength Decayable adhesion factor.
 * @param AttachmentTime Time of attachment or detachment.
 * @param SurfaceDistance Distance from surface at attachment.
 * @param LocalDistance Current distance in local space.
 * @param Padding1 Alignment padding.
 * @param LocalPosition Relative position in bone space.
 * @param Padding2 Alignment padding.
 * @param LocalNormal Relative normal in bone space.
 * @param Padding3 Alignment padding.
 */
struct FGPUBoundaryAttachment
{
	int32 OwnerID;
	int32 BoneIndex;
	float AdhesionStrength;
	float AttachmentTime;
	float SurfaceDistance;
	float LocalDistance;
	float Padding1[2];
	FVector3f LocalPosition;
	float Padding2;
	FVector3f LocalNormal;
	float Padding3;

	FGPUBoundaryAttachment()
		: OwnerID(-1)
		, BoneIndex(-1)
		, AdhesionStrength(0.0f)
		, AttachmentTime(0.0f)
		, SurfaceDistance(0.0f)
		, LocalDistance(0.0f)
		, Padding1{0.0f, 0.0f}
		, LocalPosition(FVector3f::ZeroVector)
		, Padding2(0.0f)
		, LocalNormal(FVector3f::ZeroVector)
		, Padding3(0.0f)
	{
	}

	bool IsAttached() const { return OwnerID >= 0; }

	void Clear()
	{
		OwnerID = -1;
		BoneIndex = -1;
		AdhesionStrength = 0.0f;
		AttachmentTime = 0.0f;
		SurfaceDistance = 0.0f;
		LocalDistance = 0.0f;
		Padding1[0] = 0.0f;
		Padding1[1] = 0.0f;
		LocalPosition = FVector3f::ZeroVector;
		Padding2 = 0.0f;
		LocalNormal = FVector3f::ZeroVector;
		Padding3 = 0.0f;
	}
};
static_assert(sizeof(FGPUBoundaryAttachment) == 64, "FGPUBoundaryAttachment must be 64 bytes");

/**
 * @struct FGPUBoundaryAttachmentParams
 * @brief Controls particle attachment and detachment behavior.
 * 
 * @param AttachRadius Proximity threshold for bonding.
 * @param DetachDistanceMultiplier Factor for distance-based breaking.
 * @param DetachSpeedThreshold Velocity limit for breaking bonds.
 * @param AttachCooldown Time between consecutive attachments.
 * @param ConstraintBlend Mixing factor for position constraints.
 * @param CurrentTime Current simulation time.
 * @param DeltaTime Time step size.
 * @param bEnabled Global enable toggle.
 * @param FluidParticleCount Total particles in simulation.
 * @param BoundaryParticleCount Total particles in boundary.
 * @param SmoothingRadius Local search radius.
 * @param CellSize Spatial lookup granularity.
 */
struct FGPUBoundaryAttachmentParams
{
	float AttachRadius;
	float DetachDistanceMultiplier;
	float DetachSpeedThreshold;
	float AttachCooldown;
	float ConstraintBlend;
	float CurrentTime;
	float DeltaTime;
	int32 bEnabled;
	int32 FluidParticleCount;
	int32 BoundaryParticleCount;
	float SmoothingRadius;
	float CellSize;

	FGPUBoundaryAttachmentParams()
		: AttachRadius(5.0f)
		, DetachDistanceMultiplier(3.0f)
		, DetachSpeedThreshold(500.0f)
		, AttachCooldown(0.2f)
		, ConstraintBlend(0.8f)
		, CurrentTime(0.0f)
		, DeltaTime(0.016f)
		, bEnabled(1)
		, FluidParticleCount(0)
		, BoundaryParticleCount(0)
		, SmoothingRadius(20.0f)
		, CellSize(20.0f)
	{
	}
};
static_assert(sizeof(FGPUBoundaryAttachmentParams) == 48, "FGPUBoundaryAttachmentParams must be 48 bytes");

/**
 * @struct FGPUBoundaryOwnerAABB
 * @brief Bounding box for early-out optimization.
 * 
 * @param Min Box minimum corner.
 * @param Padding1 Alignment padding.
 * @param Max Box maximum corner.
 * @param Padding2 Alignment padding.
 */
struct FGPUBoundaryOwnerAABB
{
	FVector3f Min;
	float Padding1;
	FVector3f Max;
	float Padding2;

	FGPUBoundaryOwnerAABB()
		: Min(FVector3f(FLT_MAX))
		, Padding1(0.0f)
		, Max(FVector3f(-FLT_MAX))
		, Padding2(0.0f)
	{
	}

	FGPUBoundaryOwnerAABB(const FVector3f& InMin, const FVector3f& InMax)
		: Min(InMin)
		, Padding1(0.0f)
		, Max(InMax)
		, Padding2(0.0f)
	{
	}

	bool IsValid() const { return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z; }

	FGPUBoundaryOwnerAABB ExpandBy(float Radius) const
	{
		return FGPUBoundaryOwnerAABB(Min - FVector3f(Radius), Max + FVector3f(Radius));
	}

	bool Intersects(const FGPUBoundaryOwnerAABB& Other) const
	{
		return (Min.X <= Other.Max.X && Max.X >= Other.Min.X) &&
			   (Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y) &&
			   (Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z);
	}

	bool Contains(const FVector3f& Point) const
	{
		return Point.X >= Min.X && Point.X <= Max.X &&
			   Point.Y >= Min.Y && Point.Y <= Max.Y &&
			   Point.Z >= Min.Z && Point.Z <= Max.Z;
	}

	float DistanceSquaredToPoint(const FVector3f& Point) const
	{
		float DistSq = 0.0f;
		if (Point.X < Min.X) DistSq += FMath::Square(Min.X - Point.X);
		else if (Point.X > Max.X) DistSq += FMath::Square(Point.X - Max.X);
		if (Point.Y < Min.Y) DistSq += FMath::Square(Min.Y - Point.Y);
		else if (Point.Y > Max.Y) DistSq += FMath::Square(Point.Y - Point.Y);
		if (Point.Z < Min.Z) DistSq += FMath::Square(Min.Z - Point.Z);
		else if (Point.Z > Max.Z) DistSq += FMath::Square(Point.Z - Max.Z);
		return DistSq;
	}

	float DistanceToPoint(const FVector3f& Point) const { return FMath::Sqrt(DistanceSquaredToPoint(Point)); }
};
static_assert(sizeof(FGPUBoundaryOwnerAABB) == 32, "FGPUBoundaryOwnerAABB must be 32 bytes");

/**
 * @struct FGPUBoundaryParticle
 * @brief World-space boundary particle structure.
 * 
 * @param Position World position.
 * @param Psi Volume contribution factor.
 * @param Normal Surface normal.
 * @param OwnerID ID of the boundary owner.
 * @param Velocity World space velocity.
 * @param FrictionCoeff Friction coefficient.
 * @param BoneIndex Index of the bone for segment tracking.
 * @param OriginalIndex Pre-sort index for stable attachment.
 * @param Padding1 Alignment padding.
 * @param Padding2 Alignment padding.
 */
struct FGPUBoundaryParticle
{
	FVector3f Position;
	float Psi;
	FVector3f Normal;
	int32 OwnerID;
	FVector3f Velocity;
	float FrictionCoeff;
	int32 BoneIndex;
	int32 OriginalIndex;
	float Padding1;
	float Padding2;

	FGPUBoundaryParticle()
		: Position(FVector3f::ZeroVector)
		, Psi(1.0f)
		, Normal(FVector3f::UpVector)
		, OwnerID(-1)
		, Velocity(FVector3f::ZeroVector)
		, FrictionCoeff(0.6f)
		, BoneIndex(-1)
		, OriginalIndex(-1)
		, Padding1(0.0f)
		, Padding2(0.0f)
	{
	}

	FGPUBoundaryParticle(const FVector3f& InPosition, const FVector3f& InNormal, int32 InOwnerID,
						 float InPsi = 1.0f, const FVector3f& InVelocity = FVector3f::ZeroVector,
						 float InFrictionCoeff = 0.6f, int32 InBoneIndex = -1, int32 InOriginalIndex = -1)
		: Position(InPosition)
		, Psi(InPsi)
		, Normal(InNormal)
		, OwnerID(InOwnerID)
		, Velocity(InVelocity)
		, FrictionCoeff(InFrictionCoeff)
		, BoneIndex(InBoneIndex)
		, OriginalIndex(InOriginalIndex)
		, Padding1(0.0f)
		, Padding2(0.0f)
	{
	}
};
static_assert(sizeof(FGPUBoundaryParticle) == 64, "FGPUBoundaryParticle must be 64 bytes for GPU alignment");

/**
 * @struct FGPUBoundaryParticles
 * @brief Collection of world-space boundary particles.
 * 
 * @param Particles List of particles.
 */
struct FGPUBoundaryParticles
{
	TArray<FGPUBoundaryParticle> Particles;

	void Reset() { Particles.Reset(); }
	bool IsEmpty() const { return Particles.Num() == 0; }
	int32 GetCount() const { return Particles.Num(); }
	void Add(const FVector3f& Position, const FVector3f& Normal, int32 OwnerID, float Psi = 1.0f, float FrictionCoeff = 0.6f)
	{
		Particles.Emplace(Position, Normal, OwnerID, Psi, FVector3f::ZeroVector, FrictionCoeff);
	}
};

/**
 * @struct FGPUBoundaryParticleLocal
 * @brief Bone-local boundary particle for GPU skinning.
 * 
 * @param LocalPosition Relative position in bone space.
 * @param Psi Volume contribution factor.
 * @param LocalNormal Relative normal in bone space.
 * @param FrictionCoeff Friction coefficient.
 * @param BoneIndex Index of the bone for segment tracking.
 * @param Padding Alignment padding.
 */
struct FGPUBoundaryParticleLocal
{
	FVector3f LocalPosition;
	float Psi;
	FVector3f LocalNormal;
	float FrictionCoeff;
	int32 BoneIndex;
	FVector3f Padding;

	FGPUBoundaryParticleLocal()
		: LocalPosition(FVector3f::ZeroVector)
		, Psi(1.0f)
		, LocalNormal(FVector3f::UpVector)
		, FrictionCoeff(0.6f)
		, BoneIndex(-1)
		, Padding(FVector3f::ZeroVector)
	{
	}

	FGPUBoundaryParticleLocal(const FVector3f& InLocalPosition, int32 InBoneIndex, 
							  const FVector3f& InLocalNormal, float InPsi = 1.0f, float InFrictionCoeff = 0.6f)
		: LocalPosition(InLocalPosition)
		, Psi(InPsi)
		, LocalNormal(InLocalNormal)
		, FrictionCoeff(InFrictionCoeff)
		, BoneIndex(InBoneIndex)
		, Padding(FVector3f::ZeroVector)
	{
	}
};
static_assert(sizeof(FGPUBoundaryParticleLocal) == 48, "FGPUBoundaryParticleLocal must be 48 bytes for GPU alignment");

/**
 * @struct FGPUBoundaryAdhesionParams
 * @brief Parameter set used in boundary adhesion pass.
 * 
 * @param bEnabled Enable flag.
 * @param AdhesionForceStrength Bond strength multiplier.
 * @param AdhesionVelocityStrength Velocity transfer multiplier.
 * @param AdhesionRadius Interaction distance limit.
 * @param CohesionStrength Multiplier for inter-particle cohesion.
 * @param SmoothingRadius Search radius for density contribution.
 * @param BoundaryParticleCount Particle count in boundary.
 * @param FluidParticleCount Particle count in fluid.
 * @param DeltaTime Simulation time step.
 */
struct FGPUBoundaryAdhesionParams
{
	int32 bEnabled = 0;
	float AdhesionForceStrength = 1.0f;
	float AdhesionVelocityStrength = 0.5f;
	float AdhesionRadius = 50.0f;
	float CohesionStrength = 1.0f;
	float SmoothingRadius = 25.0f;
	int32 BoundaryParticleCount = 0;
	int32 FluidParticleCount = 0;
	float DeltaTime = 0.016f;

	FGPUBoundaryAdhesionParams() = default;
};

//=============================================================================
// Compact Particle Stats (for optimized GPU→CPU Readback)
// Reduces readback size from 64 bytes to 32 bytes per particle (50% reduction)
// Used in EndFrame when detailed GPU stats are not needed
//=============================================================================

/**
 * @struct FCompactParticleStats
 * @brief Essential fields for optimized GPU->CPU readback.
 * 
 * @param Position Current world space position.
 * @param ParticleID Unique persistent ID.
 * @param SourceID ID of the spawning source.
 * @param NeighborCount Local density stat.
 * @param Flags Particle state bitfield.
 * @param Padding Alignment padding.
 */
struct FCompactParticleStats
{
	FVector3f Position;
	int32 ParticleID;
	int32 SourceID;
	uint32 NeighborCount;
	uint32 Flags;
	float Padding;

	FCompactParticleStats()
		: Position(FVector3f::ZeroVector)
		, ParticleID(0)
		, SourceID(-1)
		, NeighborCount(0)
		, Flags(0)
		, Padding(0.0f)
	{
	}
};
static_assert(sizeof(FCompactParticleStats) == 32, "FCompactParticleStats must be 32 bytes");

/**
 * @struct FCompactParticleStatsEx
 * @brief Extended fields for optimized GPU->CPU readback.
 * 
 * @param Position Current world space position.
 * @param ParticleID Unique persistent ID.
 * @param Velocity Current velocity vector.
 * @param SourceID ID of the spawning source.
 * @param NeighborCount Local density stat.
 * @param Padding Alignment padding.
 */
struct FCompactParticleStatsEx
{
	FVector3f Position;
	int32 ParticleID;
	FVector3f Velocity;
	int32 SourceID;
	uint32 NeighborCount;
	FVector3f Padding;

	FCompactParticleStatsEx()
		: Position(FVector3f::ZeroVector)
		, ParticleID(0)
		, Velocity(FVector3f::ZeroVector)
		, SourceID(-1)
		, NeighborCount(0)
		, Padding(FVector3f::ZeroVector)
	{
	}
};
static_assert(sizeof(FCompactParticleStatsEx) == 48, "FCompactParticleStatsEx must be 48 bytes");
