// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidShadowProxyComponent.h"
#include "Rendering/FluidShadowSceneProxy.h"
#include "Rendering/FluidDensityGrid.h"
#include "Materials/MaterialInstanceDynamic.h"

UFluidShadowProxyComponent::UFluidShadowProxyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Setup as shadow-casting invisible primitive
	PrimaryComponentTick.bCanEverTick = false;

	// Shadow settings
	CastShadow = true;
	bCastDynamicShadow = true;
	bCastStaticShadow = false;
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;

	// IMPORTANT: Hidden in game but still casts shadow
	// This is the standard UE way to make shadow-only geometry
	SetHiddenInGame(true);
	bCastHiddenShadow = true;

	// Visibility settings
	SetVisibility(true);  // Must be visible for shadow casting to work
	bVisibleInReflectionCaptures = false;
	bVisibleInRealTimeSkyCaptures = false;
	bVisibleInRayTracing = false;

	// Collision settings
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// Initialize bounds
	LocalFluidBounds = FBox(FVector(-50.0f), FVector(50.0f));
}

/**
 * @brief Create the scene proxy for rendering.
 * @return New scene proxy or nullptr.
 */
FPrimitiveSceneProxy* UFluidShadowProxyComponent::CreateSceneProxy()
{
	if (!bEnableShadowCasting)
	{
		return nullptr;
	}

	FFluidShadowSceneProxy* NewProxy = new FFluidShadowSceneProxy(this);
	ShadowProxy = NewProxy;
	return NewProxy;
}

/**
 * @brief Calculate bounds from the fluid bounds.
 * @param LocalToWorld Transform matrix.
 * @return Bounding sphere and box.
 */
FBoxSphereBounds UFluidShadowProxyComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox WorldBounds = LocalFluidBounds.TransformBy(LocalToWorld);
	return FBoxSphereBounds(WorldBounds);
}

/**
 * @brief Get used materials.
 * @param OutMaterials Output array.
 * @param bGetDebugMaterials Include debug materials.
 */
void UFluidShadowProxyComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (ShadowMaterialInstance)
	{
		OutMaterials.Add(ShadowMaterialInstance);
	}
	else if (ShadowMaterialBase)
	{
		OutMaterials.Add(ShadowMaterialBase);
	}
}

/**
 * @brief Get material at index.
 * @param ElementIndex Material index.
 * @return Material interface or nullptr.
 */
UMaterialInterface* UFluidShadowProxyComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0)
	{
		if (ShadowMaterialInstance)
		{
			return ShadowMaterialInstance;
		}
		return ShadowMaterialBase;
	}
	return nullptr;
}

/**
 * @brief Get number of materials.
 * @return 1 if material assigned, 0 otherwise.
 */
int32 UFluidShadowProxyComponent::GetNumMaterials() const
{
	return (ShadowMaterialBase || ShadowMaterialInstance) ? 1 : 0;
}

/**
 * @brief Set material at index.
 * @param ElementIndex Material index (only 0 supported).
 * @param Material Material to set as base.
 */
void UFluidShadowProxyComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	if (ElementIndex == 0)
	{
		ShadowMaterialBase = Material;
		CreateMaterialInstance();
		MarkRenderStateDirty();
	}
}

/**
 * @brief Update the fluid bounds.
 * @param NewBounds New world-space bounds.
 */
void UFluidShadowProxyComponent::SetFluidBounds(const FBox& NewBounds)
{
	if (!NewBounds.IsValid)
	{
		return;
	}

	// Calculate center and size from world bounds
	FVector BoundsCenter = NewBounds.GetCenter();
	FVector BoundsSize = NewBounds.GetSize();

	// Set the component's world transform so the unit cube mesh maps to fluid bounds
	// Unit cube is -0.5 to +0.5, so we scale by BoundsSize and translate to BoundsCenter
	SetWorldLocation(BoundsCenter);
	SetWorldScale3D(BoundsSize);

	// Store bounds in local space (unit cube after transform)
	LocalFluidBounds = FBox(FVector(-0.5f), FVector(0.5f));

	// Update density grid config with world bounds
	DensityGridConfig.WorldBoundsMin = NewBounds.Min;
	DensityGridConfig.WorldBoundsMax = NewBounds.Max;

	// Update material parameters
	UpdateMaterialParameters();

	// Mark bounds dirty - this triggers CalcBounds recalculation
	UpdateBounds();
	MarkRenderTransformDirty();
	MarkRenderDynamicDataDirty();

	UE_LOG(LogTemp, Verbose, TEXT("VSM Proxy: SetFluidBounds - Center: %s, Size: %s"),
		*BoundsCenter.ToString(), *BoundsSize.ToString());
}

