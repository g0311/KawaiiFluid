// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preview/FluidPreviewScene.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "Core/FluidParticle.h"
#include "Core/SpatialHash.h"
#include "Core/KawaiiFluidSimulationContext.h"
#include "Core/KawaiiFluidSimulationTypes.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

FFluidPreviewScene::FFluidPreviewScene(FPreviewScene::ConstructionValues CVS)
	: FAdvancedPreviewScene(CVS)
	, CurrentPreset(nullptr)
	, PreviewSettingsObject(nullptr)
	, SimulationModule(nullptr)
	, SimulationContext(nullptr)
	, SpawnAccumulator(0.0f)
	, bSimulationActive(false)
	, PreviewActor(nullptr)
	, ParticleMeshComponent(nullptr)
	, FloorMeshComponent(nullptr)
	, CachedParticleRadius(5.0f)
{
	// Create preview settings object
	PreviewSettingsObject = NewObject<UFluidPreviewSettingsObject>(GetTransientPackage(), NAME_None, RF_Transient);

	// Create simulation module (owns particles, SpatialHash, etc.)
	SimulationModule = NewObject<UKawaiiFluidSimulationModule>(GetTransientPackage(), NAME_None, RF_Transient);

	// Create simulation context (physics solver)
	SimulationContext = NewObject<UKawaiiFluidSimulationContext>(GetTransientPackage(), NAME_None, RF_Transient);

	// Create visualization components
	CreateVisualizationComponents();

	// Setup environment
	SetupFloor();
}

FFluidPreviewScene::~FFluidPreviewScene()
{
	// Clean up simulation module
	if (SimulationModule)
	{
		SimulationModule->Shutdown();
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
	Collector.AddReferencedObject(SimulationModule);
	Collector.AddReferencedObject(SimulationContext);
	Collector.AddReferencedObject(PreviewActor);
	Collector.AddReferencedObject(ParticleMeshComponent);
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

	// Create particle ISM component
	ParticleMeshComponent = NewObject<UInstancedStaticMeshComponent>(PreviewActor, TEXT("ParticleMesh"));
	ParticleMeshComponent->SetupAttachment(RootComp);

	// Use engine sphere mesh
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh)
	{
		ParticleMeshComponent->SetStaticMesh(SphereMesh);
	}

	// Create dynamic material for particles
	UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* ParticleMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, ParticleMeshComponent);
		ParticleMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.5f, 1.0f, 0.8f));
		ParticleMeshComponent->SetMaterial(0, ParticleMaterial);
	}

	ParticleMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ParticleMeshComponent->SetCastShadow(false);
	ParticleMeshComponent->RegisterComponent();

	// Create floor mesh component
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

	AddComponent(ParticleMeshComponent, FTransform::Identity);
	AddComponent(FloorMeshComponent, FTransform::Identity);
}

