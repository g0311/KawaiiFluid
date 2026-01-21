// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preview/FluidPreviewScene.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "Core/FluidParticle.h"
#include "Core/SpatialHash.h"
#include "Core/KawaiiFluidSimulationContext.h"
#include "Core/KawaiiFluidSimulationTypes.h"
// SimulationModule removed - using GPU simulation only
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Rendering/KawaiiFluidISMRenderer.h"
#include "Rendering/KawaiiFluidMetaballRenderer.h"
#include "Rendering/FluidRendererSubsystem.h"
#include "GPU/GPUFluidSimulator.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

FFluidPreviewScene::FFluidPreviewScene(FPreviewScene::ConstructionValues CVS)
	: FAdvancedPreviewScene(CVS)
	, CurrentPreset(nullptr)
	, PreviewSettingsObject(nullptr)
	, SimulationContext(nullptr)
	, SpawnAccumulator(0.0f)
	, bSimulationActive(false)
	, RenderingModule(nullptr)
	, PreviewActor(nullptr)
	, FloorMeshComponent(nullptr)
	, CachedParticleRadius(5.0f)
{
	// Create preview settings object
	PreviewSettingsObject = NewObject<UFluidPreviewSettingsObject>(GetTransientPackage(), NAME_None, RF_Transient);

	// Create simulation context (physics solver)
	SimulationContext = NewObject<UKawaiiFluidSimulationContext>(GetTransientPackage(), NAME_None, RF_Transient);

	// Initialize Context's RenderResource for rendering
	SimulationContext->InitializeRenderResource();

	// Initialize GPU simulator for Metaball rendering (required!)
	constexpr int32 PreviewMaxParticles = 10000;
	SimulationContext->InitializeGPUSimulator(PreviewMaxParticles);

	// Create visualization components (including PreviewActor)
	CreateVisualizationComponents();

	// Create rendering module
	RenderingModule = NewObject<UKawaiiFluidRenderingModule>(GetTransientPackage(), NAME_None, RF_Transient);

	if (RenderingModule && PreviewActor)
	{
		// Initialize with this as DataProvider (PreviewActor의 RootComponent에 ISM 부착)
		// Preset은 SetPreset()에서 나중에 설정됨
		RenderingModule->Initialize(GetWorld(), PreviewActor->GetRootComponent(), this, nullptr);

		// Metaball settings come from Preset->RenderingParameters (set in SetPreset)

		// Connect MetaballRenderer to SimulationContext for batched rendering
		if (UKawaiiFluidMetaballRenderer* MR = RenderingModule->GetMetaballRenderer())
		{
			MR->SetSimulationContext(SimulationContext);
		}

		// Register to FluidRendererSubsystem (required for Metaball ViewExtension!)
		if (UWorld* World = GetWorld())
		{
			if (UFluidRendererSubsystem* RendererSubsystem = World->GetSubsystem<UFluidRendererSubsystem>())
			{
				RendererSubsystem->RegisterRenderingModule(RenderingModule);
			}
		}
	}

	// Apply default rendering settings
	ApplyPreviewSettings();

	// Setup environment
	SetupFloor();
}

FFluidPreviewScene::~FFluidPreviewScene()
{
	// Unregister from FluidRendererSubsystem first
	if (RenderingModule)
	{
		if (UWorld* World = GetWorld())
		{
			if (UFluidRendererSubsystem* RendererSubsystem = World->GetSubsystem<UFluidRendererSubsystem>())
			{
				RendererSubsystem->UnregisterRenderingModule(RenderingModule);
			}
		}
		RenderingModule->Cleanup();
	}

	// Release GPU simulator
	if (SimulationContext)
	{
		SimulationContext->ReleaseGPUSimulator();
	}

	if (PreviewActor && PreviewActor->IsValidLowLevel())
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

void FFluidPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(CurrentPreset);
	Collector.AddReferencedObject(PreviewSettingsObject);
	Collector.AddReferencedObject(SimulationContext);
	Collector.AddReferencedObject(RenderingModule);
	Collector.AddReferencedObject(PreviewActor);
	Collector.AddReferencedObject(FloorMeshComponent);
	Collector.AddReferencedObjects(WallMeshComponents);
}

void FFluidPreviewScene::CreateVisualizationComponents()
{
	// Spawn preview actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	PreviewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!PreviewActor)
	{
		return;
	}

	// Create root component
	USceneComponent* RootComp = NewObject<USceneComponent>(PreviewActor, TEXT("Root"));
	PreviewActor->SetRootComponent(RootComp);
	RootComp->RegisterComponent();

	// Create floor mesh component
	UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	FloorMeshComponent = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("FloorMesh"));
	FloorMeshComponent->SetupAttachment(RootComp);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh)
	{
		FloorMeshComponent->SetStaticMesh(CubeMesh);
	}

	// Create floor material
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* FloorMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, FloorMeshComponent);
		FloorMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.3f, 0.3f, 0.35f, 1.0f));
		FloorMeshComponent->SetMaterial(0, FloorMaterial);
	}

	FloorMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorMeshComponent->RegisterComponent();

	AddComponent(FloorMeshComponent, FTransform::Identity);
}

