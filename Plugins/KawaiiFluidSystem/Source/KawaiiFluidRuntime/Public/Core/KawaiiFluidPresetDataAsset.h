// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Rendering/Parameters/KawaiiFluidRenderingParameters.h"
#include "KawaiiFluidPresetDataAsset.generated.h"

class UKawaiiFluidPresetDataAsset;

/** @brief Delegate broadcast when preset properties change (SmoothingRadius, etc.) */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPresetPropertyChanged, UKawaiiFluidPresetDataAsset*);

/**
 * @class UKawaiiFluidPresetDataAsset
 * @brief Data Asset that stores physics and rendering parameters for a specific fluid type.
 * 
 * Provides an editor-friendly way to configure fluid behavior including viscosity, density, 
 * surface tension, and simulation resolution. Parameters are grouped into Rendering, 
 * Physics Material, and Simulation Solver categories.
 * 
 * @param RenderingParameters Metaball rendering settings shared by components using this preset.
 * @param Viscosity XSPH viscosity coefficient (0=water, 0.5=slime, 0.8=honey).
 * @param Density Fluid density in kg/m³ (Water: 1000, Slime: 1200, Honey: 1400).
 * @param Compressibility Stiffness of the density constraint (lower = more incompressible).
 * @param SurfaceTension Position-based constraint minimizing surface area for rounded droplets.
 * @param Cohesion Force-based attraction between all particles (Akinci 2013).
 * @param Adhesion Force pulling fluid toward boundary surfaces (characters/colliders).
 * @param Bounciness Restitution coefficient for boundary collisions (0 to 1).
 * @param Friction Surface friction coefficient for boundary interactions.
 * @param AirResistance Velocity damping per substep to stabilize simulation.
 * @param ParticleRadius Primary resolution input (cm) representing particle size.
 * @param SpacingRatio Ratio of particle density relative to smoothing radius (d/h).
 * @param SmoothingRadius Kernel radius (h) auto-calculated from radius and spacing ratio.
 * @param ParticleSpacing Initial distance between particles when spawned.
 * @param ParticleMass Calculated mass per particle based on density and spacing.
 * @param EstimatedNeighborCount Predicted number of neighbors within the smoothing radius.
 * @param SubstepDeltaTime Target time step for simulation stability.
 * @param MaxSubsteps Upper limit on the number of substeps per frame.
 * @param SolverIterations XPBD constraint solver iterations (4-6 recommended for water).
 * @param ComplianceExponent Scaling factor for compressibility based on SmoothingRadius.
 * @param Gravity Acceleration vector applied to all fluid particles.
 * @param FluidName Unique identifier for collision events (e.g., "Lava", "Water").
 * @param CollisionThreshold Margin added to particle radius for collision detection.
 * @param AdhesionVelocityStrength Factor for fluid velocity matching moving boundaries.
 * @param AdhesionRadius Maximum distance for adhesion force application.
 * @param AdhesionContactOffset Offset allowing deeper overlap with colliders during adhesion.
 * @param AdhesionBoneVelocityScale Inheritance factor for bone motion during attachment.
 * @param bEnableBoundaryAttachment If true, particles attach to and follow bone movement.
 * @param bEnableRelativeVelocityDamping Reduces pressure repulsion when boundaries approach fluid.
 * @param RelativeVelocityDampingStrength Strength of the pressure damping effect.
 * @param BoundaryVelocityTransferStrength Intensity of velocity inheritance from boundaries.
 * @param BoundaryDetachSpeedThreshold Relative speed at which particles start to detach.
 * @param BoundaryMaxDetachSpeed Speed above which full detachment occurs.
 * @param bEnableParticleSleeping Freezes slow-moving particles to reduce jitter.
 * @param SleepVelocityThreshold Speed threshold for entering the sleep state.
 * @param SleepFrameThreshold Required consecutive frames below threshold to sleep.
 * @param WakeVelocityThreshold Speed required to wake a sleeping particle.
 * @param bEnableStackPressure Enables weight transfer from top to bottom attached particles.
 * @param StackPressureScale Intensity multiplier for the dripping/sliding effect.
 * @param StackPressureRadius Search radius for weight transfer (0 = use SmoothingRadius).
 * @param ArtificialPressure Strength of the PBF tensile instability correction (anti-clumping).
 * @param ArtificialPressureExponent Sharpness of the anti-clumping effect.
 * @param ArtificialPressureDeltaQ Reference distance ratio for tensile correction.
 * @param SurfaceTensionActivationRatio Radius ratio where surface tension starts.
 * @param SurfaceTensionFalloffRatio Radius ratio where surface tension reaches zero.
 * @param SurfaceTensionSurfaceThreshold Neighbor count for identifying surface particles.
 * @param MaxSurfaceTensionCorrectionPerIteration Stability limit for position correction.
 * @param SurfaceTensionVelocityDamping Under-relaxation factor for surface tension.
 * @param SurfaceTensionTolerance Dead zone around activation distance.
 * @param ThumbnailInfo Metadata for editor thumbnail generation.
 * @param OnPropertyChanged Broadcasted when properties are modified in the editor.
 */
