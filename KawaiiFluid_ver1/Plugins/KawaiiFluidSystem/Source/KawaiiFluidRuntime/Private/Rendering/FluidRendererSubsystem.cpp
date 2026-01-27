// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/FluidRendererSubsystem.h"
#include "Rendering/FluidSceneViewExtension.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Async/ParallelFor.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

bool UFluidRendererSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!IsValid(Outer))
	{
		return false;
	}

	UWorld* World = CastChecked<UWorld>(Outer);
	const EWorldType::Type WorldType = World->WorldType;

	// Support all world types that might need fluid rendering
	// Including EditorPreview for Preset Editor viewport
	return WorldType == EWorldType::Game ||
	       WorldType == EWorldType::Editor ||
	       WorldType == EWorldType::PIE ||
	       WorldType == EWorldType::EditorPreview ||
	       WorldType == EWorldType::GamePreview;
}

void UFluidRendererSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create and register Scene View Extension
	ViewExtension = FSceneViewExtensions::NewExtension<FFluidSceneViewExtension>(this);

	UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem Initialized"));
}

void UFluidRendererSubsystem::Deinitialize()
{
	// Cleanup shadow resources
	CleanupShadowResources();

	// Release View Extension
	ViewExtension.Reset();

	RegisteredRenderingModules.Empty();

	Super::Deinitialize();

	UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem Deinitialized"));
}

//========================================
// FTickableGameObject Implementation
//========================================

TStatId UFluidRendererSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFluidRendererSubsystem, STATGROUP_Tickables);
}

/**
 * @brief Subsystem tick - flushes aggregated shadow particles to ISM components.
 * Called after all Volume/Component ticks have registered their particles.
 */
void UFluidRendererSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FluidRendererSubsystem_Tick);

	if (!bEnableISMShadow)
	{
		return;
	}

	// Flush all aggregated particles to ISM components
	FlushShadowInstances();

	// Clear buffers for next frame
	ClearAggregationBuffers();
}

//========================================
// RenderingModule management
//========================================

void UFluidRendererSubsystem::RegisterRenderingModule(UKawaiiFluidRenderingModule* Module)
{
	if (!Module)
	{
		UE_LOG(LogTemp, Warning, TEXT("FluidRendererSubsystem: RegisterRenderingModule - Module is null"));
		return;
	}

	if (RegisteredRenderingModules.Contains(Module))
	{
		UE_LOG(LogTemp, Warning, TEXT("FluidRendererSubsystem: RenderingModule already registered: %s"),
			*Module->GetName());
		return;
	}

	RegisteredRenderingModules.Add(Module);

	UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem: Registered RenderingModule %s (Total: %d)"),
		*Module->GetName(),
		RegisteredRenderingModules.Num());
}

void UFluidRendererSubsystem::UnregisterRenderingModule(UKawaiiFluidRenderingModule* Module)
{
	if (!Module)
	{
		return;
	}

	int32 Removed = RegisteredRenderingModules.Remove(Module);

	if (Removed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem: Unregistered RenderingModule %s (Remaining: %d)"),
			*Module->GetName(),
			RegisteredRenderingModules.Num());
	}
}

//========================================
// Low-Poly Sphere Generation (Icosphere + Octahedron)
//========================================

/**
 * @brief Create a low-poly sphere mesh for shadow casting.
 * Supports multiple quality levels with different geometry types.
 * @param Radius The radius of the sphere (default 50.0 to match engine sphere).
 * @param Quality The quality level determining geometry complexity.
 * @return The created static mesh, or nullptr on failure.
 */
