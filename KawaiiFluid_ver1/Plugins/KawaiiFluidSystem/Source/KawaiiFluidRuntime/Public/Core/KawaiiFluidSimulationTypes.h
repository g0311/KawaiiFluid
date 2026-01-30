// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <atomic>
#include "Core/KawaiiFluidRenderingTypes.h"
#include "KawaiiFluidSimulationTypes.generated.h"

class UFluidCollider;
class UKawaiiFluidInteractionComponent;
class UKawaiiFluidSimulationModule;

/**
 * World collision detection method
 */
UENUM(BlueprintType)
enum class EWorldCollisionMethod : uint8
{
	/** Legacy sweep-based collision (SweepSingleByChannel) */
	Sweep UMETA(DisplayName = "Sweep (Legacy)"),

	/** SDF-based collision using Overlap + ClosestPoint */
	SDF UMETA(DisplayName = "SDF (Distance-based)")
};

/**
 * Grid resolution preset for Z-Order sorting
 *
 * Controls the number of bits per axis for Morton code encoding.
 * Higher bits = larger simulation bounds but more memory/computation.
 *
 * Bounds extent formula: GridResolution × CellSize
 * - Small (6 bits):  64³ = 262,144 cells,  64 × CellSize bounds
 * - Medium (7 bits): 128³ = 2,097,152 cells, 128 × CellSize bounds
 * - Large (8 bits):  256³ = 16,777,216 cells, 256 × CellSize bounds
 *
 * For 100k particle simulations, Medium (7 bits) is recommended.
 */
UENUM(BlueprintType)
enum class EGridResolutionPreset : uint8
{
	/** 6 bits per axis = 64³ cells (262,144 total)
	 * Smallest bounds, fastest computation
	 * Good for: Small-scale simulations, local effects */
	Small UMETA(DisplayName = "Small"),

	/** 7 bits per axis = 128³ cells (2,097,152 total)
	 * Balanced bounds and performance
	 * Good for: Medium-scale simulations, 100k particles */
	Medium UMETA(DisplayName = "Medium"),

	/** 8 bits per axis = 256³ cells (16,777,216 total)
	 * Largest bounds, more computation
	 * Good for: Large-scale simulations, environmental effects */
	Large UMETA(DisplayName = "Large")
};

/** Helper functions for EGridResolutionPreset */
namespace GridResolutionPresetHelper
{
	/** Get bits per axis from preset */
	inline int32 GetAxisBits(EGridResolutionPreset Preset)
	{
		switch (Preset)
		{
		case EGridResolutionPreset::Small:  return 6;
		case EGridResolutionPreset::Medium: return 7;
		case EGridResolutionPreset::Large:  return 8;
		default: return 7;
		}
	}

	/** Get grid resolution per axis (2^bits) */
	inline int32 GetGridResolution(EGridResolutionPreset Preset)
	{
		return 1 << GetAxisBits(Preset);
	}

	/** Get total cell count (resolution³) */
	inline int32 GetMaxCells(EGridResolutionPreset Preset)
	{
		const int32 Res = GetGridResolution(Preset);
		return Res * Res * Res;
	}

	/** Get display name for UI */
	inline FString GetDisplayName(EGridResolutionPreset Preset)
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
	 * Get maximum volume extent (half-extent) that a preset can support
	 * @param Preset - Grid resolution preset
	 * @param CellSize - Cell size in cm (typically SmoothingRadius)
	 * @return Maximum half-extent per axis in cm
	 */
	inline float GetMaxExtentForPreset(EGridResolutionPreset Preset, float CellSize)
	{
		// Z-Order space is centered, so max extent is (GridResolution * CellSize) / 2
		return static_cast<float>(GetGridResolution(Preset)) * CellSize * 0.5f;
	}