UCLASS(BlueprintType)
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidPresetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UKawaiiFluidPresetDataAsset();

	//========================================
	// Rendering Parameters
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (ShowOnlyInnerProperties))
	FKawaiiFluidRenderingParameters RenderingParameters;

	//========================================
	// Physics | Material
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Viscosity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "100.0"))
	float Density = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "0.001"))
	float Compressibility = 0.00001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SurfaceTension = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float Cohesion = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float Adhesion = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Bounciness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Friction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirResistance = 0.05f;

	UFUNCTION(BlueprintPure, Category = "Physics|Material")
	float GetDamping() const { return 1.0f - (AirResistance * 0.1f); }

	//========================================
	// Physics | Simulation | Resolution
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Resolution", meta = (ClampMin = "0.1"))
	float ParticleRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Resolution",
		meta = (ClampMin = "0.1", ClampMax = "0.7"))
	float SpacingRatio = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics|Simulation|Resolution")
	float SmoothingRadius = 20.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics|Simulation|Resolution")
	float ParticleSpacing = 10.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics|Simulation|Resolution")
	float ParticleMass = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics|Simulation|Resolution")
	int32 EstimatedNeighborCount = 33;

	//========================================
	// Physics | Simulation | Solver
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Solver", meta = (ClampMin = "0.001", ClampMax = "0.05"))
	float SubstepDeltaTime = 1.0f / 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Solver", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxSubsteps = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Solver", meta = (ClampMin = "1", ClampMax = "10"))
	int32 SolverIterations = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Solver", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float ComplianceExponent = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Solver")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	//========================================
	// Physics | Simulation | Collision
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Collision")
	FName FluidName = NAME_None;

	UFUNCTION(BlueprintPure, Category = "Physics|Simulation|Collision")
	FName GetFluidName() const { return FluidName; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Collision", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float CollisionThreshold = 1.0f;

	//========================================
	// Physics | Simulation | Adhesion
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Adhesion", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float AdhesionVelocityStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Adhesion", meta = (ClampMin = "0.1"))
	float AdhesionRadius = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Adhesion", meta = (ClampMin = "-50.0", ClampMax = "50.0"))
	float AdhesionContactOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Adhesion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AdhesionBoneVelocityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Adhesion")
	bool bEnableBoundaryAttachment = true;

	//========================================
	// Physics | Simulation | Boundary | Interaction
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Boundary|Interaction")
	bool bEnableRelativeVelocityDamping = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Boundary|Interaction",
		meta = (EditCondition = "bEnableRelativeVelocityDamping", ClampMin = "0.0", ClampMax = "1.0"))
	float RelativeVelocityDampingStrength = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Boundary|Interaction",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoundaryVelocityTransferStrength = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Boundary|Interaction",
		meta = (ClampMin = "0.0", ClampMax = "5000.0"))
	float BoundaryDetachSpeedThreshold = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Boundary|Interaction",
		meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float BoundaryMaxDetachSpeed = 1500.0f;

	//========================================
	// Physics | Simulation | Stability
	//========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability")
	bool bEnableParticleSleeping = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability",
		meta = (EditCondition = "bEnableParticleSleeping", ClampMin = "0.1", ClampMax = "100.0"))
	float SleepVelocityThreshold = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability",
		meta = (EditCondition = "bEnableParticleSleeping", ClampMin = "1", ClampMax = "120"))
	int32 SleepFrameThreshold = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability",
		meta = (EditCondition = "bEnableParticleSleeping", ClampMin = "1.0", ClampMax = "200.0"))
	float WakeVelocityThreshold = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability")
	bool bEnableStackPressure = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability",
		meta = (EditCondition = "bEnableStackPressure", ClampMin = "0.0", ClampMax = "1000.0"))
	float StackPressureScale = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability",
		meta = (EditCondition = "bEnableStackPressure", ClampMin = "0.0"))
	float StackPressureRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Simulation|Stability",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArtificialPressure = 0.0f;

	int32 ArtificialPressureExponent = 4;

	float ArtificialPressureDeltaQ = 0.0f;

	//========================================
	// Physics | Simulation | Surface Tension
	//========================================

	float SurfaceTensionActivationRatio = 0.5f;

	float SurfaceTensionFalloffRatio = 0.7f;

	int32 SurfaceTensionSurfaceThreshold = 0;

	float MaxSurfaceTensionCorrectionPerIteration = 5.0f;

	float SurfaceTensionVelocityDamping = 0.7f;

	float SurfaceTensionTolerance = 1.0f;

	//========================================
	// Utility Functions
	//========================================

	UFUNCTION(BlueprintCallable, Category = "Physics")
	void RecalculateDerivedParameters();

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Thumbnail")
	TObjectPtr<class UThumbnailInfo> ThumbnailInfo;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FOnPresetPropertyChanged OnPropertyChanged;
#endif
};
