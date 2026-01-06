// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Rendering/FluidDensityGrid.h"
#include "FluidShadowProxyComponent.generated.h"

class FFluidShadowSceneProxy;
class UMaterialInstanceDynamic;

/**
 * @brief Primitive component that creates a proxy for fluid shadow casting.
 *
 * This component renders a bounding box mesh that participates in shadow passes.
 * Uses a Material Instance Dynamic with Opacity Mask to create soft shadow edges
 * based on the fluid density. This approach works with UE5's native shadow systems
 * including Virtual Shadow Maps without engine modification.
 *
 * @param DensityGrid Reference to the fluid's 3D density grid.
 * @param FluidBounds World-space bounds of the fluid volume.
 * @param ShadowMaterial Material used for shadow depth rendering.
 */
UCLASS(ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent))
class KAWAIIFLUIDRUNTIME_API UFluidShadowProxyComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UFluidShadowProxyComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetNumMaterials() const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	//~ End UPrimitiveComponent Interface

	/**
	 * @brief Update the fluid bounds and trigger proxy update.
	 * @param NewBounds New world-space bounds for the fluid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Fluid Shadow")
	void SetFluidBounds(const FBox& NewBounds);

	/**
	 * @brief Set the density grid texture for ray marching.
	 * @param InDensityGrid Pointer to the density grid.
	 */
	void SetDensityGrid(FFluidDensityGrid* InDensityGrid);

	/**
	 * @brief Get the current density grid configuration.
	 * @return Current grid config.
	 */
	const FFluidDensityGridConfig& GetDensityGridConfig() const { return DensityGridConfig; }

	/**
	 * @brief Get the pooled render target for the density grid.
	 * @return Pooled render target or nullptr.
	 */
	TRefCountPtr<IPooledRenderTarget> GetDensityGridRT() const;

	/** Enable/disable shadow casting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow")
	bool bEnableShadowCasting = true;

	/** Surface density threshold for ray marching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SurfaceDensityThreshold = 0.5f;

	/** Maximum ray march steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow", meta = (ClampMin = "8", ClampMax = "256"))
	int32 MaxRayMarchSteps = 64;

	/**
	 * @brief Base material for shadow rendering.
	 *
	 * This material should be set up with:
	 * - Blend Mode: Masked
	 * - Shading Model: Unlit (for shadow only)
	 * - Two Sided: true (for volumetric shadows)
	 *
	 * Required material parameters (set via MID):
	 * - FluidHeightMap (Texture2D): Top-down height map of fluid surface
	 * - FluidBoundsMin (Vector): World-space minimum bounds
	 * - FluidBoundsMax (Vector): World-space maximum bounds
	 * - SurfaceThreshold (Scalar): Density threshold for opacity
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Shadow|Material")
	TObjectPtr<UMaterialInterface> ShadowMaterialBase;

	/**
	 * @brief Get the dynamic material instance used for shadow rendering.
	 * @return Material Instance Dynamic or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Fluid Shadow|Material")
	UMaterialInstanceDynamic* GetShadowMaterialInstance() const { return ShadowMaterialInstance; }

	/**
	 * @brief Update material parameters from current fluid state.
	 * Called automatically when density grid or bounds change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Fluid Shadow|Material")
	void UpdateMaterialParameters();

protected:
	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	//~ End UActorComponent Interface

private:
	/** Create or update the dynamic material instance. */
	void CreateMaterialInstance();

	/** Current fluid bounds in local space. */
	UPROPERTY()
	FBox LocalFluidBounds;

	/** Density grid configuration. */
	FFluidDensityGridConfig DensityGridConfig;

	/** Weak pointer to density grid (owned by fluid system). */
	FFluidDensityGrid* DensityGrid = nullptr;

	/** Scene proxy (render thread only). */
	FFluidShadowSceneProxy* ShadowProxy = nullptr;

	/** Dynamic material instance for shadow rendering. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShadowMaterialInstance;

	friend class FFluidShadowSceneProxy;
};
