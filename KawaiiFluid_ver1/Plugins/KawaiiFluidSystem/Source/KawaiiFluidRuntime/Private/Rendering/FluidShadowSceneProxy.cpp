// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidShadowSceneProxy.h"
#include "Rendering/FluidShadowProxyComponent.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"

/**
 * @brief Get the type hash for FFluidShadowSceneProxy identification.
 * @return Unique type hash.
 */
SIZE_T GetFluidShadowProxyTypeHash()
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

/**
 * @brief Construct the scene proxy.
 * @param InComponent Owner component.
 */
FFluidShadowSceneProxy::FFluidShadowSceneProxy(const UFluidShadowProxyComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, VertexFactory(GetScene().GetFeatureLevel(), "FFluidShadowSceneProxy")
{
	// Copy initial data from component
	LocalBounds = FBox(FVector(-50.0f), FVector(50.0f));

	// Initialize dynamic data
	DynamicData.DensityGridConfig = InComponent->GetDensityGridConfig();
	DynamicData.DensityGridRT = InComponent->GetDensityGridRT();
	DynamicData.SurfaceDensityThreshold = InComponent->SurfaceDensityThreshold;
	DynamicData.MaxRayMarchSteps = InComponent->MaxRayMarchSteps;
	DynamicData.bIsValid = DynamicData.DensityGridRT.IsValid();

	// Get material render proxy and relevance from component
	UMaterialInterface* ShadowMaterial = InComponent->GetMaterial(0);
	if (ShadowMaterial)
	{
		MaterialProxy = ShadowMaterial->GetRenderProxy();
		DynamicData.MaterialRenderProxy = MaterialProxy;
		MaterialRelevance = ShadowMaterial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	}

	// Set flags for shadow casting
	bCastDynamicShadow = true;
	bAffectDynamicIndirectLighting = false;

	// Verify materials if we have one
	bVerifyUsedMaterials = (MaterialProxy != nullptr);

	// Create box mesh geometry
	CreateBoxMesh();
}

FFluidShadowSceneProxy::~FFluidShadowSceneProxy()
{
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
}

/**
 * @brief Get type hash for proxy type identification.
 * @return Type hash.
 */
SIZE_T FFluidShadowSceneProxy::GetTypeHash() const
{
	return GetFluidShadowProxyTypeHash();
}

/**
 * @brief Get memory footprint of this proxy.
 * @return Memory size in bytes.
 */
uint32 FFluidShadowSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

/**
 * @brief Get view relevance for culling and rendering decisions.
 * @param View Scene view.
 * @return View relevance flags.
 */
FPrimitiveViewRelevance FFluidShadowSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;

	const bool bHasMaterial = (MaterialProxy != nullptr);

	// Apply cached material relevance if we have a material
	if (bHasMaterial)
	{
		// Use cached material relevance to properly set bOpaque, bMasked, bTranslucent flags
		MaterialRelevance.SetPrimitiveViewRelevance(Relevance);
	}

	// Shadow-only proxy: cast shadows but don't render in main pass
	Relevance.bDrawRelevance = true;  // Must be true for shadow pass to work
	Relevance.bShadowRelevance = true;
	Relevance.bDynamicRelevance = true;
	Relevance.bStaticRelevance = false;
	Relevance.bRenderInMainPass = false;  // Don't render the box in main pass
	Relevance.bRenderInDepthPass = false; // Don't render in depth prepass either
	Relevance.bRenderCustomDepth = false;
	Relevance.bUsesLightingChannels = false;
	Relevance.bTranslucentSelfShadow = false;
	Relevance.bVelocityRelevance = false;

	return Relevance;
}

/**
 * @brief Get dynamic mesh elements for rendering.
 * @param Views Array of scene views.
 * @param ViewFamily View family.
 * @param VisibilityMap Visibility bits per view.
 * @param Collector Mesh element collector.
 */
void FFluidShadowSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	// Debug: Log when this is called (throttled)
	static int32 FrameCounter = 0;
	if (++FrameCounter % 300 == 1)  // Log every ~5 seconds at 60fps
	{
		FVector Scale = GetLocalToWorld().GetScaleVector();
		FVector Location = GetLocalToWorld().GetOrigin();
		UE_LOG(LogTemp, Log, TEXT("VSM Proxy: GetDynamicMeshElements - HasMaterial: %d, Views: %d, VisMap: %u, VF_Init: %d, Bounds: %s, Scale: %s, Loc: %s"),
			MaterialProxy != nullptr, Views.Num(), VisibilityMap,
			VertexFactory.IsInitialized(),
			*GetBounds().GetBox().ToString(),
			*Scale.ToString(),
			*Location.ToString());
	}

	// Skip if vertex factory not ready
	if (!VertexFactory.IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("VSM Proxy: VertexFactory not initialized yet!"));
		return;
	}

	// Use component's material if available, otherwise default material
	FMaterialRenderProxy* UseMaterialProxy = MaterialProxy;
	if (!UseMaterialProxy)
	{
		// Fallback to default material for shadow depth
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		if (!DefaultMaterial)
		{
			UE_LOG(LogTemp, Warning, TEXT("VSM Proxy: No material available!"));
			return;
		}
		UseMaterialProxy = DefaultMaterial->GetRenderProxy();
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}

		const FSceneView* View = Views[ViewIndex];

		// Create mesh batch
		FMeshBatch& MeshBatch = Collector.AllocateMesh();

		MeshBatch.VertexFactory = &VertexFactory;
		MeshBatch.MaterialRenderProxy = UseMaterialProxy;
		MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		MeshBatch.Type = PT_TriangleList;
		MeshBatch.DepthPriorityGroup = SDPG_World;
		MeshBatch.bCanApplyViewModeOverrides = false;
		MeshBatch.bUseForMaterial = true;
		MeshBatch.bUseForDepthPass = true;
		MeshBatch.bUseAsOccluder = true;  // Required for shadow occlusion
		MeshBatch.CastShadow = true;
		MeshBatch.bUseWireframeSelectionColoring = false;

		// Setup mesh batch element
		FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = 12;  // 6 faces * 2 triangles
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = 7;  // 8 vertices (0-7)

		// Set primitive uniform buffer - required for rendering
		BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

		Collector.AddMesh(ViewIndex, MeshBatch);
	}
}

