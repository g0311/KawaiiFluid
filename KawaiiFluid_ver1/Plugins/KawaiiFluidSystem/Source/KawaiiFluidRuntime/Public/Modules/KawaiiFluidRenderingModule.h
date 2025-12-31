// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IKawaiiFluidDataProvider.h"
#include "Core/KawaiiRenderParticle.h"
#include "KawaiiFluidRenderingModule.generated.h"

class UKawaiiFluidISMRenderer;
class UKawaiiFluidSSFRRenderer;

/**
 * Fluid Rendering Module (UObject-based)
 *
 * Orchestrates fluid rendering by owning and managing internal renderer instances.
 * Receives simulation data from DataProvider and distributes to enabled renderers.
 *
 * Architecture change: This is now a UObject (not ActorComponent) owned internally
 * by data provider components (e.g., UKawaiiFluidTestDataComponent).
 *
 * Responsibilities:
 * - Own and manage internal renderer instances (ISM, SSFR)
 * - Connect DataProvider with renderers
 * - Control which renderers are active via individual enable flags
 * - Orchestrate rendering updates
 *
 * Usage:
 * @code
 * // Owned internally by TestDataComponent
 * RenderingModule = NewObject<UKawaiiFluidRenderingModule>(this);
 * RenderingModule->bEnableISMRenderer = true;
 * RenderingModule->bEnableSSFRRenderer = false;
 * RenderingModule->Initialize(GetWorld(), GetOwner(), this);
 * @endcode
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidRenderingModule : public UObject
{
	GENERATED_BODY()

public:
	UKawaiiFluidRenderingModule();

	/**
	 * Initialize RenderingModule
	 *
	 * Called by owning component after creation.
	 * Creates both renderer instances and sets their enabled state.
	 *
	 * @param InWorld World context
	 * @param InOwnerComponent Parent scene component for attachment
	 * @param InDataProvider Data provider to use
	 */
	void Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, IKawaiiFluidDataProvider* InDataProvider);

	/**
	 * Cleanup resources
	 *
	 * Called by owning component before destruction.
	 */
	void Cleanup();

	/**
	 * Update all enabled renderers
	 *
	 * Fetches data from DataProvider and updates each enabled renderer.
	 * Called by owning component's TickComponent.
	 */
	void UpdateRenderers();

	/**
	 * Get current particle count
	 *
	 * @return Number of particles being rendered
	 */
	UFUNCTION(BlueprintPure, Category = "Rendering")
	int32 GetParticleCount() const;

	/**
	 * Check if module is initialized
	 *
	 * @return true if Initialize() has been called and CachedWorld is valid
	 */
	UFUNCTION(BlueprintPure, Category = "Rendering")
	bool IsInitialized() const { return CachedWorld != nullptr && DataProviderPtr != nullptr; }

	//========================================
	// Renderer Access (read-only)
	//========================================

	/** Get ISM renderer instance */
	UFUNCTION(BlueprintPure, Category = "Rendering")
	UKawaiiFluidISMRenderer* GetISMRenderer() const { return ISMRenderer; }

	/** Get SSFR renderer instance */
	UFUNCTION(BlueprintPure, Category = "Rendering")
	UKawaiiFluidSSFRRenderer* GetSSFRRenderer() const { return SSFRRenderer; }

protected:
	//========================================
	// Internal State
	//========================================

	/** Cached world reference (replaces GetWorld()) */
	UPROPERTY()
	TObjectPtr<UWorld> CachedWorld;

	/** Cached owner component reference (for renderer attachment) */
	UPROPERTY()
	TObjectPtr<USceneComponent> CachedOwnerComponent;

	/** Data provider pointer (interface, not UPROPERTY) */
	IKawaiiFluidDataProvider* DataProviderPtr;

	//========================================
	// Internal Renderer Instances
	//========================================

	/** ISM renderer instance (UPROPERTY for GC) */
	UPROPERTY()
	TObjectPtr<UKawaiiFluidISMRenderer> ISMRenderer;

	/** SSFR renderer instance (UPROPERTY for GC) */
	UPROPERTY()
	TObjectPtr<UKawaiiFluidSSFRRenderer> SSFRRenderer;

	/** Render particles cache (for data conversion) */
	TArray<FKawaiiRenderParticle> RenderParticlesCache;
};
