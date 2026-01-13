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
	// Z-Order Space Configuration (Editable)
	//========================================

	/**
	 * Grid resolution preset for Z-Order sorting
	 * Controls the simulation bounds size and memory usage.
	 * - Small (64³): Compact bounds, fastest
	 * - Medium (128³): Balanced, recommended for 100k particles
	 * - Large (256³): Large bounds, more memory
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Z-Order Space")
	EGridResolutionPreset GridResolutionPreset = EGridResolutionPreset::Medium;

	/**
	 * Cell size for Z-Order grid (should match SmoothingRadius of fluid presets)
	 * Determines the grid cell size for neighbor search.
	 * Smaller values = more precision but smaller bounds extent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Z-Order Space", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	float CellSize = 20.0f;

	//========================================
	// Z-Order Space Info (Read-Only, Auto-Calculated)
	//========================================

	/**
	 * Grid resolution bits per axis (derived from GridResolutionPreset)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z-Order Space|Info")
	int32 GridAxisBits = 7;

	/**
	 * Grid resolution per axis (2^GridAxisBits)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z-Order Space|Info")
	int32 GridResolution = 128;

	/**
	 * Total number of cells (GridResolution³)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z-Order Space|Info")
	int32 MaxCells = 2097152;

	/**
	 * Simulation bounds extent (GridResolution * CellSize)
	 * This is the total size of the Z-Order space per axis.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z-Order Space|Info")
	float BoundsExtent = 2560.0f;

	/**
	 * World-space minimum bounds (Component location - Extent/2)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z-Order Space|Info")
	FVector WorldBoundsMin = FVector(-1280.0f, -1280.0f, -1280.0f);

	/**
	 * World-space maximum bounds (Component location + Extent/2)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z-Order Space|Info")
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
	FColor BoundsColor = FColor::Red;

	/** Wireframe line thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float BoundsLineThickness = 2.0f;

	//========================================
	// Public Methods
	//========================================

	/** Recalculate bounds based on CellSize and component location */
	UFUNCTION(BlueprintCallable, Category = "Z-Order Space")
	void RecalculateBounds();

	/** Check if a world position is within this Volume's bounds */
	UFUNCTION(BlueprintCallable, Category = "Z-Order Space")
	bool IsPositionInBounds(const FVector& WorldPosition) const;

	/** Get simulation bounds (world-space) */
	UFUNCTION(BlueprintCallable, Category = "Z-Order Space")
	void GetSimulationBounds(FVector& OutMin, FVector& OutMax) const;

	/** Get simulation bounds min (world-space) */
	UFUNCTION(BlueprintPure, Category = "Z-Order Space")
	FVector GetWorldBoundsMin() const { return WorldBoundsMin; }

	/** Get simulation bounds max (world-space) */
	UFUNCTION(BlueprintPure, Category = "Z-Order Space")
	FVector GetWorldBoundsMax() const { return WorldBoundsMax; }

	/** Get cell size */
	UFUNCTION(BlueprintPure, Category = "Z-Order Space")
	float GetCellSize() const { return CellSize; }

	/** Get bounds extent */
	UFUNCTION(BlueprintPure, Category = "Z-Order Space")
	float GetBoundsExtent() const { return BoundsExtent; }

	/** Get grid resolution preset */
	UFUNCTION(BlueprintPure, Category = "Z-Order Space")
	EGridResolutionPreset GetGridResolutionPreset() const { return GridResolutionPreset; }

	/** Get grid axis bits */
	UFUNCTION(BlueprintPure, Category = "Z-Order Space")
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
	UFUNCTION(BlueprintPure, Category = "Z-Order Space")
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