/**
 * @brief Update dynamic data from game thread.
 * @param NewData New dynamic data.
 */
void FFluidShadowSceneProxy::SetDynamicData_RenderThread(const FFluidShadowProxyDynamicData& NewData)
{
	check(IsInRenderingThread());
	DynamicData = NewData;

	// Update material proxy if changed
	if (NewData.MaterialRenderProxy)
	{
		MaterialProxy = NewData.MaterialRenderProxy;
	}
}

/**
 * @brief Create the bounding box mesh geometry.
 */
void FFluidShadowSceneProxy::CreateBoxMesh()
{
	// Create a unit cube (-0.5 to 0.5), will be transformed by LocalToWorld

	// 8 vertices of a cube
	TArray<FDynamicMeshVertex> Vertices;
	Vertices.SetNum(8);

	const float HalfSize = 0.5f;
	const FVector3f Corners[8] = {
		FVector3f(-HalfSize, -HalfSize, -HalfSize),  // 0: ---
		FVector3f(+HalfSize, -HalfSize, -HalfSize),  // 1: +--
		FVector3f(+HalfSize, +HalfSize, -HalfSize),  // 2: ++-
		FVector3f(-HalfSize, +HalfSize, -HalfSize),  // 3: -+-
		FVector3f(-HalfSize, -HalfSize, +HalfSize),  // 4: --+
		FVector3f(+HalfSize, -HalfSize, +HalfSize),  // 5: +-+
		FVector3f(+HalfSize, +HalfSize, +HalfSize),  // 6: +++
		FVector3f(-HalfSize, +HalfSize, +HalfSize),  // 7: -++
	};

	for (int32 i = 0; i < 8; i++)
	{
		Vertices[i].Position = Corners[i];
		Vertices[i].TextureCoordinate[0] = FVector2f(0.0f, 0.0f);
		Vertices[i].TangentX = FVector3f(1.0f, 0.0f, 0.0f);
		Vertices[i].TangentZ = FVector3f(0.0f, 0.0f, 1.0f);
		Vertices[i].Color = FColor::White;
	}

	// 36 indices for 12 triangles (6 faces * 2 triangles)
	TArray<uint32> Indices = {
		// Front face (-Z)
		0, 1, 2, 0, 2, 3,
		// Back face (+Z)
		5, 4, 7, 5, 7, 6,
		// Left face (-X)
		4, 0, 3, 4, 3, 7,
		// Right face (+X)
		1, 5, 6, 1, 6, 2,
		// Bottom face (-Y)
		4, 5, 1, 4, 1, 0,
		// Top face (+Y)
		3, 2, 6, 3, 6, 7,
	};

	// Initialize vertex buffers
	VertexBuffers.PositionVertexBuffer.Init(Vertices.Num());
	VertexBuffers.StaticMeshVertexBuffer.Init(Vertices.Num(), 1);
	VertexBuffers.ColorVertexBuffer.Init(Vertices.Num());

	for (int32 i = 0; i < Vertices.Num(); i++)
	{
		VertexBuffers.PositionVertexBuffer.VertexPosition(i) = Vertices[i].Position;
		FVector3f TangentX = Vertices[i].TangentX.ToFVector3f();
		FVector3f TangentZ = Vertices[i].TangentZ.ToFVector3f();
		VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(
			i,
			TangentX,
			FVector3f::CrossProduct(TangentZ, TangentX),
			TangentZ);
		VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertices[i].TextureCoordinate[0]);
		VertexBuffers.ColorVertexBuffer.VertexColor(i) = Vertices[i].Color;
	}

	// Initialize index buffer
	IndexBuffer.Indices = Indices;

	// All RHI resource initialization must happen on Render Thread in correct order
	FStaticMeshVertexBuffers* VertexBuffersPtr = &VertexBuffers;
	FDynamicMeshIndexBuffer32* IndexBufferPtr = &IndexBuffer;
	FLocalVertexFactory* VertexFactoryPtr = &VertexFactory;

	ENQUEUE_RENDER_COMMAND(FFluidShadowProxyInitResources)(
		[VertexBuffersPtr, IndexBufferPtr, VertexFactoryPtr](FRHICommandListImmediate& RHICmdList)
		{
			// 1. Initialize vertex buffers first
			VertexBuffersPtr->PositionVertexBuffer.InitResource(RHICmdList);
			VertexBuffersPtr->StaticMeshVertexBuffer.InitResource(RHICmdList);
			VertexBuffersPtr->ColorVertexBuffer.InitResource(RHICmdList);
			IndexBufferPtr->InitResource(RHICmdList);

			// 2. Bind buffers to vertex factory data
			FLocalVertexFactory::FDataType Data;
			VertexBuffersPtr->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactoryPtr, Data);
			VertexBuffersPtr->StaticMeshVertexBuffer.BindTangentVertexBuffer(VertexFactoryPtr, Data);
			VertexBuffersPtr->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactoryPtr, Data);
			VertexBuffersPtr->ColorVertexBuffer.BindColorVertexBuffer(VertexFactoryPtr, Data);

			// 3. Set data and initialize vertex factory
			VertexFactoryPtr->SetData(RHICmdList, Data);
			VertexFactoryPtr->InitResource(RHICmdList);
		});
}
