// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <atomic>
#include "Core/KawaiiFluidRenderingTypes.h"
#include "KawaiiFluidSimulationTypes.generated.h"

class UKawaiiFluidCollider;
class UKawaiiFluidInteractionComponent;
class UKawaiiFluidSimulationModule;

/**
 * @enum EWorldCollisionMethod
 * @brief Method used for detecting collisions with world geometry.
 */
UENUM(BlueprintType)
enum class EWorldCollisionMethod : uint8
{
	Sweep UMETA(DisplayName = "Sweep (Legacy)", ToolTip = "Legacy sweep-based collision using SweepSingleByChannel."),
	SDF UMETA(DisplayName = "SDF (Distance-based)", ToolTip = "SDF-based collision using Overlap and ClosestPoint for better stability.")
};

/**
 * @enum EGridResolutionPreset
 * @brief Grid resolution preset for Z-Order sorting, controlling Morton code bits per axis.
 * 
 * Bounds extent is calculated as GridResolution x CellSize.
 * Higher bits support larger simulation bounds but require more memory and computation.
 */
UENUM(BlueprintType)
enum class EGridResolutionPreset : uint8
{
	Small UMETA(DisplayName = "Small", ToolTip = "6 bits per axis = 64^3 cells. Best for small-scale local effects."),
	Medium UMETA(DisplayName = "Medium", ToolTip = "7 bits per axis = 128^3 cells. Balanced performance, recommended for 100k particles."),
	Large UMETA(DisplayName = "Large", ToolTip = "8 bits per axis = 256^3 cells. Best for large-scale environmental effects.")
};

namespace GridResolutionPresetHelper
{
	KAWAIIFLUIDRUNTIME_API int32 GetAxisBits(EGridResolutionPreset Preset);

	KAWAIIFLUIDRUNTIME_API int32 GetGridResolution(EGridResolutionPreset Preset);

	KAWAIIFLUIDRUNTIME_API int32 GetMaxCells(EGridResolutionPreset Preset);

	KAWAIIFLUIDRUNTIME_API FString GetDisplayName(EGridResolutionPreset Preset);

	KAWAIIFLUIDRUNTIME_API float GetMaxExtentForPreset(EGridResolutionPreset Preset, float CellSize);

	KAWAIIFLUIDRUNTIME_API EGridResolutionPreset SelectPresetForExtent(const FVector& VolumeExtent, float CellSize);

	KAWAIIFLUIDRUNTIME_API FVector ClampExtentToMaxSupported(const FVector& VolumeExtent, float CellSize);
}

/**
 * @enum EFluidBrushMode
 * @brief Mode for adding or removing particles using the fluid brush.
 */
UENUM(BlueprintType)
enum class EFluidBrushMode : uint8
{
	Add UMETA(DisplayName = "Add", ToolTip = "Add particles to the simulation."),
	Remove UMETA(DisplayName = "Remove", ToolTip = "Remove particles from the simulation.")
};

/**
 * @enum ESubmergedVolumeMethod
 * @brief Method for estimating the submerged volume used in buoyancy calculations.
 */
UENUM(BlueprintType)
enum class ESubmergedVolumeMethod : uint8
{
	ContactBased UMETA(DisplayName = "Contact Based", ToolTip = "Estimate volume from the number of contacting particles. Fast but approximate."),
	FixedRatio UMETA(DisplayName = "Fixed Ratio", ToolTip = "Use a fixed percentage of the object's bounding box. Simple and predictable.")
};