void FFluidPreviewScene::SetPreset(UKawaiiFluidPresetDataAsset* InPreset)
{
	CurrentPreset = InPreset;

	if (CurrentPreset)
	{
		// Initialize solvers in context and cache preset for GPU simulation
		if (SimulationContext)
		{
			SimulationContext->InitializeSolvers(CurrentPreset);
			SimulationContext->SetCachedPreset(CurrentPreset);
		}

		CachedParticleRadius = CurrentPreset->ParticleRadius;

		// Update renderers from preset
		if (RenderingModule)
		{
			// Metaball uses Preset->RenderingParameters via SetPreset
			if (UKawaiiFluidMetaballRenderer* Metaball = RenderingModule->GetMetaballRenderer())
			{
				Metaball->SetPreset(CurrentPreset);
			}
		}

		// Apply ISM settings from PreviewSettings
		ApplyPreviewSettings();
	}

	// Reset simulation
	ResetSimulation();
}

void FFluidPreviewScene::RefreshFromPreset()
{
	if (!CurrentPreset)
	{
		return;
	}

	// Update simulation context solvers (physics parameters) and cache preset
	if (SimulationContext)
	{
		SimulationContext->InitializeSolvers(CurrentPreset);
		SimulationContext->SetCachedPreset(CurrentPreset);
	}

	CachedParticleRadius = CurrentPreset->ParticleRadius;

	// Update renderers from preset
	if (RenderingModule)
	{
		// Metaball uses Preset->RenderingParameters via SetPreset
		if (UKawaiiFluidMetaballRenderer* Metaball = RenderingModule->GetMetaballRenderer())
		{
			Metaball->SetPreset(CurrentPreset);
		}
	}

	// Apply ISM settings from PreviewSettings
	ApplyPreviewSettings();
}

void FFluidPreviewScene::StartSimulation()
{
	bSimulationActive = true;
}

void FFluidPreviewScene::StopSimulation()
{
	bSimulationActive = false;
}

void FFluidPreviewScene::ResetSimulation()
{
	// Clear GPU particles
	if (SimulationContext)
	{
		FGPUFluidSimulator* GPUSimulator = SimulationContext->GetGPUSimulator();
		if (GPUSimulator)
		{
			GPUSimulator->ClearAllParticles();
		}
	}
	SpawnAccumulator = 0.0f;
}

void FFluidPreviewScene::ContinuousSpawn(float DeltaTime)
{
	if (!SimulationContext)
	{
		return;
	}

	FGPUFluidSimulator* GPUSimulator = SimulationContext->GetGPUSimulator();
	if (!GPUSimulator)
	{
		return;
	}

	const FFluidPreviewSettings& Settings = PreviewSettingsObject->Settings;

	// Check if we've reached max particle count (use GPU count)
	const int32 CurrentCount = GPUSimulator->GetParticleCount();
	if (CurrentCount >= Settings.MaxParticleCount)
	{
		return;
	}

	// Accumulate spawn time
	SpawnAccumulator += DeltaTime * Settings.ParticlesPerSecond;

	// Spawn particles based on accumulator
	const int32 ParticlesToSpawn = FMath::FloorToInt(SpawnAccumulator);
	if (ParticlesToSpawn <= 0)
	{
		return;
	}

	// Subtract spawned particles from accumulator
	SpawnAccumulator -= ParticlesToSpawn;

	const FVector SpawnLocation = Settings.SpawnLocation;
	const FVector SpawnVelocity = Settings.SpawnVelocity;
	const float SpawnRadius = Settings.SpawnRadius;

	// Calculate how many we can actually spawn
	const int32 RemainingCapacity = Settings.MaxParticleCount - CurrentCount;
	const int32 ActualSpawnCount = FMath::Min(ParticlesToSpawn, RemainingCapacity);

	// Spawn particles via GPU simulator
	for (int32 i = 0; i < ActualSpawnCount; ++i)
	{
		// Random position within sphere
		FVector RandomOffset = FMath::VRand() * FMath::FRand() * SpawnRadius;
		FVector Position = SpawnLocation + RandomOffset;

		GPUSimulator->AddSpawnRequest(
			FVector3f(Position),
			FVector3f(SpawnVelocity),
			1.0f  // Mass
		);
	}
}