	/**
	 * Auto-select the smallest Z-Order permutation that can contain the given volume extent
	 * This is used for the unified SimulationVolume system where users set a free extent
	 * and the system automatically selects the appropriate Z-Order space.
	 *
	 * @param VolumeExtent - User-defined volume half-extent (per axis)
	 * @param CellSize - Cell size in cm (typically SmoothingRadius)
	 * @return Smallest preset that can contain the volume, capped at Large
	 */
	inline EGridResolutionPreset SelectPresetForExtent(const FVector& VolumeExtent, float CellSize)
	{
		// Get the maximum axis extent
		const float MaxExtent = FMath::Max3(VolumeExtent.X, VolumeExtent.Y, VolumeExtent.Z);

		// Check presets from smallest to largest
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
	 * Clamp volume extent to the maximum supported by the largest preset
	 * @param VolumeExtent - User-defined volume half-extent
	 * @param CellSize - Cell size in cm
	 * @return Clamped extent that fits within Large preset bounds
	 */
	inline FVector ClampExtentToMaxSupported(const FVector& VolumeExtent, float CellSize)
	{
		const float MaxSupported = GetMaxExtentForPreset(EGridResolutionPreset::Large, CellSize);
		return FVector(
			FMath::Min(VolumeExtent.X, MaxSupported),
			FMath::Min(VolumeExtent.Y, MaxSupported),
			FMath::Min(VolumeExtent.Z, MaxSupported)
		);
	}
}

// EFluidDebugVisualization is now defined in KawaiiFluidRenderingTypes.h

/**
 * Fluid type for identifying different fluids in collision events
 */
UENUM(BlueprintType)
enum class EFluidType : uint8
{
	None		UMETA(DisplayName = "None"),
	Water		UMETA(DisplayName = "Water"),
	Lava		UMETA(DisplayName = "Lava"),
	Slime		UMETA(DisplayName = "Slime"),
	Oil			UMETA(DisplayName = "Oil"),
	Acid		UMETA(DisplayName = "Acid"),
	Blood		UMETA(DisplayName = "Blood"),
	Honey		UMETA(DisplayName = "Honey"),
	Custom1		UMETA(DisplayName = "Custom1"),
	Custom2		UMETA(DisplayName = "Custom2"),
	Custom3		UMETA(DisplayName = "Custom3"),
};

/**
 * Brush mode for adding or removing particles
 */
UENUM(BlueprintType)
enum class EFluidBrushMode : uint8
{
	Add		UMETA(DisplayName = "Add", ToolTip = "Add particles to simulation"),
	Remove	UMETA(DisplayName = "Remove", ToolTip = "Remove particles from simulation"),
};

/**
 * Method for estimating submerged volume for buoyancy calculation
 */
UENUM(BlueprintType)
enum class ESubmergedVolumeMethod : uint8
{
	/** Estimate submerged volume from number of contacting fluid particles. Fast but approximate. */
	ContactBased UMETA(DisplayName = "Contact Based"),

	/** Use a fixed percentage of the object's bounding box volume. Simple and predictable. */
	FixedRatio UMETA(DisplayName = "Fixed Ratio"),
};

/**
 * Brush settings struct for editor brush tool
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
 * Collision event data
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidCollisionEvent
{
	GENERATED_BODY()

	// ID-based (from GPU)
	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 ParticleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 SourceID = -1;              // Particle source Component ID

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 ColliderOwnerID = -1;       // Hit target Actor ID

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	int32 BoneIndex = -1;             // Hit bone index (-1 = none)

	// Pointer-based (looked up)
	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	TObjectPtr<UKawaiiFluidSimulationModule> SourceModule = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	TObjectPtr<UKawaiiFluidInteractionComponent> HitInteractionComponent = nullptr;

	// Collision data
	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	FVector HitNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Collision")
	float HitSpeed = 0.0f;
};

/** Collision event callback signature */
DECLARE_DELEGATE_OneParam(FOnFluidCollisionEvent, const FKawaiiFluidCollisionEvent&);

/**
 * Particle hit event delegate (Blueprint bindable)
 * Called when a particle collides with an actor
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnFluidParticleHitComponent,
	const FKawaiiFluidCollisionEvent&, CollisionEvent
);

/**
 * Simulation parameters passed to Context
 * Contains external forces, colliders, and other per-frame data
 */
USTRUCT(BlueprintType)
struct KAWAIIFLUIDRUNTIME_API FKawaiiFluidSimulationParams
{
	GENERATED_BODY()

	/** External force accumulated this frame */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	FVector ExternalForce = FVector::ZeroVector;

	/** Registered colliders */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TArray<TObjectPtr<UFluidCollider>> Colliders;

	/** Registered interaction components */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TArray<TObjectPtr<UKawaiiFluidInteractionComponent>> InteractionComponents;

	/** World reference for collision queries */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TObjectPtr<UWorld> World = nullptr;

	/** Use world collision */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	bool bUseWorldCollision = true;

	/** World collision detection method */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	EWorldCollisionMethod WorldCollisionMethod = EWorldCollisionMethod::SDF;

	/** Particle render radius (for collision detection) */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	float ParticleRadius = 5.0f;

	/** Actor to ignore in collision queries */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	TWeakObjectPtr<AActor> IgnoreActor;

	//========================================
	// GPU Simulation
	//========================================