static UStaticMesh* CreateLowPolySphere(float Radius, EFluidShadowMeshQuality Quality)
{
	// Create a transient static mesh
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!Mesh)
	{
		return nullptr;
	}

	// Initialize mesh description
	FMeshDescription MeshDesc;
	FStaticMeshAttributes Attributes(MeshDesc);
	Attributes.Register();

	// Create polygon group
	FPolygonGroupID PolygonGroup = MeshDesc.CreatePolygonGroup();

	// Get attribute arrays
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();

	// Vertex storage
	TArray<FVector3f> Vertices;
	TArray<int32> Indices;

	// Helper lambda to add vertex and return index
	auto AddVertex = [&](const FVector3f& Pos) -> int32
	{
		Vertices.Add(Pos.GetSafeNormal() * Radius);
		return Vertices.Num() - 1;
	};

	// Helper lambda to get midpoint vertex (for subdivision)
	TMap<uint64, int32> MidpointCache;
	auto GetMidpoint = [&](int32 I1, int32 I2) -> int32
	{
		uint64 Key = (uint64)FMath::Min(I1, I2) << 32 | (uint64)FMath::Max(I1, I2);
		if (int32* Found = MidpointCache.Find(Key))
		{
			return *Found;
		}
		FVector3f Mid = (Vertices[I1] + Vertices[I2]) * 0.5f;
		int32 NewIdx = AddVertex(Mid);
		MidpointCache.Add(Key, NewIdx);
		return NewIdx;
	};

	if (Quality == EFluidShadowMeshQuality::Low)
	{
		// Octahedron: 6 vertices, 8 triangles
		AddVertex(FVector3f(0, 0, 1));   // 0: Top
		AddVertex(FVector3f(0, 0, -1));  // 1: Bottom
		AddVertex(FVector3f(1, 0, 0));   // 2: Front
		AddVertex(FVector3f(-1, 0, 0));  // 3: Back
		AddVertex(FVector3f(0, 1, 0));   // 4: Right
		AddVertex(FVector3f(0, -1, 0));  // 5: Left

		// Top cap
		Indices.Append({0, 2, 4});
		Indices.Append({0, 4, 3});
		Indices.Append({0, 3, 5});
		Indices.Append({0, 5, 2});
		// Bottom cap
		Indices.Append({1, 4, 2});
		Indices.Append({1, 3, 4});
		Indices.Append({1, 5, 3});
		Indices.Append({1, 2, 5});
	}
	else
	{
		// Icosahedron base: 12 vertices, 20 triangles
		const float Phi = (1.0f + FMath::Sqrt(5.0f)) / 2.0f; // Golden ratio

		// 12 vertices of icosahedron
		AddVertex(FVector3f(-1, Phi, 0));
		AddVertex(FVector3f(1, Phi, 0));
		AddVertex(FVector3f(-1, -Phi, 0));
		AddVertex(FVector3f(1, -Phi, 0));

		AddVertex(FVector3f(0, -1, Phi));
		AddVertex(FVector3f(0, 1, Phi));
		AddVertex(FVector3f(0, -1, -Phi));
		AddVertex(FVector3f(0, 1, -Phi));

		AddVertex(FVector3f(Phi, 0, -1));
		AddVertex(FVector3f(Phi, 0, 1));
		AddVertex(FVector3f(-Phi, 0, -1));
		AddVertex(FVector3f(-Phi, 0, 1));

		// 20 faces of icosahedron
		Indices.Append({0, 11, 5});
		Indices.Append({0, 5, 1});
		Indices.Append({0, 1, 7});
		Indices.Append({0, 7, 10});
		Indices.Append({0, 10, 11});

		Indices.Append({1, 5, 9});
		Indices.Append({5, 11, 4});
		Indices.Append({11, 10, 2});
		Indices.Append({10, 7, 6});
		Indices.Append({7, 1, 8});

		Indices.Append({3, 9, 4});
		Indices.Append({3, 4, 2});
		Indices.Append({3, 2, 6});
		Indices.Append({3, 6, 8});
		Indices.Append({3, 8, 9});

		Indices.Append({4, 9, 5});
		Indices.Append({2, 4, 11});
		Indices.Append({6, 2, 10});
		Indices.Append({8, 6, 7});
		Indices.Append({9, 8, 1});

		// Subdivide for High quality (20 -> 80 triangles)
		if (Quality == EFluidShadowMeshQuality::High)
		{
			TArray<int32> NewIndices;
			NewIndices.Reserve(Indices.Num() * 4);

			for (int32 i = 0; i < Indices.Num(); i += 3)
			{
				int32 V0 = Indices[i];
				int32 V1 = Indices[i + 1];
				int32 V2 = Indices[i + 2];

				// Get midpoints
				int32 M01 = GetMidpoint(V0, V1);
				int32 M12 = GetMidpoint(V1, V2);
				int32 M20 = GetMidpoint(V2, V0);

				// Create 4 new triangles
				NewIndices.Append({V0, M01, M20});
				NewIndices.Append({V1, M12, M01});
				NewIndices.Append({V2, M20, M12});
				NewIndices.Append({M01, M12, M20});
			}

			Indices = MoveTemp(NewIndices);
		}
	}

	// Reserve mesh description space
	const int32 NumTriangles = Indices.Num() / 3;
	MeshDesc.ReserveNewVertices(Vertices.Num());
	MeshDesc.ReserveNewVertexInstances(Indices.Num());
	MeshDesc.ReserveNewPolygons(NumTriangles);

	// Create vertices in mesh description
	TArray<FVertexID> VertexIDs;
	VertexIDs.Reserve(Vertices.Num());
	for (const FVector3f& Pos : Vertices)
	{
		FVertexID VID = MeshDesc.CreateVertex();
		VertexPositions[VID] = Pos;
		VertexIDs.Add(VID);
	}

	// Create triangles
	for (int32 i = 0; i < Indices.Num(); i += 3)
	{
		FVertexID V0 = VertexIDs[Indices[i]];
		FVertexID V1 = VertexIDs[Indices[i + 1]];
		FVertexID V2 = VertexIDs[Indices[i + 2]];

		FVector3f P0 = VertexPositions[V0];
		FVector3f P1 = VertexPositions[V1];
		FVector3f P2 = VertexPositions[V2];

		FVertexInstanceID VI0 = MeshDesc.CreateVertexInstance(V0);
		FVertexInstanceID VI1 = MeshDesc.CreateVertexInstance(V1);
		FVertexInstanceID VI2 = MeshDesc.CreateVertexInstance(V2);

		// Sphere normal = normalized position
		VertexInstanceNormals[VI0] = P0.GetSafeNormal();
		VertexInstanceNormals[VI1] = P1.GetSafeNormal();
		VertexInstanceNormals[VI2] = P2.GetSafeNormal();

		// Simple UVs (not important for shadow mesh)
		VertexInstanceUVs[VI0] = FVector2f(0.5f, 0.5f);
		VertexInstanceUVs[VI1] = FVector2f(0.5f, 0.5f);
		VertexInstanceUVs[VI2] = FVector2f(0.5f, 0.5f);

		TArray<FVertexInstanceID> TriVerts;
		TriVerts.Add(VI0);
		TriVerts.Add(VI1);
		TriVerts.Add(VI2);
		MeshDesc.CreatePolygon(PolygonGroup, TriVerts);
	}

	// Build the static mesh
	TArray<const FMeshDescription*> MeshDescriptions;
	MeshDescriptions.Add(&MeshDesc);

	UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
	BuildParams.bFastBuild = true;
	BuildParams.bAllowCpuAccess = false;
	BuildParams.bCommitMeshDescription = false;

	Mesh->BuildFromMeshDescriptions(MeshDescriptions, BuildParams);

	return Mesh;
}

