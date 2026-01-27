// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/FluidRendererSubsystem.h"
#include "Rendering/FluidSceneViewExtension.h"
#include "Rendering/FluidShadowUtils.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
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
	// Release View Extension
	ViewExtension.Reset();

	RegisteredRenderingModules.Empty();

	Super::Deinitialize();

	UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem Deinitialized"));
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
// Cached Light Direction (Game Thread)
//========================================

/**
 * @brief Update cached light direction from the main DirectionalLight in the world.
 * Must be called from game thread before rendering.
 */
void UFluidRendererSubsystem::UpdateCachedLightDirection()
{
	check(IsInGameThread());

	UWorld* World = GetWorld();
	if (!World)
	{
		bHasCachedLightData = false;
		return;
	}

	// Find main DirectionalLight using TActorIterator (game thread only)
	ADirectionalLight* MainLight = FluidShadowUtils::FindMainDirectionalLight(World);

	if (MainLight)
	{
		UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(MainLight->GetLightComponent());
		if (LightComp)
		{
			// Get light direction (forward vector points toward the light source, negate for shadow direction)
			FVector LightDir = -LightComp->GetForwardVector();
			CachedLightDirection = FVector3f(LightDir);

			// Calculate light view-projection matrix for shadow mapping
			// Use a default bounds for now (TODO: use actual fluid bounds)
			FBox FluidBounds(FVector(-1000, -1000, -1000), FVector(1000, 1000, 1000));
			FBox ExpandedBounds = FluidBounds.ExpandBy(FluidBounds.GetExtent().Size() * 0.5f);

			FMatrix ViewMatrix, ProjectionMatrix;
			FluidShadowUtils::CalculateDirectionalLightMatrices(LightDir, ExpandedBounds, ViewMatrix, ProjectionMatrix);
			CachedLightViewProjectionMatrix = FMatrix44f(ViewMatrix * ProjectionMatrix);

			bHasCachedLightData = true;
			return;
		}
	}

	// Fallback: use default sun direction
	CachedLightDirection = FVector3f(0.5f, 0.5f, -0.707f).GetSafeNormal();

	FBox FluidBounds(FVector(-1000, -1000, -1000), FVector(1000, 1000, 1000));
	FBox ExpandedBounds = FluidBounds.ExpandBy(FluidBounds.GetExtent().Size() * 0.5f);

	FMatrix ViewMatrix, ProjectionMatrix;
	FluidShadowUtils::CalculateDirectionalLightMatrices(FVector(CachedLightDirection), ExpandedBounds, ViewMatrix, ProjectionMatrix);
	CachedLightViewProjectionMatrix = FMatrix44f(ViewMatrix * ProjectionMatrix);

	bHasCachedLightData = true;
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
// VSM Buffer Management
//========================================

void UFluidRendererSubsystem::SwapVSMBuffers()
{
	// Swap VSM buffers: previous frame's write buffer becomes current frame's read buffer
	VSMTexture_Read = VSMTexture_Write;
	LightVPMatrix_Read = LightVPMatrix_Write;
}

//========================================
// ISM Shadow Management
//========================================

/**
 * @brief Create or destroy the ISM shadow component based on settings.
 */
void UFluidRendererSubsystem::UpdateShadowProxyState()
{
	check(IsInGameThread());

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Note: Actor and ISM creation is now fully handled in UpdateShadowInstances()
	// This function only handles cleanup when shadow is disabled

	if (!bEnableVSMIntegration && (IsValid(ShadowProxyActor) || IsValid(ShadowInstanceComponent)))
	{
		// Destroy ISM component
		if (IsValid(ShadowInstanceComponent))
		{
			ShadowInstanceComponent->ClearInstances();
			ShadowInstanceComponent->DestroyComponent();
		}
		ShadowInstanceComponent = nullptr;

		if (IsValid(ShadowProxyActor))
		{
			ShadowProxyActor->Destroy();
		}
		ShadowProxyActor = nullptr;

		// Release resources - clear all quality levels
		for (int32 i = 0; i < 3; ++i)
		{
			ShadowSphereMeshes[i] = nullptr;
		}
		CachedInstanceTransforms.Empty();
	}
}

/**
 * @brief Get or create shadow sphere mesh for specified quality level.
 * Uses lazy creation and caches meshes per quality level.
 * @param Quality The quality level for the shadow mesh.
 * @return The shadow sphere mesh, or nullptr on failure.
 */
UStaticMesh* UFluidRendererSubsystem::GetOrCreateShadowMesh(EFluidShadowMeshQuality Quality)
{
	const int32 QualityIndex = static_cast<int32>(Quality);
	if (QualityIndex < 0 || QualityIndex >= 3)
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

void UFluidRendererSubsystem::UpdateShadowInstances(const FVector* ParticlePositions, int32 NumParticles, float ParticleRadius, EFluidShadowMeshQuality Quality, int32 MaxParticles)
{
	check(IsInGameThread());

	// Clear HISM instances if no particles (do this even if VSM is disabled)
	if (NumParticles <= 0 || !ParticlePositions)
	{
		if (IsValid(ShadowInstanceComponent))
		{
			// Instead of clearing, just hide if we have a reasonable number of instances
			const int32 CurrentCount = ShadowInstanceComponent->GetInstanceCount();
			if (CurrentCount > 0 && CurrentCount < 5000)
			{
				CachedInstanceTransforms.SetNum(CurrentCount);
				for (auto& Transform : CachedInstanceTransforms)
				{
					Transform.SetScale3D(FVector::ZeroVector);
				}
				ShadowInstanceComponent->BatchUpdateInstancesTransforms(0, CachedInstanceTransforms, false, true, false);
			}
			else
			{
				ShadowInstanceComponent->ClearInstances();
			}
			ShadowInstanceComponent->MarkRenderStateDirty();
		}
		return;
	}

	// Early return if VSM integration is disabled
	if (!bEnableVSMIntegration)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || !World->bActorsInitialized)
	{
		return;
	}

	// Lazy creation of Actor
	if (!IsValid(ShadowProxyActor))
	{
		ShadowProxyActor = nullptr;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = TEXT("FluidShadowProxyActor");
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ShadowProxyActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (ShadowProxyActor)
		{
			// SetActorLabel removed in UE 5.7
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("FluidRendererSubsystem: Failed to spawn shadow proxy actor"));
			return;
		}
	}

	// Get or create shadow mesh for requested quality level
	UStaticMesh* ShadowMesh = GetOrCreateShadowMesh(Quality);
	if (!ShadowMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("FluidRendererSubsystem: Failed to get shadow sphere mesh for quality %d"),
			static_cast<int32>(Quality));
		return;
	}

	// Check if quality changed - need to update ISM component's mesh
	if (CurrentShadowMeshQuality != Quality)
	{
		if (IsValid(ShadowInstanceComponent))
		{
			ShadowInstanceComponent->ClearInstances();
			ShadowInstanceComponent->SetStaticMesh(ShadowMesh);
		}
		CurrentShadowMeshQuality = Quality;
	}

	// Lazy creation of ISM component
	if (!IsValid(ShadowInstanceComponent))
	{
		ShadowInstanceComponent = nullptr;

		// Create ISM component for instanced shadow spheres
		ShadowInstanceComponent = NewObject<UInstancedStaticMeshComponent>(ShadowProxyActor, TEXT("ShadowInstanceComponent"));
		if (ShadowInstanceComponent)
		{
			// Set as root component so it's properly attached to the actor
			ShadowProxyActor->SetRootComponent(ShadowInstanceComponent);

			ShadowInstanceComponent->SetStaticMesh(ShadowMesh);

			// Shadow settings - cast shadow but invisible in main pass
			ShadowInstanceComponent->CastShadow = true;
			ShadowInstanceComponent->bCastDynamicShadow = true;
			ShadowInstanceComponent->bCastStaticShadow = false;
			ShadowInstanceComponent->bAffectDynamicIndirectLighting = false;
			ShadowInstanceComponent->bAffectDistanceFieldLighting = false;

			// Hidden but casts shadow (both editor and game)
			ShadowInstanceComponent->SetVisibility(false);
			ShadowInstanceComponent->SetHiddenInGame(true);
			ShadowInstanceComponent->bCastHiddenShadow = true;

			// Performance settings
			ShadowInstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ShadowInstanceComponent->SetGenerateOverlapEvents(false);
			ShadowInstanceComponent->bDisableCollision = true;
			ShadowInstanceComponent->SetMobility(EComponentMobility::Movable);

			// Register component to the world
			ShadowInstanceComponent->RegisterComponent();
		}
	}

	if (!IsValid(ShadowInstanceComponent))
	{
		return;
	}

	// Calculate number of instances needed for active particles
	const int32 SkipFactor = FMath::Max(1, ParticleSkipFactor);
	int32 NumActiveInstances = (NumParticles + SkipFactor - 1) / SkipFactor;
	if (MaxShadowInstances > 0)
	{
		NumActiveInstances = FMath::Min(NumActiveInstances, MaxShadowInstances);
	}

	// Calculate target capacity (MaxParticles or current active count with padding)
	int32 TargetCapacity = NumActiveInstances;
	if (MaxParticles > 0)
	{
		TargetCapacity = FMath::Max(TargetCapacity, (MaxParticles + SkipFactor - 1) / SkipFactor);
	}

	// Current state
	const int32 CurrentCapacity = ShadowInstanceComponent->GetInstanceCount();
	
	// Determine if we need to rebuild (reallocate)
	// Rebuild if:
	// 1. Current capacity is insufficient
	// 2. Current capacity is excessively large (waste > 50% AND > 1000 instances)
	const bool bInsufficient = CurrentCapacity < NumActiveInstances;
	const bool bExcessive = (CurrentCapacity > TargetCapacity * 1.5f) && (CurrentCapacity - TargetCapacity > 1000);

	// Prepare transforms buffer
	// Use the larger of Current or Target capacity to ensure we fill everything
	const int32 BufferSize = bInsufficient ? TargetCapacity : CurrentCapacity;
	CachedInstanceTransforms.SetNumUninitialized(BufferSize);

	const FVector SphereScale = FVector(ParticleRadius / 50.0f);
	
	// 1. Fill active instances
	// Parallelize this loop for performance
	ParallelFor(NumActiveInstances, [&](int32 InstanceIdx)
	{
		const int32 ParticleIdx = InstanceIdx * SkipFactor;
		if (ParticleIdx < NumParticles)
		{
			const FVector& Position = ParticlePositions[ParticleIdx];
			FTransform& OutTransform = CachedInstanceTransforms[InstanceIdx];

			if (FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z))
			{
				OutTransform.SetTranslation(Position);
				OutTransform.SetRotation(FQuat::Identity);
				OutTransform.SetScale3D(SphereScale);
			}
			else
			{
				OutTransform.SetIdentity();
				OutTransform.SetScale3D(FVector::ZeroVector);
			}
		}
	});

	// 2. Fill remaining instances (padding/pooling) with zero scale
	if (BufferSize > NumActiveInstances)
	{
		const int32 Remaining = BufferSize - NumActiveInstances;
		// Simple loop is fast enough for zeroing
		for (int32 i = NumActiveInstances; i < BufferSize; ++i)
		{
			CachedInstanceTransforms[i].SetIdentity();
			CachedInstanceTransforms[i].SetScale3D(FVector::ZeroVector);
		}
	}

	// Apply to ISM Component
	if (bInsufficient || bExcessive)
	{
		// Full Rebuild
		ShadowInstanceComponent->ClearInstances();
		// Add instances (false = don't return indices, true = mark dirty)
		ShadowInstanceComponent->AddInstances(CachedInstanceTransforms, false, true);
	}
	else
	{
		// Batch Update (Fast path)
		ShadowInstanceComponent->BatchUpdateInstancesTransforms(
			0,
			CachedInstanceTransforms,
			false,  // bWorldSpace
			true,   // bMarkRenderStateDirty
			false   // bTeleport
		);
	}
}