	/** Grid resolution preset for Z-Order sorting (determines shader permutation) */
	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;

	/**
	 * Simulation origin (component world location)
	 * Used to offset preset simulation bounds to world space
	 * Preset bounds are defined relative to component, not absolute world coordinates
	 */
	FVector SimulationOrigin = FVector::ZeroVector;

	/** World bounds for GPU AABB collision (optional) */
	UPROPERTY(BlueprintReadWrite, Category = "Simulation")
	FBox WorldBounds = FBox(EForceInit::ForceInit);

	/** Bounds center (world space) - for OBB collision */
	FVector BoundsCenter = FVector::ZeroVector;

	/** Bounds half-extent (local space) - for OBB collision */
	FVector BoundsExtent = FVector::ZeroVector;

	/** Bounds rotation - for OBB collision (identity = AABB mode) */
	FQuat BoundsRotation = FQuat::Identity;

	/** Bounds collision restitution (bounciness) - used for Containment on GPU */
	float BoundsRestitution = 0.3f;

	/** Bounds collision friction - used for Containment on GPU */
	float BoundsFriction = 0.1f;

	//========================================
	// Static Boundary Particles (Akinci 2012)
	//========================================

	/** Enable static boundary particles for density contribution at walls/floors
	 * This helps prevent density deficit near boundaries which causes wall climbing artifacts */
	bool bEnableStaticBoundaryParticles = true;

	/** Static boundary particle spacing in cm */
	float StaticBoundaryParticleSpacing = 5.0f;

	//========================================
	// Collision Event Settings
	//========================================

	/** Enable collision events */
	bool bEnableCollisionEvents = false;

	/** Minimum velocity for collision event (cm/s) */
	float MinVelocityForEvent = 50.0f;

	/** Max events per frame */
	int32 MaxEventsPerFrame = 10;

	/**
	 * Pointer to atomic event counter (thread-safe, managed externally)
	 * Must be set before simulation if collision events are enabled
	 */
	std::atomic<int32>* EventCountPtr = nullptr;

	/** Per-particle event cooldown in seconds (prevents same particle spamming events) */
	float EventCooldownPerParticle = 0.1f;

	/** Pointer to per-particle last event time map (managed by component) */
	TMap<int32, float>* ParticleLastEventTimePtr = nullptr;

	/** Current game time for cooldown calculation */
	float CurrentGameTime = 0.0f;

	/** Collision event callback (non-UPROPERTY, set by component) */
	FOnFluidCollisionEvent OnCollisionEvent;

	/** Source ID for filtering collision events (only events from this source trigger callback) */
	int32 SourceID = -1;

	//========================================
	// Shape Matching (Slime)
	//========================================

	/** Enable shape matching constraint */
	bool bEnableShapeMatching = false;

	/** Shape matching stiffness (0 = no restoration, 1 = rigid) */
	float ShapeMatchingStiffness = 0.01f;

	/** Core particle stiffness multiplier */
	float ShapeMatchingCoreMultiplier = 1.0f;

	/** Core density constraint reduction (0 = full density effect, 1 = no density effect for core) */
	float CoreDensityConstraintReduction = 0.0f;

	//========================================
	// Surface Detection (Slime)
	//========================================

	/** Neighbor count threshold for surface detection (fewer neighbors = surface particle) */
	int32 SurfaceNeighborThreshold = 25;

	//========================================
	// CPU Collision Feedback Buffer (for deferred processing)
	//========================================

	/** CPU collision feedback buffer pointer (owned by Subsystem, written by Context) */
	TArray<FKawaiiFluidCollisionEvent>* CPUCollisionFeedbackBufferPtr = nullptr;

	/** CPU collision feedback buffer lock (ParallelFor safe) */
	FCriticalSection* CPUCollisionFeedbackLockPtr = nullptr;

	FKawaiiFluidSimulationParams() = default;
};

/**
 * Batching info for Module-based simulation
 */
struct FKawaiiFluidModuleBatchInfo
{
	/** Module that owns these particles */
	TObjectPtr<UKawaiiFluidSimulationModule> Module = nullptr;

	/** Start index in merged buffer */
	int32 StartIndex = 0;

	/** Number of particles from this module */
	int32 ParticleCount = 0;

	FKawaiiFluidModuleBatchInfo() = default;
	FKawaiiFluidModuleBatchInfo(UKawaiiFluidSimulationModule* InModule, int32 InStart, int32 InCount)
		: Module(InModule), StartIndex(InStart), ParticleCount(InCount) {}
};
