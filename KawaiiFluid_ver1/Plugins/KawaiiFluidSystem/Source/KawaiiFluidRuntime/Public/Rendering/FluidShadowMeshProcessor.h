// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshPassProcessor.h"
#include "MeshMaterialShader.h"
#include "Rendering/FluidDensityGrid.h"

class FFluidShadowSceneProxy;

/**
 * @brief Mesh pass processor for fluid shadow depth rendering.
 *
 * This processor handles fluid shadow proxy meshes in shadow depth passes,
 * using custom shaders that perform ray marching against the density grid
 * to compute accurate fluid depth.
 *
 * The processor intercepts fluid proxy meshes and:
 * 1. Binds the density grid texture and parameters
 * 2. Uses custom vertex/pixel shaders with SV_Depth output
 * 3. Integrates with both CSM and VSM shadow systems
 */
class KAWAIIFLUIDRUNTIME_API FFluidShadowMeshProcessor : public FMeshPassProcessor
{
public:
	/**
	 * @brief Construct the mesh processor.
	 * @param InScene Scene being rendered.
	 * @param InFeatureLevel RHI feature level.
	 * @param InViewIfDynamicMeshCommand View for dynamic mesh commands.
	 * @param InPassDrawRenderState Pass draw render state.
	 * @param InDrawListContext Draw list context.
	 */
	FFluidShadowMeshProcessor(
		const FScene* InScene,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext);

	/**
	 * @brief Add mesh batch for processing.
	 * @param MeshBatch Mesh batch to process.
	 * @param BatchElementMask Element mask.
	 * @param PrimitiveSceneProxy Scene proxy.
	 * @param StaticMeshId Static mesh ID.
	 * @param MaterialRenderProxy Material proxy.
	 * @param MaterialResource Material resource.
	 * @param ShadowMapTextureResolution Shadow map resolution.
	 * @param LightSceneInfo Light info.
	 * @param ShadowDepthBias Shadow depth bias.
	 * @param bSinglePassPointLightShadow Point light shadow flag.
	 */
	virtual void AddMeshBatch(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		int32 StaticMeshId = -1) override;

	/**
	 * @brief Check if this processor can handle the given proxy.
	 * @param PrimitiveSceneProxy Proxy to check.
	 * @return True if this is a fluid shadow proxy.
	 */
	static bool IsFluidShadowProxy(const FPrimitiveSceneProxy* PrimitiveSceneProxy);

private:
	/**
	 * @brief Process a fluid shadow mesh batch.
	 * @param MeshBatch Mesh batch.
	 * @param BatchElementMask Element mask.
	 * @param FluidProxy Fluid shadow proxy.
	 * @param StaticMeshId Static mesh ID.
	 */
	void ProcessFluidShadowMesh(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FFluidShadowSceneProxy* FluidProxy,
		int32 StaticMeshId);

	/** Pass draw render state. */
	FMeshPassProcessorRenderState PassDrawRenderState;
};

/**
 * @brief Factory functions for creating fluid shadow mesh processor.
 */
namespace FluidShadowMeshProcessorFactory
{
	/**
	 * @brief Create mesh processor for shadow depth pass.
	 * @param Scene Scene.
	 * @param View Scene view.
	 * @param DrawListContext Draw list context.
	 * @return New mesh processor.
	 */
	FMeshPassProcessor* CreateShadowDepthPassProcessor(
		const FScene* Scene,
		const FSceneView* View,
		FMeshPassDrawListContext* DrawListContext);

	/**
	 * @brief Register the mesh processor with the engine.
	 * Called during module startup.
	 */
	void RegisterMeshProcessor();

	/**
	 * @brief Unregister the mesh processor from the engine.
	 * Called during module shutdown.
	 */
	void UnregisterMeshProcessor();
}