void FFluidPreviewScene::SetPreset(UKawaiiFluidPresetDataAsset* InPreset)
{
	CurrentPreset = InPreset;

	if (CurrentPreset)
	{
		// Update particle visuals (color)
		if (ParticleMeshComponent)
		{
			UMaterialInstanceDynamic* ParticleMaterial = Cast<UMaterialInstanceDynamic>(ParticleMeshComponent->GetMaterial(0));
			if (ParticleMaterial)
			{
				ParticleMaterial->SetVectorParameterValue(TEXT("Color"), CurrentPreset->Color);
			}
		}

		// Initialize simulation module with preset
		if (SimulationModule)
		{
			SimulationModule->Initialize(CurrentPreset);
		}

		// Initialize solvers in context
		if (SimulationContext)
		{
			SimulationContext->InitializeSolvers(CurrentPreset);
		}

		CachedParticleRadius = CurrentPreset->ParticleRadius;
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

	// Update particle color
	if (ParticleMeshComponent)
	{
		UMaterialInstanceDynamic* ParticleMaterial = Cast<UMaterialInstanceDynamic>(ParticleMeshComponent->GetMaterial(0));
		if (ParticleMaterial)
		{
			ParticleMaterial->SetVectorParameterValue(TEXT("Color"), CurrentPreset->Color);
		}
	}

	// Update simulation module preset (handles SpatialHash internally)
	if (SimulationModule)
	{
		SimulationModule->SetPreset(CurrentPreset);
	}

	CachedParticleRadius = CurrentPreset->ParticleRadius;

	// Update particle visualization scale
	UpdateParticleVisuals();
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
	// Clear existing particles via SimulationModule
	if (SimulationModule)
	{
		SimulationModule->ClearAllParticles();
		SimulationModule->SetAccumulatedTime(0.0f);
	}
	SpawnAccumulator = 0.0f;

	// Update visuals
	UpdateParticleVisuals();
}

void FFluidPreviewScene::ContinuousSpawn(float DeltaTime)
{
	if (!SimulationModule)
	{
		return;
	}

	const FFluidPreviewSettings& Settings = PreviewSettingsObject->Settings;

	// Check if we've reached max particle count
	if (SimulationModule->GetParticleCount() >= Settings.MaxParticleCount)
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
	const int32 RemainingCapacity = Settings.MaxParticleCount - SimulationModule->GetParticleCount();
	const int32 ActualSpawnCount = FMath::Min(ParticlesToSpawn, RemainingCapacity);

	// Spawn particles via SimulationModule
	for (int32 i = 0; i < ActualSpawnCount; ++i)
	{
		// Random position within sphere
		FVector RandomOffset = FMath::VRand() * FMath::FRand() * SpawnRadius;
		FVector Position = SpawnLocation + RandomOffset;

		SimulationModule->SpawnParticle(Position, SpawnVelocity);
	}
}

void FFluidPreviewScene::TickSimulation(float DeltaTime)
{
	if (!bSimulationActive || !CurrentPreset || !SimulationModule)
	{
		return;
	}

	// Continuous spawn new particles
	ContinuousSpawn(DeltaTime);

	// Skip simulation if no particles yet
	FSpatialHash* SpatialHash = SimulationModule->GetSpatialHash();
	if (SimulationModule->GetParticleCount() == 0 || !SimulationContext || !SpatialHash)
	{
		return;
	}

	// Build simulation params
	FKawaiiFluidSimulationParams Params;
	Params.ExternalForce = CurrentPreset->Gravity;
	Params.World = GetWorld();
	Params.bUseWorldCollision = false; // We handle collision manually in preview
	Params.ParticleRadius = CurrentPreset->ParticleRadius;

	// Get accumulated time from module
	float AccumulatedTime = SimulationModule->GetAccumulatedTime();

	// Run simulation using module's particle array
	SimulationContext->Simulate(
		SimulationModule->GetParticlesMutable(),
		CurrentPreset,
		Params,
		*SpatialHash,
		DeltaTime,
		AccumulatedTime
	);

	// Update accumulated time back to module
	SimulationModule->SetAccumulatedTime(AccumulatedTime);

	// Handle floor collision manually
	HandleFloorCollision();

	// Handle wall collision if enabled
	if (PreviewSettingsObject->Settings.bShowWalls)
	{
		HandleWallCollision();
	}

	// Update visuals
	UpdateParticleVisuals();
}

void FFluidPreviewScene::HandleFloorCollision()
{
	if (!PreviewSettingsObject->Settings.bShowFloor || !SimulationModule)
	{
		return;
	}

	const float FloorY = PreviewSettingsObject->Settings.FloorHeight;
	const float ParticleRadius = CurrentPreset ? CurrentPreset->ParticleRadius : 5.0f;
	const float Restitution = CurrentPreset ? CurrentPreset->Restitution : 0.0f;
	const float Friction = CurrentPreset ? CurrentPreset->Friction : 0.5f;

	for (FFluidParticle& Particle : SimulationModule->GetParticlesMutable())
	{
		const float MinZ = FloorY + ParticleRadius;

		if (Particle.Position.Z < MinZ)
		{
			Particle.Position.Z = MinZ;
			Particle.PredictedPosition.Z = MinZ;

			// Apply friction and restitution
			if (Particle.Velocity.Z < 0)
			{
				Particle.Velocity.Z *= -Restitution;

				// Apply friction to horizontal velocity
				FVector HorizontalVel(Particle.Velocity.X, Particle.Velocity.Y, 0.0f);
				HorizontalVel *= (1.0f - Friction);
				Particle.Velocity.X = HorizontalVel.X;
				Particle.Velocity.Y = HorizontalVel.Y;
			}
		}
	}
}

void FFluidPreviewScene::HandleWallCollision()
{
	if (!SimulationModule)
	{
		return;
	}

	const FFluidPreviewSettings& Settings = PreviewSettingsObject->Settings;
	const float HalfSize = Settings.FloorSize.X * 0.5f;
	const float ParticleRadius = CurrentPreset ? CurrentPreset->ParticleRadius : 5.0f;
	const float Restitution = CurrentPreset ? CurrentPreset->Restitution : 0.0f;

	for (FFluidParticle& Particle : SimulationModule->GetParticlesMutable())
	{
		// X boundaries
		if (Particle.Position.X < -HalfSize + ParticleRadius)
		{
			Particle.Position.X = -HalfSize + ParticleRadius;
			Particle.PredictedPosition.X = Particle.Position.X;
			Particle.Velocity.X *= -Restitution;
		}
		else if (Particle.Position.X > HalfSize - ParticleRadius)
		{
			Particle.Position.X = HalfSize - ParticleRadius;
			Particle.PredictedPosition.X = Particle.Position.X;
			Particle.Velocity.X *= -Restitution;
		}

		// Y boundaries
		if (Particle.Position.Y < -HalfSize + ParticleRadius)
		{
			Particle.Position.Y = -HalfSize + ParticleRadius;
			Particle.PredictedPosition.Y = Particle.Position.Y;
			Particle.Velocity.Y *= -Restitution;
		}
		else if (Particle.Position.Y > HalfSize - ParticleRadius)
		{
			Particle.Position.Y = HalfSize - ParticleRadius;
			Particle.PredictedPosition.Y = Particle.Position.Y;
			Particle.Velocity.Y *= -Restitution;
		}
	}
}

FFluidPreviewSettings& FFluidPreviewScene::GetPreviewSettings()
{
	return PreviewSettingsObject->Settings;
}

void FFluidPreviewScene::ApplyPreviewSettings()
{
	UpdateEnvironment();
}

void FFluidPreviewScene::SetupFloor()
{
	UpdateEnvironment();
}

void FFluidPreviewScene::SetupWalls()
{
	// Wall setup is handled in UpdateEnvironment
	UpdateEnvironment();
}

void FFluidPreviewScene::UpdateEnvironment()
{
	if (!FloorMeshComponent)
	{
		return;
	}

	const FFluidPreviewSettings& Settings = PreviewSettingsObject->Settings;

	// Update floor visibility and transform
	FloorMeshComponent->SetVisibility(Settings.bShowFloor);

	if (Settings.bShowFloor)
	{
		FVector FloorScale = Settings.FloorSize / 100.0f; // Cube is 100 units
		FloorMeshComponent->SetWorldLocation(FVector(0.0f, 0.0f, Settings.FloorHeight - Settings.FloorSize.Z * 0.5f));
		FloorMeshComponent->SetWorldScale3D(FloorScale);
	}

	// TODO: Update wall meshes if needed
}

void FFluidPreviewScene::UpdateParticleVisuals()
{
	if (!ParticleMeshComponent || !SimulationModule)
	{
		return;
	}

	const TArray<FFluidParticle>& Particles = SimulationModule->GetParticles();
	const int32 NumParticles = Particles.Num();

	// Adjust instance count
	const int32 CurrentInstanceCount = ParticleMeshComponent->GetInstanceCount();
	if (CurrentInstanceCount != NumParticles)
	{
		ParticleMeshComponent->ClearInstances();

		TArray<FTransform> Transforms;
		Transforms.SetNum(NumParticles);

		for (int32 i = 0; i < NumParticles; ++i)
		{
			Transforms[i] = FTransform::Identity;
		}

		ParticleMeshComponent->AddInstances(Transforms, false);
	}

	// Update instance transforms
	const float ParticleScale = CachedParticleRadius / 50.0f; // Sphere is 100 units diameter

	for (int32 i = 0; i < NumParticles; ++i)
	{
		const FFluidParticle& Particle = Particles[i];
		FTransform InstanceTransform(
			FRotator::ZeroRotator,
			Particle.Position,
			FVector(ParticleScale)
		);
		ParticleMeshComponent->UpdateInstanceTransform(i, InstanceTransform, false, false, true);
	}

	ParticleMeshComponent->MarkRenderStateDirty();
}

//========================================
// Particle Access (via SimulationModule)
//========================================

TArray<FFluidParticle>& FFluidPreviewScene::GetParticles()
{
	static TArray<FFluidParticle> EmptyArray;
	if (SimulationModule)
	{
		return SimulationModule->GetParticlesMutable();
	}
	return EmptyArray;
}

const TArray<FFluidParticle>& FFluidPreviewScene::GetParticles() const
{
	static TArray<FFluidParticle> EmptyArray;
	if (SimulationModule)
	{
		return SimulationModule->GetParticles();
	}
	return EmptyArray;
}

int32 FFluidPreviewScene::GetParticleCount() const
{
	return SimulationModule ? SimulationModule->GetParticleCount() : 0;
}

float FFluidPreviewScene::GetSimulationTime() const
{
	return SimulationModule ? SimulationModule->GetAccumulatedTime() : 0.0f;
}

float FFluidPreviewScene::GetAverageDensity() const
{
	if (!SimulationModule || SimulationModule->GetParticleCount() == 0)
	{
		return 0.0f;
	}

	const TArray<FFluidParticle>& Particles = SimulationModule->GetParticles();
	float TotalDensity = 0.0f;
	for (const FFluidParticle& Particle : Particles)
	{
		TotalDensity += Particle.Density;
	}

	return TotalDensity / Particles.Num();
}
