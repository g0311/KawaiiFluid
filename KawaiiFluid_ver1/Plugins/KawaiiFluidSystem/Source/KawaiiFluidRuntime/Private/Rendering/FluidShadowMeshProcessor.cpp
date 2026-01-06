// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidShadowMeshProcessor.h"
#include "Rendering/FluidShadowSceneProxy.h"
#include "Rendering/FluidShadowVertexFactory.h"
#include "Rendering/FluidVSMDepthShader.h"
#include "MeshPassProcessor.inl"
#include "ScenePrivate.h"
#include "SceneRendering.h"

// Include for shadow depth pass
#include "ShadowRendering.h"

/**
 * @brief Construct the mesh processor.
 * @param InScene Scene being rendered.
 * @param InFeatureLevel RHI feature level.
 * @param InViewIfDynamicMeshCommand View for dynamic mesh commands.
 * @param InPassDrawRenderState Pass draw render state.
 * @param InDrawListContext Draw list context.
 */
FFluidShadowMeshProcessor::FFluidShadowMeshProcessor(
	const FScene* InScene,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::Num, InScene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
{
}

// GetFluidShadowProxyTypeHash() is defined in FluidShadowSceneProxy.cpp

/**
 * @brief Check if this processor can handle the given proxy.
 * @param PrimitiveSceneProxy Proxy to check.
 * @return True if this is a fluid shadow proxy.
 */
bool FFluidShadowMeshProcessor::IsFluidShadowProxy(const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
	if (!PrimitiveSceneProxy)
	{
		return false;
	}

	// Use GetTypeHash() to identify fluid shadow proxy (no RTTI needed)
	return PrimitiveSceneProxy->GetTypeHash() == GetFluidShadowProxyTypeHash();
}

/**
 * @brief Add mesh batch for processing.
 * @param MeshBatch Mesh batch to process.
 * @param BatchElementMask Element mask.
 * @param PrimitiveSceneProxy Scene proxy.
 * @param StaticMeshId Static mesh ID.
 */
void FFluidShadowMeshProcessor::AddMeshBatch(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	// Check if this is a fluid shadow proxy using type hash
	if (!IsFluidShadowProxy(PrimitiveSceneProxy))
	{
		return;
	}

	// Safe to static_cast after type hash check
	const FFluidShadowSceneProxy* FluidProxy = static_cast<const FFluidShadowSceneProxy*>(PrimitiveSceneProxy);

	// Validate the proxy has valid data
	if (!FluidProxy->IsDynamicDataValid())
	{
		return;
	}

	ProcessFluidShadowMesh(MeshBatch, BatchElementMask, FluidProxy, StaticMeshId);
}

/**
 * @brief Process a fluid shadow mesh batch.
 * @param MeshBatch Mesh batch.
 * @param BatchElementMask Element mask.
 * @param FluidProxy Fluid shadow proxy.
 * @param StaticMeshId Static mesh ID.
 */
void FFluidShadowMeshProcessor::ProcessFluidShadowMesh(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FFluidShadowSceneProxy* FluidProxy,
	int32 StaticMeshId)
{
	// Note: In a full implementation, we would:
	// 1. Get the shadow depth shaders (FFluidVSMDepthVS, FFluidVSMDepthPS)
	// 2. Create mesh draw commands with proper bindings
	// 3. Bind the density grid texture and parameters

	// For now, this is a placeholder that shows the structure.
	// The actual integration with UE5's shadow system requires
	// hooking into FShadowDepthPassMeshProcessor or similar.

	// This processor would typically be registered as an extension
	// to the shadow depth pass processor, not as a standalone processor.
}

//=============================================================================
// Factory Functions
//=============================================================================

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
		FMeshPassDrawListContext* DrawListContext)
	{
		FMeshPassProcessorRenderState PassDrawRenderState;

		// Setup render state for shadow depth
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_LessEqual>::GetRHI());
		PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());

		return new FFluidShadowMeshProcessor(
			Scene,
			Scene->GetFeatureLevel(),
			View,
			PassDrawRenderState,
			DrawListContext);
	}

	// Note: Registration of custom mesh pass processors in UE5 requires
	// modifying the engine's mesh pass processor registration system.
	// For a plugin, the typical approach is to:
	// 1. Use a custom render pass that runs after shadow passes
	// 2. Or inject into the shadow pass via FSceneRenderer extension

	/**
	 * @brief Register the mesh processor with the engine.
	 */
	void RegisterMeshProcessor()
	{
		// In UE5, mesh pass processors are typically registered via
		// IMPLEMENT_SHADERPIPELINE_TYPE_* macros in engine code.
		// For a plugin, we need alternative approaches like:
		// - Overriding FSceneRenderer::RenderShadowDepthMaps
		// - Using FSceneViewExtension for custom passes
		// - Hooking into the render graph via FSceneViewExtension

		// This function would be called from the module's StartupModule()
	}

	/**
	 * @brief Unregister the mesh processor from the engine.
	 */
	void UnregisterMeshProcessor()
	{
		// Cleanup registered processors
		// Called from module's ShutdownModule()
	}
}