/**
 * @brief Update shadow instances with anisotropy data for ellipsoid shadows.
 *
 * Note: This function is currently not used because anisotropy-based shadows cause flickering.
 * The flickering is caused by GPU Morton sorting which reorders particle indices every frame,
 * resulting in mismatched position/anisotropy data pairs. Until a stable particle ID system
 * is implemented, use UpdateShadowInstances() with uniform spheres instead.
 *
 * @param ParticlePositions Array of particle world positions.
 * @param AnisotropyAxis1 Array of first ellipsoid axis (xyz=direction, w=scale).
 * @param AnisotropyAxis2 Array of second ellipsoid axis.
 * @param AnisotropyAxis3 Array of third ellipsoid axis.
 * @param NumParticles Number of particles.
 * @param ParticleRadius Radius of each particle.
 * @param Quality Shadow mesh quality level (Low/Medium/High).
 */
void UFluidRendererSubsystem::UpdateShadowInstancesWithAnisotropy(
	const FVector* ParticlePositions,
	const FVector4* AnisotropyAxis1,
	const FVector4* AnisotropyAxis2,
	const FVector4* AnisotropyAxis3,
	int32 NumParticles,
	float ParticleRadius,
	EFluidShadowMeshQuality Quality)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FluidShadow_UpdateInstancesWithAnisotropy);

	check(IsInGameThread());

	// Early return if VSM integration is disabled
	if (!bEnableVSMIntegration)
	{
		return;
	}

	// Clear ISM instances if no particles
	if (NumParticles <= 0 || !ParticlePositions)
	{
		if (IsValid(ShadowInstanceComponent))
		{
			ShadowInstanceComponent->ClearInstances();
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || !World->bActorsInitialized)
	{
		return;
	}

	// Lazy creation of Actor
	if (!IsValid(ShadowProxyActor))
	{
		ShadowProxyActor = nullptr;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = TEXT("FluidShadowProxyActor");
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ShadowProxyActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (ShadowProxyActor)
		{
			// SetActorLabel removed in UE 5.7
		}
		else
		{
			return;
		}
	}

	// Get or create shadow mesh for requested quality level
	UStaticMesh* ShadowMesh = GetOrCreateShadowMesh(Quality);
	if (!ShadowMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("FluidRendererSubsystem: Failed to get shadow sphere mesh (anisotropy) for quality %d"),
			static_cast<int32>(Quality));
		return;
	}

	// Check if quality changed - need to update ISM component's mesh
	if (CurrentShadowMeshQuality != Quality)
	{
		if (IsValid(ShadowInstanceComponent))
		{
			ShadowInstanceComponent->ClearInstances();
			ShadowInstanceComponent->SetStaticMesh(ShadowMesh);
		}
		CurrentShadowMeshQuality = Quality;
	}

	// Lazy creation of ISM component
	if (!IsValid(ShadowInstanceComponent))
	{
		ShadowInstanceComponent = nullptr;

		ShadowInstanceComponent = NewObject<UInstancedStaticMeshComponent>(ShadowProxyActor, TEXT("ShadowInstanceComponent"));
		if (ShadowInstanceComponent)
		{
			ShadowProxyActor->SetRootComponent(ShadowInstanceComponent);
			ShadowInstanceComponent->SetStaticMesh(ShadowMesh);

			ShadowInstanceComponent->CastShadow = true;
			ShadowInstanceComponent->bCastDynamicShadow = true;
			ShadowInstanceComponent->bCastStaticShadow = false;
			ShadowInstanceComponent->bAffectDynamicIndirectLighting = false;
			ShadowInstanceComponent->bAffectDistanceFieldLighting = false;

			ShadowInstanceComponent->SetVisibility(false);
			ShadowInstanceComponent->SetHiddenInGame(true);
			ShadowInstanceComponent->bCastHiddenShadow = true;

			ShadowInstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ShadowInstanceComponent->SetGenerateOverlapEvents(false);
			ShadowInstanceComponent->bDisableCollision = true;
			ShadowInstanceComponent->SetMobility(EComponentMobility::Movable);

			ShadowInstanceComponent->RegisterComponent();
		}
	}

	if (!IsValid(ShadowInstanceComponent))
	{
		return;
	}

	// Calculate number of instances
	const int32 SkipFactor = FMath::Max(1, ParticleSkipFactor);
	int32 NumInstances = (NumParticles + SkipFactor - 1) / SkipFactor;

	if (MaxShadowInstances > 0)
	{
		NumInstances = FMath::Min(NumInstances, MaxShadowInstances);
	}

	// Prepare transforms with anisotropy
	CachedInstanceTransforms.SetNumUninitialized(NumInstances);

	const float BaseScale = ParticleRadius / 50.0f;
	const bool bHasAnisotropy = (AnisotropyAxis1 != nullptr && AnisotropyAxis2 != nullptr && AnisotropyAxis3 != nullptr);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FluidShadow_ComputeTransforms);

		for (int32 i = 0, InstanceIdx = 0; i < NumParticles && InstanceIdx < NumInstances; i += SkipFactor, ++InstanceIdx)
		{
			FTransform& InstanceTransform = CachedInstanceTransforms[InstanceIdx];

			// Validate position - skip particles with NaN/Inf positions
			const FVector& Position = ParticlePositions[i];
			if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z))
			{
				// Use a safe default transform for invalid particles
				InstanceTransform.SetLocation(FVector::ZeroVector);
				InstanceTransform.SetRotation(FQuat::Identity);
				InstanceTransform.SetScale3D(FVector::ZeroVector);  // Zero scale = invisible
				continue;
			}

			InstanceTransform.SetLocation(Position);

			if (bHasAnisotropy)
			{
				// Build rotation from anisotropy axes (eigenvectors)
				const FVector4& Axis1 = AnisotropyAxis1[i];
				const FVector4& Axis2 = AnisotropyAxis2[i];
				const FVector4& Axis3 = AnisotropyAxis3[i];

				// Validate scale values - FMath::Clamp does NOT handle NaN properly!
				// NaN comparisons always return false, so Clamp(NaN, min, max) returns NaN
				const bool bValidScales =
					FMath::IsFinite(Axis1.W) &&
					FMath::IsFinite(Axis2.W) &&
					FMath::IsFinite(Axis3.W);

				if (bValidScales)
				{
					// Extract scale from W components (eigenvalues) with clamping
					const float Scale1 = FMath::Clamp(Axis1.W, 0.1f, 3.0f);
					const float Scale2 = FMath::Clamp(Axis2.W, 0.1f, 3.0f);
					const float Scale3 = FMath::Clamp(Axis3.W, 0.1f, 3.0f);

					// Build rotation matrix from eigenvectors
					FMatrix RotationMatrix = FMatrix::Identity;
					RotationMatrix.SetAxis(0, FVector(Axis1.X, Axis1.Y, Axis1.Z).GetSafeNormal());
					RotationMatrix.SetAxis(1, FVector(Axis2.X, Axis2.Y, Axis2.Z).GetSafeNormal());
					RotationMatrix.SetAxis(2, FVector(Axis3.X, Axis3.Y, Axis3.Z).GetSafeNormal());

					InstanceTransform.SetRotation(RotationMatrix.ToQuat());
					InstanceTransform.SetScale3D(FVector(Scale1, Scale2, Scale3) * BaseScale);
				}
				else
				{
					// Invalid anisotropy data - fall back to uniform sphere
					InstanceTransform.SetRotation(FQuat::Identity);
					InstanceTransform.SetScale3D(FVector(BaseScale));
				}
			}
			else
			{
				InstanceTransform.SetRotation(FQuat::Identity);
				InstanceTransform.SetScale3D(FVector(BaseScale));
			}
		}
	}

	// Optimized ISM update
	TRACE_CPUPROFILER_EVENT_SCOPE(FluidShadow_ISMUpdate);
	const int32 CurrentInstanceCount = ShadowInstanceComponent->GetInstanceCount();
	const int32 TargetInstanceCount = NumInstances;

	if (TargetInstanceCount == 0)
	{
		if (CurrentInstanceCount > 0)
		{
			ShadowInstanceComponent->ClearInstances();
		}
		return;
	}

	if (CurrentInstanceCount == TargetInstanceCount)
	{
		// Batch update all transforms at once
		ShadowInstanceComponent->BatchUpdateInstancesTransforms(
			0,
			CachedInstanceTransforms,
			false,  // bWorldSpace
			true,   // bMarkRenderStateDirty
			false   // bTeleport
		);
	}
	else
	{
		// Instance count changed - rebuild entirely
		ShadowInstanceComponent->ClearInstances();
		ShadowInstanceComponent->AddInstances(CachedInstanceTransforms, false, true);
	}
}