/**
 * @struct FFluidBrushSettings
 * @brief Settings for the editor fluid brush tool.
 * 
 * @param Mode The brush mode (Add/Remove).
 * @param Radius Radius of the brush stroke.
 * @param ParticlesPerStroke Number of particles spawned per single brush stroke.
 * @param InitialVelocity Initial velocity applied to newly added particles.
 * @param Randomness Amount of random variation in particle placement.
 * @param StrokeInterval Time interval between consecutive strokes.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FFluidBrushSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Brush")
	EFluidBrushMode Mode = EFluidBrushMode::Add;

	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float Radius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = "1", ClampMax = "100"))
	int32 ParticlesPerStroke = 15;

	UPROPERTY(EditAnywhere, Category = "Brush")
	FVector InitialVelocity = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Randomness = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float StrokeInterval = 0.03f;
};

/**
 * @struct FKawaiiFluidCollisionEvent
 * @brief Data structure containing information about a particle collision event.
 * 
 * @param ParticleIndex Index of the particle that collided.
 * @param SourceID ID of the component that spawned the particle.
 * @param ColliderOwnerID ID of the actor that was hit.
 * @param BoneIndex Index of the specific bone hit (for skeletal meshes).
 * @param HitActor Pointer to the actor that was hit.
 * @param SourceModule Pointer to the simulation module that owns the particle.
 * @param HitInteractionComponent Pointer to the interaction component that was hit.
 * @param HitLocation World-space location of the collision.
 * @param HitNormal Surface normal at the collision point.
 * @param HitSpeed Velocity magnitude of the particle at the time of impact.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidCollisionEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 ParticleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 SourceID = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 ColliderOwnerID = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 BoneIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	TObjectPtr<UKawaiiFluidSimulationModule> SourceModule = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	TObjectPtr<UKawaiiFluidInteractionComponent> HitInteractionComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	FVector HitNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	float HitSpeed = 0.0f;
};

DECLARE_DELEGATE_OneParam(FOnFluidCollisionEvent, const FKawaiiFluidCollisionEvent&);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnFluidParticleHitComponent,
	const FKawaiiFluidCollisionEvent&, CollisionEvent
);

/**
 * @struct FKawaiiFluidSimulationParams
 * @brief Parameters passed to the simulation context for each frame.
 * 
 * @param ExternalForce Force vector accumulated this frame (e.g. wind).
 * @param Colliders Array of active fluid collider components.
 * @param InteractionComponents Array of active interaction components.
 * @param World Reference to the world for collision queries.
 * @param bUseWorldCollision Whether to enable collision with world geometry.
 * @param WorldCollisionMethod Detection method for world collision (Sweep/SDF).
 * @param ParticleRadius Radius used for collision detection.
 * @param IgnoreActor Actor to be ignored during collision queries.
 * @param GridResolutionPreset Preset determining the resolution of the Z-Order spatial hash.
 * @param SimulationOrigin World-space origin used to offset local simulation bounds.
 * @param WorldBounds AABB bounds for GPU collision containment.
 * @param BoundsCenter World-space center for OBB collision.
 * @param BoundsExtent Half-extent for OBB collision.
 * @param BoundsRotation Rotation for OBB collision.
 * @param BoundsRestitution Restitution coefficient for boundary collisions.
 * @param BoundsFriction Friction coefficient for boundary collisions.
 * @param bSkipBoundsCollision If true, particles are not constrained by volume bounds.
 * @param bEnableStaticBoundaryParticles Enable Akinci-style static boundary particles for density consistency.
 * @param StaticBoundaryParticleSpacing Spacing between generated static boundary particles.
 * @param bEnableCollisionEvents Enable generation of collision event data.
 * @param MinVelocityForEvent Minimum speed required to trigger a collision event.
 * @param MaxEventsPerFrame Maximum number of collision events allowed per frame.
 * @param EventCountPtr Thread-safe pointer to the atomic event counter.
 * @param EventCooldownPerParticle Cooldown time to prevent event spamming from the same particle.
 * @param ParticleLastEventTimePtr Pointer to a map tracking the last event time per particle.
 * @param CurrentGameTime Current game time for cooldown calculations.
 * @param OnCollisionEvent Delegate callback for collision events.
 * @param SourceID ID filter for collision events.
 * @param SurfaceNeighborThreshold Neighbor count threshold for identifying surface particles.
 * @param CPUCollisionFeedbackBufferPtr Buffer for deferred collision processing on the CPU.
 * @param CPUCollisionFeedbackLockPtr Critical section for thread-safe buffer access.
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidSimulationParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	FVector ExternalForce = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TArray<TObjectPtr<UKawaiiFluidCollider>> Colliders;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TArray<TObjectPtr<UKawaiiFluidInteractionComponent>> InteractionComponents;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TObjectPtr<UWorld> World = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	bool bUseWorldCollision = true;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	EWorldCollisionMethod WorldCollisionMethod = EWorldCollisionMethod::SDF;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	float ParticleRadius = 5.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TWeakObjectPtr<AActor> IgnoreActor;

	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;

	FVector SimulationOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	FBox WorldBounds = FBox(EForceInit::ForceInit);

	FVector BoundsCenter = FVector::ZeroVector;

	FVector BoundsExtent = FVector::ZeroVector;

	FQuat BoundsRotation = FQuat::Identity;

	float BoundsRestitution = 0.3f;

	float BoundsFriction = 0.1f;

	bool bSkipBoundsCollision = false;

	bool bEnableStaticBoundaryParticles = true;

	float StaticBoundaryParticleSpacing = 5.0f;

	bool bEnableCollisionEvents = false;

	float MinVelocityForEvent = 50.0f;

	int32 MaxEventsPerFrame = 10;

	std::atomic<int32>* EventCountPtr = nullptr;

	float EventCooldownPerParticle = 0.1f;

	TMap<int32, float>* ParticleLastEventTimePtr = nullptr;

	float CurrentGameTime = 0.0f;

	FOnFluidCollisionEvent OnCollisionEvent;

	int32 SourceID = -1;

	int32 SurfaceNeighborThreshold = 25;

	TArray<FKawaiiFluidCollisionEvent>* CPUCollisionFeedbackBufferPtr = nullptr;

	FCriticalSection* CPUCollisionFeedbackLockPtr = nullptr;

	FKawaiiFluidSimulationParams() = default;
};

/**
 * @struct FKawaiiFluidModuleBatchInfo
 * @brief Batching information for module-based fluid simulation.
 * 
 * @param Module Pointer to the module that owns these particles.
 * @param StartIndex The start index of this module's particles in the merged buffer.
 * @param ParticleCount The number of particles belonging to this module.
 */
struct FKawaiiFluidModuleBatchInfo
{
	TObjectPtr<UKawaiiFluidSimulationModule> Module = nullptr;

	int32 StartIndex = 0;

	int32 ParticleCount = 0;

	FKawaiiFluidModuleBatchInfo() = default;
	FKawaiiFluidModuleBatchInfo(UKawaiiFluidSimulationModule* InModule, int32 InStart, int32 InCount)
		: Module(InModule), StartIndex(InStart), ParticleCount(InCount) {}
};