//========================================
// ISM Shadow Management (Multi-Volume Aggregation)
//========================================

/**
 * @brief Get or create shadow sphere mesh for specified quality level.
 * Uses lazy creation and caches meshes per quality level.
 * @param Quality The quality level for the shadow mesh.
 * @return The shadow sphere mesh, or nullptr on failure.
 */
UStaticMesh* UFluidRendererSubsystem::GetOrCreateShadowMesh(EFluidShadowMeshQuality Quality)
{
	const int32 QualityIndex = static_cast<int32>(Quality);
	if (QualityIndex < 0 || QualityIndex >= NUM_SHADOW_QUALITY_LEVELS)
	{
		return nullptr;
	}

	// Return cached mesh if valid
	if (IsValid(ShadowSphereMeshes[QualityIndex]))
	{
		return ShadowSphereMeshes[QualityIndex];
	}

	// Create new mesh for this quality level
	ShadowSphereMeshes[QualityIndex] = CreateLowPolySphere(50.0f, Quality);

	if (ShadowSphereMeshes[QualityIndex])
	{
		int32 TriCount = (Quality == EFluidShadowMeshQuality::Low) ? 8 :
		                 (Quality == EFluidShadowMeshQuality::Medium) ? 20 : 80;
		UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem: Created shadow sphere (Quality: %d, %d triangles)"),
			QualityIndex, TriCount);
	}

	return ShadowSphereMeshes[QualityIndex];
}

/**
 * @brief Get or create ISM component for specified quality level.
 * @param Quality The quality level for the ISM component.
 * @return The ISM component, or nullptr on failure.
 */