void FFluidPreviewScene::TickSimulation(float DeltaTime)
{
	if (!bSimulationActive || !CurrentPreset || !SimulationContext)
	{
		return;
	}

	FGPUFluidSimulator* GPUSimulator = SimulationContext->GetGPUSimulator();
	if (!GPUSimulator)
	{
		return;
	}

	// Continuous spawn new particles (GPU spawn requests)
	ContinuousSpawn(DeltaTime);

	// Build simulation params
	FKawaiiFluidSimulationParams Params;
	Params.ExternalForce = CurrentPreset->Gravity;
	Params.World = GetWorld();
	Params.bUseWorldCollision = false;
	Params.ParticleRadius = CurrentPreset->ParticleRadius;

	// Set simulation bounds for GPU collision (floor at Z=0)
	const FVector BoundsMin(-500.0, -500.0, 0.0);
	const FVector BoundsMax(500.0, 500.0, 500.0);
	Params.WorldBounds = FBox(BoundsMin, BoundsMax);
	GPUSimulator->SetSimulationBounds(FVector3f(BoundsMin), FVector3f(BoundsMax));

	// Run GPU simulation
	static TArray<FFluidParticle> DummyParticles;  // GPU mode doesn't use CPU particles
	static FSpatialHash DummySpatialHash(CurrentPreset->SmoothingRadius);
	float AccumulatedTime = 0.0f;

	SimulationContext->Simulate(
		DummyParticles,
		CurrentPreset,
		Params,
		DummySpatialHash,
		DeltaTime,
		AccumulatedTime
	);

	// Update rendering module
	if (RenderingModule)
	{
		RenderingModule->UpdateRenderers();
	}
}

void FFluidPreviewScene::HandleFloorCollision()
{
	// Floor collision is handled by GPU bounds collision
	// See TickSimulation() -> Params.WorldBounds
}

FFluidPreviewSettings& FFluidPreviewScene::GetPreviewSettings()
{
	return PreviewSettingsObject->Settings;
}

void FFluidPreviewScene::ApplyPreviewSettings()
{
	UpdateEnvironment();

	// Metaball settings come from Preset->RenderingParameters (set in SetPreset/RefreshFromPreset)
}

void FFluidPreviewScene::SetupFloor()
{
	UpdateEnvironment();
}

void FFluidPreviewScene::UpdateEnvironment()
{
	if (!FloorMeshComponent)
	{
		return;
	}

	// Floor always visible with fixed size
	const FVector FloorSize(500.0f, 500.0f, 10.0f);
	const float FloorHeight = 0.0f;

	FloorMeshComponent->SetVisibility(true);
	FVector FloorScale = FloorSize / 100.0f; // Cube is 100 units
	FloorMeshComponent->SetWorldLocation(FVector(0.0f, 0.0f, FloorHeight - FloorSize.Z * 0.5f));
	FloorMeshComponent->SetWorldScale3D(FloorScale);
}

//========================================
// IKawaiiFluidDataProvider Interface
//========================================

const TArray<FFluidParticle>& FFluidPreviewScene::GetParticles() const
{
	// GPU mode - no CPU particles available
	static TArray<FFluidParticle> EmptyArray;
	return EmptyArray;
}

int32 FFluidPreviewScene::GetParticleCount() const
{
	// Return GPU particle count
	return GetGPUParticleCount();
}

float FFluidPreviewScene::GetParticleRadius() const
{
	return CachedParticleRadius;
}

bool FFluidPreviewScene::IsDataValid() const
{
	// Valid if GPU simulation is active (particles may be 0 but still valid for rendering)
	return IsGPUSimulationActive();
}

//========================================
// Particle Access (GPU mode - limited access)
//========================================

TArray<FFluidParticle>& FFluidPreviewScene::GetParticlesMutable()
{
	// GPU mode - no mutable CPU particles available
	static TArray<FFluidParticle> EmptyArray;
	return EmptyArray;
}

float FFluidPreviewScene::GetSimulationTime() const
{
	// GPU simulation doesn't track accumulated time the same way
	return 0.0f;
}

float FFluidPreviewScene::GetAverageDensity() const
{
	// GPU mode - density is computed on GPU, not easily accessible
	return 0.0f;
}

//========================================
// GPU Simulation Interface
//========================================

bool FFluidPreviewScene::IsGPUSimulationActive() const
{
	return SimulationContext && SimulationContext->IsGPUSimulatorReady();
}

int32 FFluidPreviewScene::GetGPUParticleCount() const
{
	if (SimulationContext)
	{
		FGPUFluidSimulator* GPUSimulator = SimulationContext->GetGPUSimulator();
		if (GPUSimulator)
		{
			return GPUSimulator->GetParticleCount();
		}
	}
	return 0;
}

FGPUFluidSimulator* FFluidPreviewScene::GetGPUSimulator() const
{
	return SimulationContext ? SimulationContext->GetGPUSimulator() : nullptr;
}
