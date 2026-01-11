// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidRendererSubsystem.h"
#include "Rendering/FluidSceneViewExtension.h"
#include "Rendering/FluidShadowHistoryManager.h"
#include "Rendering/FluidShadowUtils.h"
#include "Rendering/FluidShadowProxyComponent.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

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

	// Scene View Extension 생성 및 등록
	ViewExtension = FSceneViewExtensions::NewExtension<FFluidSceneViewExtension>(this);

	// Shadow History Manager 생성
	ShadowHistoryManager = MakeUnique<FFluidShadowHistoryManager>();

	UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem Initialized"));
}

void UFluidRendererSubsystem::Deinitialize()
{
	// View Extension 해제
	ViewExtension.Reset();

	// Shadow History Manager 해제
	ShadowHistoryManager.Reset();

	RegisteredRenderingModules.Empty();

	Super::Deinitialize();

	UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem Deinitialized"));
}

//========================================
// RenderingModule 관리
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
// VSM Buffer Management
//========================================

void UFluidRendererSubsystem::SwapVSMBuffers()
{
	// Swap VSM buffers: previous frame's write buffer becomes current frame's read buffer
	VSMTexture_Read = VSMTexture_Write;
	LightVPMatrix_Read = LightVPMatrix_Write;
}

//========================================
// UE5 VSM Integration (Phase 6)
//========================================

/**
 * @brief Update density grid from particle positions.
 * @param ParticlePositions Array of particle world positions.
 * @param NumParticles Number of particles.
 * @param FluidBounds World-space bounds of the fluid.
 */
void UFluidRendererSubsystem::UpdateDensityGrid(const FVector* ParticlePositions, int32 NumParticles, const FBox& FluidBounds)
{
	if (!bEnableVSMIntegration || NumParticles <= 0 || !ParticlePositions)
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("VSM: UpdateDensityGrid called - Particles: %d, Bounds: %s"),
		NumParticles, *FluidBounds.ToString());

	// Note: DensityGrid creation disabled for now - requires Render Thread
	// Material-based shadow approach works without DensityGrid
	// TODO: Move DensityGrid creation to Render Thread if ray-marched shadows needed

	/*
	// Create density grid if needed
	if (!DensityGrid.IsValid())
	{
		DensityGrid = MakeUnique<FFluidDensityGrid>();
	}

	// Setup grid config
	FFluidDensityGridConfig GridConfig;
	GridConfig.Resolution = FIntVector(DensityGridResolution, DensityGridResolution, DensityGridResolution);
	GridConfig.WorldBoundsMin = FluidBounds.Min;
	GridConfig.WorldBoundsMax = FluidBounds.Max;
	GridConfig.SurfaceThreshold = SurfaceDensityThreshold;
	GridConfig.BoundsPadding = DensityParticleRadius * 2.0f;

	// Initialize or update grid
	if (!DensityGrid->IsValid() || DensityGrid->NeedsReallocation(GridConfig))
	{
		DensityGrid->Initialize(GridConfig);
	}

	// Update bounds
	DensityGrid->UpdateBoundsFromParticles(ParticlePositions, NumParticles);
	*/

	// Update shadow proxy if exists
	if (ShadowProxyComponent)
	{
		ShadowProxyComponent->SetFluidBounds(FluidBounds);
		// DensityGrid disabled for now - Material-based shadows don't need it
		// ShadowProxyComponent->SetDensityGrid(DensityGrid.Get());
		ShadowProxyComponent->SurfaceDensityThreshold = SurfaceDensityThreshold;
		ShadowProxyComponent->MaxRayMarchSteps = MaxRayMarchSteps;

		UE_LOG(LogTemp, Verbose, TEXT("VSM: ShadowProxy updated - Material: %s, Bounds: %s"),
			ShadowProxyComponent->ShadowMaterialBase ? *ShadowProxyComponent->ShadowMaterialBase->GetName() : TEXT("NULL"),
			*FluidBounds.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("VSM: ShadowProxyComponent is NULL!"));
	}

	// Note: Actual density rasterization is done in the render thread
	// via RenderFluidDensityRasterize() called from ViewExtension
}