UInstancedStaticMeshComponent* UFluidRendererSubsystem::GetOrCreateShadowISM(EFluidShadowMeshQuality Quality)
{
	const int32 QualityIndex = static_cast<int32>(Quality);
	if (QualityIndex < 0 || QualityIndex >= NUM_SHADOW_QUALITY_LEVELS)
	{
		return nullptr;
	}

	// Return cached component if valid
	if (IsValid(ShadowInstanceComponents[QualityIndex]))
	{
		return ShadowInstanceComponents[QualityIndex];
	}

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || !World->bActorsInitialized)
	{
		return nullptr;
	}

	// Lazy creation of Actor (shared across all quality levels)
	if (!IsValid(ShadowProxyActor))
	{
		ShadowProxyActor = nullptr;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = TEXT("FluidShadowProxyActor");
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ShadowProxyActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (!ShadowProxyActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("FluidRendererSubsystem: Failed to spawn shadow proxy actor"));
			return nullptr;
		}
	}

	// Get or create shadow mesh for this quality
	UStaticMesh* ShadowMesh = GetOrCreateShadowMesh(Quality);
	if (!ShadowMesh)
	{
		return nullptr;
	}

	// Create ISM component
	FName ComponentName = *FString::Printf(TEXT("ShadowISM_Quality%d"), QualityIndex);
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(ShadowProxyActor, ComponentName);
	if (!ISM)
	{
		return nullptr;
	}

	// Set the first created ISM as root component
	if (!ShadowProxyActor->GetRootComponent())
	{
		ShadowProxyActor->SetRootComponent(ISM);
	}
	else
	{
		ISM->SetupAttachment(ShadowProxyActor->GetRootComponent());
	}

	ISM->SetStaticMesh(ShadowMesh);

	// Shadow settings - cast shadow but invisible in main pass
	ISM->CastShadow = true;
	ISM->bCastDynamicShadow = true;
	ISM->bCastStaticShadow = false;
	ISM->bAffectDynamicIndirectLighting = false;
	ISM->bAffectDistanceFieldLighting = false;

	// Hidden but casts shadow
	ISM->SetVisibility(false);
	ISM->SetHiddenInGame(true);
	ISM->bCastHiddenShadow = true;

	// Performance settings - shadow-only mesh optimization
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetGenerateOverlapEvents(false);
	ISM->bDisableCollision = true;
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->bNeverDistanceCull = true;
	ISM->NumCustomDataFloats = 0;
	ISM->bUseAsOccluder = false;
	ISM->bReceivesDecals = false;
	ISM->bSelectable = false;

	// Register component
	ISM->RegisterComponent();

	ShadowInstanceComponents[QualityIndex] = ISM;

	UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem: Created shadow ISM for quality %d"), QualityIndex);

	return ISM;
}

/**
 * @brief Register shadow particles for aggregation.
 * Particles are aggregated per quality level and rendered in Subsystem Tick.
 * @param ParticlePositions Array of particle world positions.
 * @param NumParticles Number of particles.
 * @param ParticleRadius Radius of each particle.
 * @param Quality Shadow mesh quality level (Low/Medium/High).
 */
void UFluidRendererSubsystem::RegisterShadowParticles(const FVector* ParticlePositions, int32 NumParticles, float ParticleRadius, EFluidShadowMeshQuality Quality)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FluidShadow_RegisterParticles);

	if (!bEnableISMShadow || NumParticles <= 0 || !ParticlePositions)
	{
		return;
	}

	const int32 QualityIndex = static_cast<int32>(Quality);
	if (QualityIndex < 0 || QualityIndex >= NUM_SHADOW_QUALITY_LEVELS)
	{
		return;
	}

	// Apply skip factor
	const int32 SkipFactor = FMath::Max(1, ParticleSkipFactor);
	const int32 NumToAdd = (NumParticles + SkipFactor - 1) / SkipFactor;

	// Reserve space to avoid frequent reallocations
	TArray<FVector>& Buffer = AggregatedPositions[QualityIndex];
	Buffer.Reserve(Buffer.Num() + NumToAdd);

	// Add positions with skip factor (no validation - simulation guarantees valid data)
	for (int32 i = 0; i < NumParticles; i += SkipFactor)
	{
		Buffer.Add(ParticlePositions[i]);
	}

	// Track max radius for this quality level
	AggregatedRadius[QualityIndex] = FMath::Max(AggregatedRadius[QualityIndex], ParticleRadius);
	bHasParticlesThisFrame[QualityIndex] = true;
}

