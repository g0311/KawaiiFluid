// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IKawaiiFluidDataProvider.generated.h"

struct FKawaiiFluidParticle;
class FGPUFluidSimulator;

UINTERFACE(MinimalAPI, Blueprintable)
class UKawaiiFluidDataProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * @class IKawaiiFluidDataProvider
 * @brief Interface providing fluid simulation particle data to rendering modules.
 * 
 * Simulation modules implement this interface to expose their data without 
 * creating direct dependencies on the rendering layer.
 */
class IKawaiiFluidDataProvider
{
	GENERATED_BODY()

public:
	/**
	 * @brief Get simulation particle data.
	 * @return Const reference to particle array containing raw simulation state.
	 */
	virtual const TArray<FKawaiiFluidParticle>& GetParticles() const = 0;

	/**
	 * @brief Get the number of active particles in the simulation.
	 * @return Active particle count.
	 */
	virtual int32 GetParticleCount() const = 0;

	/**
	 * @brief Get the particle radius used in physical simulation (cm).
	 * @return Simulation particle radius.
	 */
	virtual float GetParticleRadius() const = 0;

	/**
	 * @brief Check if particle data is valid and ready for rendering.
	 * @return True if data is available.
	 */
	virtual bool IsDataValid() const = 0;

	/**
	 * @brief Get a human-readable identifier for this data provider.
	 * @return Debug name string.
	 */
	virtual FString GetDebugName() const = 0;

	/**
	 * @brief Check if GPU simulation is currently active.
	 * @return True if GPU buffers are ready for access.
	 */
	virtual bool IsGPUSimulationActive() const { return false; }

	/**
	 * @brief Get the number of particles currently in the GPU buffer.
	 * @return GPU particle count.
	 */
	virtual int32 GetGPUParticleCount() const { return 0; }

	/**
	 * @brief Get the GPU fluid simulator instance for direct buffer access.
	 * @return Pointer to GPU simulator, or nullptr if not using GPU.
	 */
	virtual FGPUFluidSimulator* GetGPUSimulator() const { return nullptr; }
};
