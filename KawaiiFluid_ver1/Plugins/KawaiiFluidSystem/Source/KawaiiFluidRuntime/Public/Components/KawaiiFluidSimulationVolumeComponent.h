// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "KawaiiFluidSimulationVolumeComponent.generated.h"

/**
 * Kawaii Fluid Simulation Volume Component
 *
 * Defines a Z-Order simulation space for fluid particles.
 * Multiple fluid components can share the same Volume to enable particle interaction.
 *
 * Usage:
 * 1. Place an Actor with UKawaiiFluidSimulationVolumeComponent in the level
 * 2. Set CellSize to match your fluid's SmoothingRadius
 * 3. Assign this Volume to UKawaiiFluidComponent's TargetVolume property
 *
 * All fluid components sharing the same Volume will:
 * - Use the same Z-Order space bounds
 * - Be able to interact with each other (if same Preset)
 * - Be batched together for better performance
 */
UCLASS(ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent, DisplayName="Kawaii Fluid Simulation Volume"))
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidSimulationVolumeComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKawaiiFluidSimulationVolumeComponent();

	//========================================
	// UActorComponent Interface
	//========================================

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//========================================
	// Volume Size Configuration (User-Friendly)
	//========================================

	/**
	 * Use uniform (cube) size for simulation volume
	 * When checked, enter a single size value. When unchecked, enter separate X/Y/Z values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Volume", meta = (DisplayName = "Uniform Size"))
	bool bUniformSize = true;

	/**
	 * Simulation volume size (cm) - cube dimensions when Uniform Size is checked
	 * Particles are confined within this box. Enter the full box size (not half).
	 *
	 * Example: 400 cm means a 400×400×400 cm cube.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Volume",
		meta = (EditCondition = "bUniformSize", EditConditionHides, DisplayName = "Size", ClampMin = "10.0", ClampMax = "10240.0"))
	float UniformVolumeSize = 2560.0f;

	/**
	 * Simulation volume size (cm) - separate X/Y/Z dimensions
	 * Particles are confined within this box. Enter the full box size (not half).
	 *
	 * Example: (400, 300, 200) means a 400×300×200 cm box.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Volume",
		meta = (EditCondition = "bUniformSize == false", EditConditionHides, DisplayName = "Size"))
	FVector VolumeSize = FVector(2560.0f, 2560.0f, 2560.0f);

	/**
	 * Wall bounce (0 = no bounce, 1 = full bounce)
	 * How much particles bounce when hitting the volume walls.
	 * Modules using this Volume will inherit this setting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Volume",
		meta = (DisplayName = "Wall Bounce", ClampMin = "0.0", ClampMax = "1.0"))
	float WallBounce = 0.3f;

	/**
	 * Wall friction (0 = slippery, 1 = sticky)
	 * How much particles slow down when sliding along walls.
	 * Modules using this Volume will inherit this setting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Volume",
		meta = (DisplayName = "Wall Friction", ClampMin = "0.0", ClampMax = "1.0"))
	float WallFriction = 0.1f;

	//========================================
	// Z-Order Space Configuration (Advanced)
	//========================================

	/**
	 * Cell size for Z-Order grid (should match SmoothingRadius of fluid presets)
	 * Determines the grid cell size for neighbor search.
	 * Smaller values = more precision but smaller bounds extent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Volume|Advanced", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	float CellSize = 20.0f;

	/**
	 * Grid resolution preset for Z-Order sorting (auto-selected based on volume size)
	 * Read-only - the system automatically selects the optimal preset.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation Volume|Advanced", meta = (DisplayName = "Internal Grid Preset (Auto)"))
	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;

	//========================================
	// Internal Info (Read-Only, Auto-Calculated)
	//========================================

	/**
	 * Grid resolution bits per axis (derived from GridResolutionPreset)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation Volume|Advanced")
	int32 GridAxisBits = 7;

	/**
	 * Grid resolution per axis (2^GridAxisBits)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation Volume|Advanced")
	int32 GridResolution = 128;

	/**
	 * Total number of cells (GridResolution³)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation Volume|Advanced")
	int32 MaxCells = 2097152;

	/**
	 * Actual simulation bounds extent (may differ from requested VolumeSize due to grid constraints)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation Volume|Advanced")
	float BoundsExtent = 2560.0f;

	/**
	 * World-space minimum bounds (Component location - Extent/2)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation Volume|Advanced")
	FVector WorldBoundsMin = FVector(-1280.0f, -1280.0f, -1280.0f);

	/**
	 * World-space maximum bounds (Component location + Extent/2)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation Volume|Advanced")
	FVector WorldBoundsMax = FVector(1280.0f, 1280.0f, 1280.0f);

	//========================================
	// Debug Visualization
	//========================================

	/** Show bounds wireframe in editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowBoundsInEditor = true;

	/** Show bounds wireframe at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowBoundsAtRuntime = false;

	/** Wireframe color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FColor BoundsColor = FColor::Green;

	/** Wireframe line thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float BoundsLineThickness = 2.0f;

	/** Show internal Z-Order grid space wireframe (for debugging spatial partitioning) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Advanced", meta = (DisplayName = "Show Internal Grid Wireframe"))
	bool bShowZOrderSpaceWireframe = false;

	/** Internal grid wireframe color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Advanced",
		meta = (EditCondition = "bShowZOrderSpaceWireframe", EditConditionHides, DisplayName = "Grid Wireframe Color"))
	FColor ZOrderSpaceWireframeColor = FColor::Red;

	//========================================
	// Public Methods
	//========================================

	/** Recalculate bounds based on VolumeSize and component location */
	UFUNCTION(BlueprintCallable, Category = "Simulation Volume")
	void RecalculateBounds();

	/** Check if a world position is within this Volume's bounds */
	UFUNCTION(BlueprintCallable, Category = "Simulation Volume")
	bool IsPositionInBounds(const FVector& WorldPosition) const;

	/** Get simulation bounds (world-space) */
	UFUNCTION(BlueprintCallable, Category = "Simulation Volume")
	void GetSimulationBounds(FVector& OutMin, FVector& OutMax) const;

	/** Get simulation bounds min (world-space) */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	FVector GetWorldBoundsMin() const { return WorldBoundsMin; }

	/** Get simulation bounds max (world-space) */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	FVector GetWorldBoundsMax() const { return WorldBoundsMax; }

	/** Get the effective volume size (full size, cm)
	 * Returns UniformVolumeSize as FVector if bUniformSize is true, otherwise VolumeSize
	 */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	FVector GetEffectiveVolumeSize() const
	{
		return bUniformSize ? FVector(UniformVolumeSize) : VolumeSize;
	}

	/** Get the volume half-extent (for internal collision/rendering use) */
	FVector GetVolumeHalfExtent() const
	{
		return GetEffectiveVolumeSize() * 0.5f;
	}

	/** Get wall bounce coefficient */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	float GetWallBounce() const { return WallBounce; }

	/** Get wall friction coefficient */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	float GetWallFriction() const { return WallFriction; }

	/** Get cell size */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	float GetCellSize() const { return CellSize; }

	/** Get bounds extent */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	float GetBoundsExtent() const { return BoundsExtent; }

	/** Get grid resolution preset */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	EGridResolutionPreset GetGridResolutionPreset() const { return GridResolutionPreset; }

	/** Get grid axis bits */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	int32 GetGridAxisBits() const { return GridAxisBits; }

	//========================================
	// Registration (for Subsystem tracking)
	//========================================

	/** Get all registered fluid modules using this Volume */
	const TArray<TWeakObjectPtr<UKawaiiFluidSimulationModule>>& GetRegisteredModules() const { return RegisteredModules; }

	/** Register a fluid module to this Volume */
	void RegisterModule(UKawaiiFluidSimulationModule* Module);

	/** Unregister a fluid module from this Volume */
	void UnregisterModule(UKawaiiFluidSimulationModule* Module);

	/** Get the number of registered modules */
	UFUNCTION(BlueprintPure, Category = "Simulation Volume")
	int32 GetRegisteredModuleCount() const { return RegisteredModules.Num(); }

private:
	//========================================
	// Registered Modules
	//========================================

	/** Fluid modules using this Volume */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UKawaiiFluidSimulationModule>> RegisteredModules;

	//========================================
	// Subsystem Registration
	//========================================

	void RegisterToSubsystem();
	void UnregisterFromSubsystem();

	//========================================
	// Debug Visualization
	//========================================

	void DrawBoundsVisualization();
};