/**
 * @brief Flush aggregated particles to ISM components.
 * Creates transforms from aggregated positions and updates ISM per quality level.
 */
void UFluidRendererSubsystem::FlushShadowInstances()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FluidShadow_FlushInstances);

	check(IsInGameThread());

	for (int32 QualityIndex = 0; QualityIndex < NUM_SHADOW_QUALITY_LEVELS; ++QualityIndex)
	{
		const EFluidShadowMeshQuality Quality = static_cast<EFluidShadowMeshQuality>(QualityIndex);
		TArray<FVector>& Positions = AggregatedPositions[QualityIndex];
		const float Radius = AggregatedRadius[QualityIndex];
		const bool bHasParticles = bHasParticlesThisFrame[QualityIndex];

		// Get or create ISM component for this quality
		UInstancedStaticMeshComponent* ISM = ShadowInstanceComponents[QualityIndex];

		// If no particles registered this frame
		if (!bHasParticles || Positions.Num() == 0)
		{
			// Clear existing instances if any
			if (IsValid(ISM) && ISM->GetInstanceCount() > 0)
			{
				ISM->ClearInstances();
				ISM->MarkRenderStateDirty();
			}
			continue;
		}

		// Create ISM if needed
		if (!IsValid(ISM))
		{
			ISM = GetOrCreateShadowISM(Quality);
			if (!ISM)
			{
				continue;
			}
		}

		const int32 NumInstances = Positions.Num();

		// Prepare transforms
		CachedInstanceTransforms.SetNumUninitialized(NumInstances);
		const FVector SphereScale = FVector(Radius / 50.0f);

		// Generate transforms (parallelize only when count is large enough to offset overhead)
		constexpr int32 ParallelThreshold = 1024;
		if (NumInstances >= ParallelThreshold)
		{
			ParallelFor(NumInstances, [&](int32 i)
			{
				FTransform& T = CachedInstanceTransforms[i];
				T.SetTranslation(Positions[i]);
				T.SetRotation(FQuat::Identity);
				T.SetScale3D(SphereScale);
			});
		}
		else
		{
			for (int32 i = 0; i < NumInstances; ++i)
			{
				FTransform& T = CachedInstanceTransforms[i];
				T.SetTranslation(Positions[i]);
				T.SetRotation(FQuat::Identity);
				T.SetScale3D(SphereScale);
			}
		}

		// Update ISM
		const int32 CurrentCount = ISM->GetInstanceCount();
		if (CurrentCount == NumInstances)
		{
			// Fast path: batch update
			ISM->BatchUpdateInstancesTransforms(0, CachedInstanceTransforms, false, true, false);
		}
		else
		{
			// Rebuild
			ISM->ClearInstances();
			ISM->AddInstances(CachedInstanceTransforms, false, true);
		}
	}
}

/**
 * @brief Clear aggregation buffers for next frame.
 */
void UFluidRendererSubsystem::ClearAggregationBuffers()
{
	for (int32 i = 0; i < NUM_SHADOW_QUALITY_LEVELS; ++i)
	{
		AggregatedPositions[i].Reset();
		AggregatedRadius[i] = 0.0f;
		bHasParticlesThisFrame[i] = false;
	}
}

/**
 * @brief Cleanup all shadow resources.
 */
void UFluidRendererSubsystem::CleanupShadowResources()
{
	// Destroy ISM components
	for (int32 i = 0; i < NUM_SHADOW_QUALITY_LEVELS; ++i)
	{
		if (IsValid(ShadowInstanceComponents[i]))
		{
			ShadowInstanceComponents[i]->ClearInstances();
			ShadowInstanceComponents[i]->DestroyComponent();
		}
		ShadowInstanceComponents[i] = nullptr;
		ShadowSphereMeshes[i] = nullptr;
	}

	// Destroy proxy actor
	if (IsValid(ShadowProxyActor))
	{
		ShadowProxyActor->Destroy();
	}
	ShadowProxyActor = nullptr;

	// Clear buffers
	ClearAggregationBuffers();
	CachedInstanceTransforms.Empty();
}