/**
 * @brief Set the density grid for ray marching.
 * @param InDensityGrid Pointer to density grid.
 */
void UFluidShadowProxyComponent::SetDensityGrid(FFluidDensityGrid* InDensityGrid)
{
	DensityGrid = InDensityGrid;

	if (DensityGrid)
	{
		DensityGridConfig = DensityGrid->GetConfig();
	}

	// Update material parameters
	UpdateMaterialParameters();

	MarkRenderDynamicDataDirty();
}

/**
 * @brief Get the pooled render target for the density grid.
 * @return Pooled render target or nullptr.
 */
TRefCountPtr<IPooledRenderTarget> UFluidShadowProxyComponent::GetDensityGridRT() const
{
	if (DensityGrid)
	{
		return DensityGrid->GetPooledRenderTarget();
	}
	return nullptr;
}

/**
 * @brief Called when component is registered.
 */
void UFluidShadowProxyComponent::OnRegister()
{
	Super::OnRegister();

	// Create material instance if base material is set
	CreateMaterialInstance();
}

/**
 * @brief Called when component is unregistered.
 */
void UFluidShadowProxyComponent::OnUnregister()
{
	ShadowProxy = nullptr;
	Super::OnUnregister();
}

/**
 * @brief Send dynamic data to render thread.
 */
void UFluidShadowProxyComponent::SendRenderDynamicData_Concurrent()
{
	if (!ShadowProxy)
	{
		return;
	}

	// Prepare dynamic data
	FFluidShadowProxyDynamicData NewData;
	NewData.DensityGridConfig = DensityGridConfig;
	NewData.DensityGridRT = GetDensityGridRT();
	NewData.SurfaceDensityThreshold = SurfaceDensityThreshold;
	NewData.MaxRayMarchSteps = MaxRayMarchSteps;
	NewData.bIsValid = (NewData.DensityGridRT.IsValid());

	// Get material render proxy
	if (ShadowMaterialInstance)
	{
		NewData.MaterialRenderProxy = ShadowMaterialInstance->GetRenderProxy();
	}
	else if (ShadowMaterialBase)
	{
		NewData.MaterialRenderProxy = ShadowMaterialBase->GetRenderProxy();
	}

	// Send to render thread
	FFluidShadowSceneProxy* Proxy = ShadowProxy;
	ENQUEUE_RENDER_COMMAND(FFluidShadowProxyUpdateDynamicData)(
		[Proxy, NewData](FRHICommandListImmediate& RHICmdList)
		{
			Proxy->SetDynamicData_RenderThread(NewData);
		});
}

/**
 * @brief Create or update the dynamic material instance.
 */
void UFluidShadowProxyComponent::CreateMaterialInstance()
{
	if (!ShadowMaterialBase)
	{
		ShadowMaterialInstance = nullptr;
		return;
	}

	// Create MID from base material
	if (!ShadowMaterialInstance || ShadowMaterialInstance->Parent != ShadowMaterialBase)
	{
		ShadowMaterialInstance = UMaterialInstanceDynamic::Create(ShadowMaterialBase, this);
	}

	// Initialize parameters
	UpdateMaterialParameters();
}

/**
 * @brief Update material parameters from current fluid state.
 */
void UFluidShadowProxyComponent::UpdateMaterialParameters()
{
	if (!ShadowMaterialInstance)
	{
		return;
	}

	// Set bounds parameters
	ShadowMaterialInstance->SetVectorParameterValue(
		TEXT("FluidBoundsMin"),
		FLinearColor(DensityGridConfig.WorldBoundsMin.X, DensityGridConfig.WorldBoundsMin.Y, DensityGridConfig.WorldBoundsMin.Z, 0.0f));

	ShadowMaterialInstance->SetVectorParameterValue(
		TEXT("FluidBoundsMax"),
		FLinearColor(DensityGridConfig.WorldBoundsMax.X, DensityGridConfig.WorldBoundsMax.Y, DensityGridConfig.WorldBoundsMax.Z, 0.0f));

	// Set grid size for normalization
	FVector GridSize = DensityGridConfig.GetWorldSize();
	ShadowMaterialInstance->SetVectorParameterValue(
		TEXT("FluidGridSize"),
		FLinearColor(GridSize.X, GridSize.Y, GridSize.Z, 0.0f));

	// Set threshold parameter
	ShadowMaterialInstance->SetScalarParameterValue(
		TEXT("SurfaceThreshold"),
		SurfaceDensityThreshold);

	// Note: The density texture (3D) must be set via custom node in material
	// as UE5 materials don't natively support 3D texture parameters.
	// For now, use a 2D height map approximation for shadow casting.
}