/**
 * @brief Create or destroy the shadow proxy component based on settings.
 */
void UFluidRendererSubsystem::UpdateShadowProxyState()
{
	check(IsInGameThread());

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("VSM: UpdateShadowProxyState - World is NULL!"));
		return;
	}

	// Note: Actor and HISM creation is now fully handled in UpdateShadowInstances()
	// This function only handles cleanup when VSM is disabled

	if (!bEnableVSMIntegration && (IsValid(ShadowProxyActor) || IsValid(ShadowInstanceComponent)))
	{
		// Destroy HISM component
		if (IsValid(ShadowInstanceComponent))
		{
			ShadowInstanceComponent->ClearInstances();
			ShadowInstanceComponent->DestroyComponent();
		}
		ShadowInstanceComponent = nullptr;

		// Destroy legacy shadow proxy
		if (IsValid(ShadowProxyComponent))
		{
			ShadowProxyComponent->DestroyComponent();
		}
		ShadowProxyComponent = nullptr;

		if (IsValid(ShadowProxyActor))
		{
			ShadowProxyActor->Destroy();
		}
		ShadowProxyActor = nullptr;

		// Release resources
		ShadowSphereMesh = nullptr;
		DensityGrid.Reset();
		CachedInstanceTransforms.Empty();

		UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem: Destroyed shadow instance component"));
	}
}

/**
 * @brief Update shadow instances from particle positions.
 * @param ParticlePositions Array of particle world positions.
 * @param NumParticles Number of particles.
 */
