// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "PrimitiveSceneProxy.h"
#include "Rendering/FluidDensityGrid.h"
#include "RenderTargetPool.h"

class UFluidShadowProxyComponent;

/** Get the type hash for FFluidShadowSceneProxy identification. */
SIZE_T GetFluidShadowProxyTypeHash();

/**
 * @brief Dynamic data passed from game thread to render thread.
 * @param DensityGridConfig Current density grid configuration.
 * @param DensityGridRT Pooled render target for density grid.
 * @param MaterialRenderProxy Material proxy for shadow rendering.
 */
struct FFluidShadowProxyDynamicData
{
	FFluidDensityGridConfig DensityGridConfig;
	TRefCountPtr<IPooledRenderTarget> DensityGridRT;
	float SurfaceDensityThreshold = 0.5f;
	int32 MaxRayMarchSteps = 64;
	bool bIsValid = false;
	FMaterialRenderProxy* MaterialRenderProxy = nullptr;
};

/**
 * @brief Scene proxy for fluid shadow rendering.
 *
 * This proxy renders an invisible bounding box mesh that participates in
 * shadow depth passes. The pixel shader performs ray marching against
 * the density grid to compute the actual fluid depth.
 *
 * Key features:
 * - Participates in both CSM and VSM shadow passes
 * - Uses SV_Depth output to override rasterized depth with ray-marched depth
 * - Supports dynamic updates via SendRenderDynamicData
 */
class FFluidShadowSceneProxy : public FPrimitiveSceneProxy
{
public:
	/**
	 * @brief Construct the scene proxy.
	 * @param InComponent Owner component.
	 */
	FFluidShadowSceneProxy(const UFluidShadowProxyComponent* InComponent);

	virtual ~FFluidShadowSceneProxy();

	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override { return false; }
	virtual bool IsUsingDistanceCullFade() const override { return false; }
	//~ End FPrimitiveSceneProxy Interface

	/**
	 * @brief Update dynamic data from game thread.
	 * @param NewData New dynamic data.
	 */
	void SetDynamicData_RenderThread(const FFluidShadowProxyDynamicData& NewData);

	/** Get the density grid configuration. */
	const FFluidDensityGridConfig& GetDensityGridConfig() const { return DynamicData.DensityGridConfig; }

	/** Get the density grid render target. */
	TRefCountPtr<IPooledRenderTarget> GetDensityGridRT() const { return DynamicData.DensityGridRT; }

	/** Get surface density threshold. */
	float GetSurfaceDensityThreshold() const { return DynamicData.SurfaceDensityThreshold; }

	/** Get max ray march steps. */
	int32 GetMaxRayMarchSteps() const { return DynamicData.MaxRayMarchSteps; }

	/** Check if dynamic data is valid. */
	bool IsDynamicDataValid() const { return DynamicData.bIsValid; }

private:
	/** Create the box mesh vertices and indices. */
	void CreateBoxMesh();

	/** Dynamic data (updated from game thread). */
	FFluidShadowProxyDynamicData DynamicData;

	/** Local bounds of the proxy box. */
	FBox LocalBounds;

	/** Box vertex buffer. */
	FStaticMeshVertexBuffers VertexBuffers;

	/** Box index buffer. */
	FDynamicMeshIndexBuffer32 IndexBuffer;

	/** Vertex factory for rendering. */
	FLocalVertexFactory VertexFactory;

	/** Material render proxy (may be null for shadow-only). */
	FMaterialRenderProxy* MaterialProxy = nullptr;

	/** Cached material relevance for rendering decisions. */
	FMaterialRelevance MaterialRelevance;
};