void UFluidRendererSubsystem::UpdateShadowInstances(const FVector* ParticlePositions, int32 NumParticles)
{
	check(IsInGameThread());

	// Debug log
	static int32 DebugCounter = 0;
	if (++DebugCounter % 300 == 1)
	{
		UE_LOG(LogTemp, Log, TEXT("VSM UpdateShadowInstances: bEnable=%d, NumParticles=%d, Actor=%p, HISM=%p"),
			bEnableVSMIntegration, NumParticles, ShadowProxyActor, ShadowInstanceComponent);
	}

	// VSM 비활성화 시 early return
	if (!bEnableVSMIntegration)
	{
		return;
	}

	// 파티클이 없으면 HISM 클리어
	if (NumParticles <= 0 || !ParticlePositions)
	{
		if (IsValid(ShadowInstanceComponent))
		{
			ShadowInstanceComponent->ClearInstances();
			ShadowInstanceComponent->MarkRenderStateDirty();
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Lazy creation of Actor (moved here from UpdateShadowProxyState for better timing)
	// Use IsValid() to catch stale pointers from previous PIE sessions
	if (!IsValid(ShadowProxyActor))
	{
		ShadowProxyActor = nullptr;  // Clear stale pointer
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = TEXT("FluidShadowProxyActor");
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ShadowProxyActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (ShadowProxyActor)
		{
			ShadowProxyActor->SetActorLabel(TEXT("FluidShadowProxy"));
			UE_LOG(LogTemp, Log, TEXT("VSM: Created shadow proxy actor in UpdateShadowInstances"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("VSM: Failed to spawn shadow proxy actor!"));
			return;
		}
	}

	// Lazy creation of HISM component
	// Use IsValid() to catch stale pointers from previous PIE sessions
	if (!IsValid(ShadowInstanceComponent))
	{
		ShadowInstanceComponent = nullptr;  // Clear stale pointer

		// Load sphere mesh if not already loaded (also check validity)
		if (!IsValid(ShadowSphereMesh))
		{
			ShadowSphereMesh = nullptr;
			ShadowSphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			if (!ShadowSphereMesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("VSM: Failed to load sphere mesh!"));
				return;
			}
		}

		// Create HISM component for instanced shadow spheres
		ShadowInstanceComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(ShadowProxyActor, TEXT("ShadowInstanceComponent"));
		if (ShadowInstanceComponent)
		{
			// Set as root component so it's properly attached to the actor
			ShadowProxyActor->SetRootComponent(ShadowInstanceComponent);

			ShadowInstanceComponent->SetStaticMesh(ShadowSphereMesh);

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
			ShadowInstanceComponent->SetCullDistances(0, 50000);  // Don't cull by distance

			// Use simple collision for better performance
			ShadowInstanceComponent->SetMobility(EComponentMobility::Movable);

			// Register component to the world
			ShadowInstanceComponent->RegisterComponent();

			UE_LOG(LogTemp, Log, TEXT("FluidRendererSubsystem: Created HISM shadow component as root (Mesh: %s)"),
				*ShadowSphereMesh->GetName());
		}
	}

	if (!IsValid(ShadowInstanceComponent))
	{
		return;
	}

	// Calculate number of instances based on skip factor and max limit
	int32 SkipFactor = FMath::Max(1, ParticleSkipFactor);
	int32 NumInstances = (NumParticles + SkipFactor - 1) / SkipFactor;

	if (MaxShadowInstances > 0)
	{
		NumInstances = FMath::Min(NumInstances, MaxShadowInstances);
	}

	// Prepare transforms
	CachedInstanceTransforms.Reset();
	CachedInstanceTransforms.Reserve(NumInstances);

	const FVector SphereScale = FVector(ShadowSphereRadius / 50.0f);  // Default sphere is 100 units diameter

	for (int32 i = 0; i < NumParticles && CachedInstanceTransforms.Num() < NumInstances; i += SkipFactor)
	{
		FTransform InstanceTransform;
		InstanceTransform.SetLocation(ParticlePositions[i]);
		InstanceTransform.SetScale3D(SphereScale);
		CachedInstanceTransforms.Add(InstanceTransform);
	}

	// Batch update instances - much faster than individual Add/Remove
	const int32 CurrentInstanceCount = ShadowInstanceComponent->GetInstanceCount();
	const int32 TargetInstanceCount = CachedInstanceTransforms.Num();

	if (TargetInstanceCount == 0)
	{
		if (CurrentInstanceCount > 0)
		{
			ShadowInstanceComponent->ClearInstances();
		}
		return;
	}

	// If instance count changed significantly, rebuild entirely
	if (FMath::Abs(CurrentInstanceCount - TargetInstanceCount) > TargetInstanceCount / 4)
	{
		ShadowInstanceComponent->ClearInstances();
		ShadowInstanceComponent->AddInstances(CachedInstanceTransforms, false);
		ShadowInstanceComponent->MarkRenderStateDirty();
	}
	else
	{
		// Update existing instances and add/remove as needed
		int32 UpdateCount = FMath::Min(CurrentInstanceCount, TargetInstanceCount);

		// Batch update transforms
		for (int32 i = 0; i < UpdateCount; ++i)
		{
			ShadowInstanceComponent->UpdateInstanceTransform(i, CachedInstanceTransforms[i], false, false, false);
		}

		// Add new instances if needed
		if (TargetInstanceCount > CurrentInstanceCount)
		{
			for (int32 i = CurrentInstanceCount; i < TargetInstanceCount; ++i)
			{
				ShadowInstanceComponent->AddInstance(CachedInstanceTransforms[i], false);
			}
		}
		// Remove excess instances if needed
		else if (TargetInstanceCount < CurrentInstanceCount)
		{
			for (int32 i = CurrentInstanceCount - 1; i >= TargetInstanceCount; --i)
			{
				ShadowInstanceComponent->RemoveInstance(i);
			}
		}

		// Mark render state dirty after batch update
		ShadowInstanceComponent->MarkRenderStateDirty();
	}

	static int32 LogCounter = 0;
	if (++LogCounter % 300 == 1)
	{
		UE_LOG(LogTemp, Log, TEXT("VSM HISM: Updated %d shadow instances from %d particles (Skip: %d, Radius: %.1f)"),
			TargetInstanceCount, NumParticles, SkipFactor, ShadowSphereRadius);
	}
}

