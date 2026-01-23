// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Actors/KawaiiFluidVolume.h"
#include "Actors/KawaiiFluidEmitter.h"
#include "Components/KawaiiFluidVolumeComponent.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Core/KawaiiFluidSimulationContext.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Rendering/KawaiiFluidMetaballRenderer.h"
#include "Rendering/KawaiiFluidISMRenderer.h"
#include "Rendering/FluidRendererSubsystem.h"
#include "GPU/GPUFluidSimulator.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"

AKawaiiFluidVolume::AKawaiiFluidVolume()
{
	// Create the volume component as root
	VolumeComponent = CreateDefaultSubobject<UKawaiiFluidVolumeComponent>(TEXT("VolumeComponent"));
	RootComponent = VolumeComponent;

	// Create the rendering module
	RenderingModule = CreateDefaultSubobject<UKawaiiFluidRenderingModule>(TEXT("RenderingModule"));

	// Enable ticking for simulation
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AKawaiiFluidVolume::BeginPlay()
{
	Super::BeginPlay();

	// Initialize simulation
	InitializeSimulation();

	// Initialize rendering
	InitializeRendering();

	// Register to subsystem
	RegisterToSubsystem();
}

void AKawaiiFluidVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from subsystem
	UnregisterFromSubsystem();

	// Cleanup rendering
	CleanupRendering();

	// Cleanup simulation
	CleanupSimulation();

	Super::EndPlay(EndPlayReason);
}

void AKawaiiFluidVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World || !VolumeComponent)
	{
		return;
	}

	// Process pending spawn requests BEFORE subsystem simulation runs
	// This converts queued spawn requests (from emitters) into actual particles
	ProcessPendingSpawnRequests();

	// Update rendering (read from VolumeComponent)
	if (VolumeComponent->bEnableRendering && RenderingModule && RenderingModule->IsInitialized())
	{
		RenderingModule->UpdateRenderers();
	}

	// Debug visualization (read from VolumeComponent)
	if (VolumeComponent->bEnableDebugDraw)
	{
		DrawDebugParticles();
	}
	if (VolumeComponent->bShowStaticBoundaryParticles)
	{
		DrawDebugStaticBoundaryParticles();
	}

	// VSM Integration: Update shadow proxy with particle data
	if (SimulationModule)
	{
		if (UFluidRendererSubsystem* RendererSubsystem = World->GetSubsystem<UFluidRendererSubsystem>())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(VSM Integration Volume)
			if (RendererSubsystem->bEnableVSMIntegration && VolumeComponent->bEnableShadow)
			{
				// Use member buffer to avoid per-frame allocation
				ShadowPredictionBuffer.Reset();
				TArray<FVector>& Positions = ShadowPredictionBuffer;
				int32 NumParticles = 0;

				// Check if GPU simulation is active
				FGPUFluidSimulator* GPUSimulator = SimulationModule->GetGPUSimulator();
				const bool bGPUActive = SimulationModule->IsGPUSimulationActive() && GPUSimulator != nullptr;

				// 파티클 수가 0이면 ISM 클리어하고 스킵
				const int32 ActualParticleCount = bGPUActive ? GPUSimulator->GetParticleCount() : SimulationModule->GetParticleCount();
				if (ActualParticleCount <= 0)
				{
					CachedShadowPositions.Empty();
					CachedShadowVelocities.Empty();
					RendererSubsystem->UpdateShadowInstances(nullptr, 0, 0.0f);
					return;
				}

				if (bGPUActive)
				{
					// GPU Mode: Enable async shadow readback and get positions
					GPUSimulator->SetShadowReadbackEnabled(true);
					GPUSimulator->SetAnisotropyReadbackEnabled(true);

					TArray<FVector> NewVelocities;
					TArray<FVector4> NewAnisotropyAxis1, NewAnisotropyAxis2, NewAnisotropyAxis3;

					if (GPUSimulator->HasReadyShadowPositions())
					{
						// New readback data available - update cache (with anisotropy)
						GPUSimulator->GetShadowDataWithAnisotropy(
							Positions, NewVelocities,
							NewAnisotropyAxis1, NewAnisotropyAxis2, NewAnisotropyAxis3);
						NumParticles = Positions.Num();

						if (NumParticles > 0)
						{
							// Use Move for arrays that won't be used after this (avoids deep copy)
							CachedShadowPositions = Positions;  // Keep copy - needed for UpdateShadowInstances below
							CachedShadowVelocities = MoveTemp(NewVelocities);
							CachedAnisotropyAxis1 = MoveTemp(NewAnisotropyAxis1);
							CachedAnisotropyAxis2 = MoveTemp(NewAnisotropyAxis2);
							CachedAnisotropyAxis3 = MoveTemp(NewAnisotropyAxis3);
							LastShadowReadbackFrame = GFrameCounter;
							LastShadowReadbackTime = FPlatformTime::Seconds();

							// Also cache neighbor counts for isolation detection
							GPUSimulator->GetShadowNeighborCounts(CachedNeighborCounts);
						}
					}
					else if (CachedShadowPositions.Num() > 0 && CachedShadowVelocities.Num() == CachedShadowPositions.Num())
					{
						// No new data - predict positions using cached velocity
						const double CurrentTime = FPlatformTime::Seconds();
						const float PredictionDelta = static_cast<float>(CurrentTime - LastShadowReadbackTime);

						// Clamp prediction delta to avoid extreme extrapolation
						const float ClampedDelta = FMath::Clamp(PredictionDelta, 0.0f, 0.1f);

						NumParticles = CachedShadowPositions.Num();
						Positions.SetNumUninitialized(NumParticles);

						// Parallelize prediction loop
						ParallelFor(NumParticles, [&](int32 i)
						{
							// Predict: Position += Velocity * DeltaTime
							Positions[i] = CachedShadowPositions[i] + CachedShadowVelocities[i] * ClampedDelta;
						});
						// Note: Anisotropy is not predicted, use cached values directly
					}
					else if (CachedShadowPositions.Num() > 0)
					{
						// Fallback: use cached positions without prediction
						Positions = CachedShadowPositions;
						NumParticles = Positions.Num();
					}
				}
				else
				{
					// CPU Mode: Get positions from CPU particles
					const TArray<FFluidParticle>& Particles = SimulationModule->GetParticles();
					NumParticles = Particles.Num();

					if (NumParticles > 0)
					{
						Positions.SetNum(NumParticles);
						CachedShadowVelocities.SetNum(NumParticles);
						CachedNeighborCounts.SetNum(NumParticles);

						for (int32 i = 0; i < NumParticles; ++i)
						{
							Positions[i] = Particles[i].Position;
							CachedShadowVelocities[i] = Particles[i].Velocity;
							CachedNeighborCounts[i] = Particles[i].NeighborIndices.Num();
						}
					}
				}

				if (NumParticles > 0)
				{
					// Calculate fluid bounds from positions (Parallel Reduction)
					FBox FluidBounds(ForceInit);

					if (NumParticles > 1024)
					{
						const int32 ChunkSize = 1024;
						const int32 NumChunks = FMath::DivideAndRoundUp(NumParticles, ChunkSize);
						TArray<FBox> ChunkBounds;
						ChunkBounds.Init(FBox(ForceInit), NumChunks);

						ParallelFor(NumChunks, [&](int32 ChunkIndex)
						{
							const int32 StartIndex = ChunkIndex * ChunkSize;
							const int32 EndIndex = FMath::Min(StartIndex + ChunkSize, NumParticles);

							FBox LocalBox(ForceInit);
							for (int32 i = StartIndex; i < EndIndex; ++i)
							{
								LocalBox += Positions[i];
							}
							ChunkBounds[ChunkIndex] = LocalBox;
						});

						// 각 청크의 결과를 메인 Bounds에 병합
						for (const FBox& Box : ChunkBounds)
						{
							FluidBounds += Box;
						}
					}
					else
					{
						// 파티클 수가 적으면 단일 스레드에서 처리 (오버헤드 방지)
						for (int32 i = 0; i < NumParticles; ++i)
						{
							FluidBounds += Positions[i];
						}
					}

					// Expand bounds by particle radius
					const float ParticleRadius = SimulationModule->GetParticleRadius();
					FluidBounds = FluidBounds.ExpandBy(ParticleRadius * 2.0f);

					// Update shadow proxy state (creates HISM component if needed)
					RendererSubsystem->UpdateShadowProxyState();

					// Update HISM shadow instances with uniform spheres
					// Volume doesn't have MaxParticleCount like Component, use current count
					RendererSubsystem->UpdateShadowInstances(
						Positions.GetData(),
						NumParticles,
						ParticleRadius
					);

					// Spawn splash VFX based on condition mode (read from VolumeComponent)
					UNiagaraSystem* SplashVFX = VolumeComponent->SplashVFX;
					if (SplashVFX)
					{
						const float SplashVelocityThreshold = VolumeComponent->SplashVelocityThreshold;
						const int32 MaxSplashVFXPerFrame = VolumeComponent->MaxSplashVFXPerFrame;
						const ESplashConditionMode SplashConditionMode = VolumeComponent->SplashConditionMode;
						const int32 IsolationNeighborThreshold = VolumeComponent->IsolationNeighborThreshold;

						int32 SpawnCount = 0;
						const bool bHasVelocityData = CachedShadowVelocities.Num() == NumParticles;
						const bool bHasNeighborData = CachedNeighborCounts.Num() == NumParticles;
						const bool bHasPrevNeighborData = PrevNeighborCounts.Num() == NumParticles;

						for (int32 i = 0; i < NumParticles && SpawnCount < MaxSplashVFXPerFrame; ++i)
						{
							// Velocity condition: fast-moving particle
							bool bFastMoving = false;
							FVector VelocityDir = FVector::UpVector;
							if (bHasVelocityData)
							{
								const float Speed = CachedShadowVelocities[i].Size();
								bFastMoving = Speed > SplashVelocityThreshold;
								VelocityDir = CachedShadowVelocities[i].GetSafeNormal();
							}

							// Isolation condition: few neighbors
							bool bIsolated = false;
							bool bJustBecameIsolated = false;
							if (bHasNeighborData)
							{
								bIsolated = CachedNeighborCounts[i] <= IsolationNeighborThreshold;

								// State change detection: was not isolated -> now isolated
								if (bHasPrevNeighborData)
								{
									const bool bWasIsolated = PrevNeighborCounts[i] <= IsolationNeighborThreshold;
									bJustBecameIsolated = bIsolated && !bWasIsolated;
								}
								else
								{
									// First frame with data - treat as state change if isolated
									bJustBecameIsolated = bIsolated;
								}
							}

							// Evaluate spawn condition based on mode
							// For isolation-related modes, only spawn on state change (non-isolated -> isolated)
							bool bShouldSpawn = false;
							switch (SplashConditionMode)
							{
							case ESplashConditionMode::VelocityAndIsolation:
								bShouldSpawn = bFastMoving && bJustBecameIsolated;
								break;
							case ESplashConditionMode::VelocityOrIsolation:
								bShouldSpawn = bFastMoving || bJustBecameIsolated;
								break;
							case ESplashConditionMode::VelocityOnly:
								bShouldSpawn = bFastMoving;
								break;
							case ESplashConditionMode::IsolationOnly:
								bShouldSpawn = bJustBecameIsolated;
								break;
							}

							if (bShouldSpawn)
							{
								UNiagaraFunctionLibrary::SpawnSystemAtLocation(
									World,
									SplashVFX,
									Positions[i],
									VelocityDir.Rotation()
								);
								++SpawnCount;
							}
						}

						// Update previous neighbor counts for next frame's state change detection
						if (bHasNeighborData)
						{
							PrevNeighborCounts = CachedNeighborCounts;
						}
					}
				}
			}
		}
	}

	// Simulation is handled by subsystem for proper batching
	// The Subsystem tick runs after actor ticks, so particles spawned above
	// will be simulated in the current frame.
}

#if WITH_EDITOR
void AKawaiiFluidVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Preset changes are now handled in VolumeComponent
	// This method can be used for actor-specific property changes if needed
}
#endif

void AKawaiiFluidVolume::Simulate(float DeltaTime)
{
	// Process pending spawn requests before simulation
	ProcessPendingSpawnRequests();

	// Simulation is handled by Subsystem which processes all registered modules.
	// The SimulationModule registered in InitializeSimulation() is automatically
	// simulated by the Subsystem.
}

void AKawaiiFluidVolume::QueueSpawnRequest(FVector Position, FVector Velocity, int32 SourceID)
{
	FGPUSpawnRequest Request;
	Request.Position = FVector3f(Position);
	Request.Velocity = FVector3f(Velocity);
	Request.SourceID = SourceID;
	Request.Mass = 1.0f;
	Request.Radius = 0.0f; // Use default from preset

	PendingSpawnRequests.Add(Request);
}

void AKawaiiFluidVolume::QueueSpawnRequests(const TArray<FVector>& Positions, const TArray<FVector>& Velocities, int32 SourceID)
{
	const int32 Count = FMath::Min(Positions.Num(), Velocities.Num());
	if (Count == 0)
	{
		return;
	}

	PendingSpawnRequests.Reserve(PendingSpawnRequests.Num() + Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FGPUSpawnRequest Request;
		Request.Position = FVector3f(Positions[i]);
		Request.Velocity = FVector3f(Velocities[i]);
		Request.SourceID = SourceID;
		Request.Mass = 1.0f;
		Request.Radius = 0.0f;

		PendingSpawnRequests.Add(Request);
	}
}

void AKawaiiFluidVolume::ProcessPendingSpawnRequests()
{
	if (PendingSpawnRequests.Num() == 0 || !SimulationModule)
	{
		return;
	}

	// Debug: Check GPU simulator status before spawning
	static bool bLoggedOnce = false;
	if (!bLoggedOnce)
	{
		const bool bHasGPU = SimulationModule->GetGPUSimulator() != nullptr;
		const bool bGPUActive = SimulationModule->IsGPUSimulationActive();
		UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: ProcessPendingSpawnRequests - GPU=%s, Active=%s"),
			*GetName(), bHasGPU ? TEXT("Yes") : TEXT("No"), bGPUActive ? TEXT("Yes") : TEXT("No"));
		bLoggedOnce = true;
	}

	int32 SuccessCount = 0;
	// Spawn all pending particles
	for (const FGPUSpawnRequest& Request : PendingSpawnRequests)
	{
		int32 Result = SimulationModule->SpawnParticle(
			FVector(Request.Position.X, Request.Position.Y, Request.Position.Z),
			FVector(Request.Velocity.X, Request.Velocity.Y, Request.Velocity.Z)
		);
		if (Result >= 0)
		{
			SuccessCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: Processed %d spawn requests, %d succeeded"),
		*GetName(), PendingSpawnRequests.Num(), SuccessCount);

	PendingSpawnRequests.Empty();
}

void AKawaiiFluidVolume::RegisterEmitter(AKawaiiFluidEmitter* Emitter)
{
	if (Emitter && !RegisteredEmitters.Contains(Emitter))
	{
		RegisteredEmitters.Add(Emitter);
		UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume: Registered emitter %s"), *Emitter->GetName());
	}
}

void AKawaiiFluidVolume::UnregisterEmitter(AKawaiiFluidEmitter* Emitter)
{
	if (Emitter)
	{
		RegisteredEmitters.Remove(Emitter);
		UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume: Unregistered emitter %s"), *Emitter->GetName());
	}
}

//========================================
// Delegate Getters (from VolumeComponent)
//========================================

UKawaiiFluidPresetDataAsset* AKawaiiFluidVolume::GetPreset() const
{
	return VolumeComponent ? VolumeComponent->GetPreset() : nullptr;
}

EFluidType AKawaiiFluidVolume::GetFluidType() const
{
	return VolumeComponent ? VolumeComponent->GetFluidType() : EFluidType::None;
}

void AKawaiiFluidVolume::SetFluidType(EFluidType InFluidType)
{
	if (VolumeComponent)
	{
		VolumeComponent->SetFluidType(InFluidType);
	}

	// Also forward to SimulationModule if initialized
	if (SimulationModule)
	{
		SimulationModule->SetFluidType(InFluidType);
	}
}

float AKawaiiFluidVolume::GetParticleSpacing() const
{
	return VolumeComponent ? VolumeComponent->GetParticleSpacing() : 10.0f;
}

FVector AKawaiiFluidVolume::GetVolumeSize() const
{
	return VolumeComponent ? VolumeComponent->GetEffectiveVolumeSize() : FVector(1000.0f);
}

//========================================
// Debug Methods (Delegate to VolumeComponent)
//========================================

void AKawaiiFluidVolume::SetDebugVisualization(EFluidDebugVisualization Mode)
{
	if (VolumeComponent)
	{
		VolumeComponent->SetDebugVisualization(Mode);
	}
}

EFluidDebugVisualization AKawaiiFluidVolume::GetDebugVisualization() const
{
	return VolumeComponent ? VolumeComponent->GetDebugVisualization() : EFluidDebugVisualization::None;
}

void AKawaiiFluidVolume::EnableDebugDraw(EFluidDebugVisualization Mode, float PointSize)
{
	if (VolumeComponent)
	{
		VolumeComponent->EnableDebugDraw(Mode, PointSize);
	}
}

void AKawaiiFluidVolume::DisableDebugDraw()
{
	if (VolumeComponent)
	{
		VolumeComponent->DisableDebugDraw();
	}
}

//========================================
// Initialization
//========================================

void AKawaiiFluidVolume::InitializeSimulation()
{
	UWorld* World = GetWorld();
	if (!World || !VolumeComponent)
	{
		return;
	}

	// Get Preset from VolumeComponent
	UKawaiiFluidPresetDataAsset* Preset = VolumeComponent->GetPreset();

	// Create default Preset if not assigned
	if (!Preset)
	{
		Preset = NewObject<UKawaiiFluidPresetDataAsset>(this, NAME_None, RF_Transient);
		VolumeComponent->Preset = Preset;
		UE_LOG(LogTemp, Warning, TEXT("AKawaiiFluidVolume [%s]: No Preset assigned, using default values"), *GetName());
	}

	// Create SimulationModule
	SimulationModule = NewObject<UKawaiiFluidSimulationModule>(this);
	SimulationModule->Initialize(Preset);

	// Forward VolumeComponent properties to SimulationModule
	SimulationModule->FluidType = VolumeComponent->FluidType;
	SimulationModule->bUseWorldCollision = VolumeComponent->bUseWorldCollision;
	SimulationModule->bEnableCollisionEvents = VolumeComponent->bEnableCollisionEvents;
	SimulationModule->MinVelocityForEvent = VolumeComponent->MinVelocityForEvent;
	SimulationModule->MaxEventsPerFrame = VolumeComponent->MaxEventsPerFrame;
	SimulationModule->EventCooldownPerParticle = VolumeComponent->EventCooldownPerParticle;

	// Set the module to use this volume's VolumeComponent for bounds
	SimulationModule->SetTargetSimulationVolume(this);

	// Register module with VolumeComponent
	VolumeComponent->RegisterModule(SimulationModule);

	// Get or create SimulationContext from Subsystem
	UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>();
	if (Subsystem)
	{
		// Register the module to subsystem (handles SourceID allocation and GPU setup)
		Subsystem->RegisterModule(SimulationModule);

		// Get context for this volume
		SimulationContext = Subsystem->GetOrCreateContext(VolumeComponent, Preset);
	}

	// Debug: Verify GPU initialization status
	const bool bGPUReady = SimulationModule && SimulationModule->GetGPUSimulator() != nullptr;
	const bool bGPUActive = SimulationModule && SimulationModule->IsGPUSimulationActive();
	const bool bContextReady = SimulationContext && SimulationContext->IsGPUSimulatorReady();

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: Simulation initialized with Preset [%s]"),
		*GetName(), Preset ? *Preset->GetName() : TEXT("Default"));
	UE_LOG(LogTemp, Log, TEXT("  - GPU Simulator: %s, Active: %s, Context Ready: %s"),
		bGPUReady ? TEXT("Ready") : TEXT("NOT Ready"),
		bGPUActive ? TEXT("Yes") : TEXT("No"),
		bContextReady ? TEXT("Yes") : TEXT("No"));
}

void AKawaiiFluidVolume::CleanupSimulation()
{
	UWorld* World = GetWorld();

	// Unregister module from VolumeComponent
	if (VolumeComponent && SimulationModule)
	{
		VolumeComponent->UnregisterModule(SimulationModule);
	}

	// Unregister module from Subsystem
	if (World)
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			if (SimulationModule)
			{
				Subsystem->UnregisterModule(SimulationModule);
			}
		}
	}

	// Shutdown SimulationModule
	if (SimulationModule)
	{
		SimulationModule->Shutdown();
		SimulationModule = nullptr;
	}

	// Clear context reference (context is owned by Subsystem)
	SimulationContext = nullptr;

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: Simulation cleaned up"), *GetName());
}

void AKawaiiFluidVolume::InitializeRendering()
{
	if (!VolumeComponent || !VolumeComponent->bEnableRendering || !RenderingModule || !SimulationModule)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Get Preset from VolumeComponent
	UKawaiiFluidPresetDataAsset* Preset = VolumeComponent->GetPreset();

	// Initialize RenderingModule with SimulationModule as data provider
	// Rendering parameters (PipelineType, etc.) are taken from Preset->RenderingParameters
	RenderingModule->Initialize(World, VolumeComponent, SimulationModule, Preset);

	// Configure ISMRenderer (debug/preview renderer)
	if (UKawaiiFluidISMRenderer* ISMRenderer = RenderingModule->GetISMRenderer())
	{
		// ISM disabled by default (use Metaball for production)
		// To enable ISM debug view, set ISMRenderer->bEnabled=true in Blueprint or C++
		// ISMRenderer manages its own settings (bEnabled, ParticleColor as UPROPERTY)
		ISMRenderer->SetEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: ISMRenderer disabled (use Metaball renderer)"), *GetName());
	}

	// Configure MetaballRenderer based on preset's RenderingParameters
	if (UKawaiiFluidMetaballRenderer* MetaballRenderer = RenderingModule->GetMetaballRenderer())
	{
		// Enable MetaballRenderer based on preset's bEnableRendering
		if (Preset)
		{
			MetaballRenderer->SetEnabled(Preset->RenderingParameters.bEnableRendering);

			// UpdatePipeline() creates the correct pipeline based on preset's PipelineType
			// GetLocalParameters() returns Preset->RenderingParameters
			MetaballRenderer->UpdatePipeline();

			UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: MetaballRenderer configured (Enabled: %s, Pipeline: %d)"),
				*GetName(),
				Preset->RenderingParameters.bEnableRendering ? TEXT("true") : TEXT("false"),
				static_cast<int32>(Preset->RenderingParameters.PipelineType));
	}

	// Connect MetaballRenderer to SimulationContext
		// This is done here because MetaballRenderer wasn't created yet when
		// Subsystem::RegisterModule was called in InitializeSimulation
		if (SimulationContext)
		{
			MetaballRenderer->SetSimulationContext(SimulationContext);
		}
	}

	// Register RenderingModule with FluidRendererSubsystem (CRITICAL for SSFR rendering pipeline)
	if (UFluidRendererSubsystem* RendererSubsystem = World->GetSubsystem<UFluidRendererSubsystem>())
	{
		RendererSubsystem->RegisterRenderingModule(RenderingModule);
	}

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: Rendering initialized"), *GetName());
}

void AKawaiiFluidVolume::CleanupRendering()
{
	// Unregister from FluidRendererSubsystem first
	if (UWorld* World = GetWorld())
	{
		if (UFluidRendererSubsystem* RendererSubsystem = World->GetSubsystem<UFluidRendererSubsystem>())
		{
			RendererSubsystem->UnregisterRenderingModule(RenderingModule);
		}
	}

	if (RenderingModule)
	{
		RenderingModule->Cleanup();
	}

	UE_LOG(LogTemp, Log, TEXT("AKawaiiFluidVolume [%s]: Rendering cleaned up"), *GetName());
}

void AKawaiiFluidVolume::RegisterToSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->RegisterVolume(this);
		}
	}
}

void AKawaiiFluidVolume::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->UnregisterVolume(this);
		}
	}
}

//========================================
// Debug Visualization
//========================================

void AKawaiiFluidVolume::DrawDebugParticles()
{
	UWorld* World = GetWorld();
	if (!World || !SimulationModule || !VolumeComponent)
	{
		return;
	}

	// Get particle data
	TArray<FVector> Positions = SimulationModule->GetParticlePositions();

	if (Positions.Num() == 0)
	{
		return;
	}

	// Get debug settings from VolumeComponent
	const EFluidDebugVisualization DebugDrawMode = VolumeComponent->DebugDrawMode;
	const float DebugPointSize = VolumeComponent->DebugPointSize;

	// Get densities from particles if needed
	TArray<float> Densities;
	if (DebugDrawMode == EFluidDebugVisualization::Density)
	{
		const TArray<FFluidParticle>& Particles = SimulationModule->GetParticles();
		Densities.Reserve(Particles.Num());
		for (const FFluidParticle& P : Particles)
		{
			Densities.Add(P.Density);
		}
	}

	// Update bounds for position-based coloring
	DebugDrawBoundsMin = Positions[0];
	DebugDrawBoundsMax = Positions[0];
	for (const FVector& Pos : Positions)
	{
		DebugDrawBoundsMin = DebugDrawBoundsMin.ComponentMin(Pos);
		DebugDrawBoundsMax = DebugDrawBoundsMax.ComponentMax(Pos);
	}

	const int32 TotalCount = Positions.Num();
	for (int32 i = 0; i < TotalCount; ++i)
	{
		const float Density = (i < Densities.Num()) ? Densities[i] : 0.0f;
		FColor Color = ComputeDebugDrawColor(i, TotalCount, Positions[i], Density);
		DrawDebugPoint(World, Positions[i], DebugPointSize, Color, false, -1.0f, 0);
	}
}

void AKawaiiFluidVolume::DrawDebugStaticBoundaryParticles()
{
	UWorld* World = GetWorld();
	if (!World || !VolumeComponent)
	{
		return;
	}

	// Get debug settings from VolumeComponent
	const float StaticBoundaryPointSize = VolumeComponent->StaticBoundaryPointSize;
	const FColor StaticBoundaryColor = VolumeComponent->StaticBoundaryColor;
	const bool bShowStaticBoundaryNormals = VolumeComponent->bShowStaticBoundaryNormals;
	const float StaticBoundaryNormalLength = VolumeComponent->StaticBoundaryNormalLength;

#if WITH_EDITOR
	// In editor (non-game), generate preview boundary particles
	if (!World->IsGameWorld())
	{
		GenerateEditorBoundaryParticlesPreview();

		for (int32 i = 0; i < EditorPreviewBoundaryPositions.Num(); ++i)
		{
			DrawDebugPoint(World, EditorPreviewBoundaryPositions[i], StaticBoundaryPointSize, StaticBoundaryColor, false, -1.0f, 0);

			if (bShowStaticBoundaryNormals && i < EditorPreviewBoundaryNormals.Num())
			{
				const FVector EndPos = EditorPreviewBoundaryPositions[i] + EditorPreviewBoundaryNormals[i] * StaticBoundaryNormalLength;
				DrawDebugLine(World, EditorPreviewBoundaryPositions[i], EndPos, FColor::Yellow, false, -1.0f, 0, 1.0f);
			}
		}
		return;
	}
#endif

	// Runtime: boundary particles are generated based on volume bounds
	// For runtime, use the same generation logic as editor preview
	if (!VolumeComponent->IsStaticBoundaryParticlesEnabled())
	{
		return;
	}

	const float Spacing = VolumeComponent->GetStaticBoundaryParticleSpacing();
	const FVector BoundsMin = VolumeComponent->GetWorldBoundsMin();
	const FVector BoundsMax = VolumeComponent->GetWorldBoundsMax();

	// Draw bottom face only for performance (most important for visualization)
	for (float x = BoundsMin.X; x <= BoundsMax.X; x += Spacing)
	{
		for (float y = BoundsMin.Y; y <= BoundsMax.Y; y += Spacing)
		{
			const FVector Pos(x, y, BoundsMin.Z);
			DrawDebugPoint(World, Pos, StaticBoundaryPointSize, StaticBoundaryColor, false, -1.0f, 0);

			if (bShowStaticBoundaryNormals)
			{
				const FVector EndPos = Pos + FVector(0, 0, 1) * StaticBoundaryNormalLength;
				DrawDebugLine(World, Pos, EndPos, FColor::Yellow, false, -1.0f, 0, 1.0f);
			}
		}
	}
}

#if WITH_EDITOR
void AKawaiiFluidVolume::GenerateEditorBoundaryParticlesPreview()
{
	// Only regenerate every N frames to avoid performance issues
	const uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame - LastEditorPreviewFrame < 30)
	{
		return;
	}
	LastEditorPreviewFrame = CurrentFrame;

	EditorPreviewBoundaryPositions.Empty();
	EditorPreviewBoundaryNormals.Empty();

	if (!VolumeComponent || !VolumeComponent->IsStaticBoundaryParticlesEnabled())
	{
		return;
	}

	const float Spacing = VolumeComponent->GetStaticBoundaryParticleSpacing();
	const FVector BoundsMin = VolumeComponent->GetWorldBoundsMin();
	const FVector BoundsMax = VolumeComponent->GetWorldBoundsMax();

	// Generate boundary particles on all 6 faces
	// Bottom face (Z min)
	for (float x = BoundsMin.X; x <= BoundsMax.X; x += Spacing)
	{
		for (float y = BoundsMin.Y; y <= BoundsMax.Y; y += Spacing)
		{
			EditorPreviewBoundaryPositions.Add(FVector(x, y, BoundsMin.Z));
			EditorPreviewBoundaryNormals.Add(FVector(0, 0, 1));
		}
	}

	// Top face (Z max)
	for (float x = BoundsMin.X; x <= BoundsMax.X; x += Spacing)
	{
		for (float y = BoundsMin.Y; y <= BoundsMax.Y; y += Spacing)
		{
			EditorPreviewBoundaryPositions.Add(FVector(x, y, BoundsMax.Z));
			EditorPreviewBoundaryNormals.Add(FVector(0, 0, -1));
		}
	}

	// Front face (Y min)
	for (float x = BoundsMin.X; x <= BoundsMax.X; x += Spacing)
	{
		for (float z = BoundsMin.Z + Spacing; z < BoundsMax.Z; z += Spacing)
		{
			EditorPreviewBoundaryPositions.Add(FVector(x, BoundsMin.Y, z));
			EditorPreviewBoundaryNormals.Add(FVector(0, 1, 0));
		}
	}

	// Back face (Y max)
	for (float x = BoundsMin.X; x <= BoundsMax.X; x += Spacing)
	{
		for (float z = BoundsMin.Z + Spacing; z < BoundsMax.Z; z += Spacing)
		{
			EditorPreviewBoundaryPositions.Add(FVector(x, BoundsMax.Y, z));
			EditorPreviewBoundaryNormals.Add(FVector(0, -1, 0));
		}
	}

	// Left face (X min)
	for (float y = BoundsMin.Y + Spacing; y < BoundsMax.Y; y += Spacing)
	{
		for (float z = BoundsMin.Z + Spacing; z < BoundsMax.Z; z += Spacing)
		{
			EditorPreviewBoundaryPositions.Add(FVector(BoundsMin.X, y, z));
			EditorPreviewBoundaryNormals.Add(FVector(1, 0, 0));
		}
	}

	// Right face (X max)
	for (float y = BoundsMin.Y + Spacing; y < BoundsMax.Y; y += Spacing)
	{
		for (float z = BoundsMin.Z + Spacing; z < BoundsMax.Z; z += Spacing)
		{
			EditorPreviewBoundaryPositions.Add(FVector(BoundsMax.X, y, z));
			EditorPreviewBoundaryNormals.Add(FVector(-1, 0, 0));
		}
	}
}
#endif

FColor AKawaiiFluidVolume::ComputeDebugDrawColor(int32 ParticleIndex, int32 TotalCount, const FVector& InPosition, float Density) const
{
	if (!VolumeComponent)
	{
		return FColor::White;
	}

	const EFluidDebugVisualization DebugDrawMode = VolumeComponent->DebugDrawMode;
	const UKawaiiFluidPresetDataAsset* Preset = VolumeComponent->GetPreset();

	switch (DebugDrawMode)
	{
	case EFluidDebugVisualization::ZOrderArrayIndex:
		{
			// Rainbow gradient based on array index
			const float T = TotalCount > 1 ? static_cast<float>(ParticleIndex) / static_cast<float>(TotalCount - 1) : 0.0f;
			return FLinearColor::MakeFromHSV8(static_cast<uint8>(T * 255.0f), 255, 255).ToFColor(true);
		}

	case EFluidDebugVisualization::PositionX:
		{
			const FVector Range = DebugDrawBoundsMax - DebugDrawBoundsMin;
			const float Normalized = (Range.X > KINDA_SMALL_NUMBER) ?
				(InPosition.X - DebugDrawBoundsMin.X) / Range.X : 0.0f;
			return FColor(static_cast<uint8>(FMath::Clamp(Normalized, 0.0f, 1.0f) * 255), 50, 50);
		}

	case EFluidDebugVisualization::PositionY:
		{
			const FVector Range = DebugDrawBoundsMax - DebugDrawBoundsMin;
			const float Normalized = (Range.Y > KINDA_SMALL_NUMBER) ?
				(InPosition.Y - DebugDrawBoundsMin.Y) / Range.Y : 0.0f;
			return FColor(50, static_cast<uint8>(FMath::Clamp(Normalized, 0.0f, 1.0f) * 255), 50);
		}

	case EFluidDebugVisualization::PositionZ:
		{
			const FVector Range = DebugDrawBoundsMax - DebugDrawBoundsMin;
			const float Normalized = (Range.Z > KINDA_SMALL_NUMBER) ?
				(InPosition.Z - DebugDrawBoundsMin.Z) / Range.Z : 0.0f;
			return FColor(50, 50, static_cast<uint8>(FMath::Clamp(Normalized, 0.0f, 1.0f) * 255));
		}

	case EFluidDebugVisualization::Density:
		{
			// Blue (low) to Red (high) based on density
			const float RestDensity = Preset ? Preset->RestDensity : 1000.0f;
			const float NormalizedDensity = FMath::Clamp(Density / (RestDensity * 2.0f), 0.0f, 1.0f);
			return FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, NormalizedDensity).ToFColor(true);
		}

	default:
		return FColor::White;
	}
}

//========================================
// Brush API
//========================================

void AKawaiiFluidVolume::AddParticlesInRadius(const FVector& WorldCenter, float Radius, int32 Count,
                                              const FVector& Velocity, float Randomness,
                                              const FVector& SurfaceNormal)
{
	if (!SimulationModule || Count <= 0 || Radius <= 0.0f)
	{
		return;
	}

	// Hemisphere distribution above surface
	for (int32 i = 0; i < Count; ++i)
	{
		// Random point in unit sphere
		FVector RandomDir = FMath::VRand();

		// Ensure above surface (dot with normal > 0)
		if (FVector::DotProduct(RandomDir, SurfaceNormal) < 0)
		{
			RandomDir = -RandomDir;
		}

		// Apply randomness to radius
		const float RandomRadius = Radius * FMath::FRandRange(1.0f - Randomness, 1.0f);
		const FVector SpawnPos = WorldCenter + RandomDir * RandomRadius;

		SimulationModule->SpawnParticle(SpawnPos, Velocity);
	}
}

int32 AKawaiiFluidVolume::RemoveParticlesInRadius(const FVector& WorldCenter, float Radius)
{
	if (!SimulationModule)
	{
		return 0;
	}

	// ID-based despawn: CPU에서 리드백 데이터로 영역 내 ParticleID 수집 후 GPU에서 제거
	FGPUFluidSimulator* GPUSimulator = SimulationModule->GetGPUSimulator();
	if (!GPUSimulator)
	{
		return 0;
	}

	// Get readback particle data from GPU
	TArray<FGPUFluidParticle> ReadbackParticles;
	if (!GPUSimulator->GetReadbackGPUParticles(ReadbackParticles))
	{
		// No valid readback data yet, skip this frame
		return 0;
	}

	// Find particles within radius and collect their IDs (Volume removes all particles, no SourceID filter)
	const float RadiusSq = Radius * Radius;
	const FVector3f WorldCenterF = FVector3f(WorldCenter);
	TArray<int32> ParticleIDsToRemove;
	TArray<int32> AllReadbackIDs;
	ParticleIDsToRemove.Reserve(128);
	AllReadbackIDs.Reserve(ReadbackParticles.Num());

	for (const FGPUFluidParticle& Particle : ReadbackParticles)
	{
		AllReadbackIDs.Add(Particle.ParticleID);

		const float DistSq = FVector3f::DistSquared(Particle.Position, WorldCenterF);
		if (DistSq <= RadiusSq)
		{
			ParticleIDsToRemove.Add(Particle.ParticleID);
		}
	}

	// Submit ID-based despawn request (CleanupCompletedRequests는 Readback 시 호출됨)
	if (ParticleIDsToRemove.Num() > 0)
	{
		GPUSimulator->AddDespawnByIDRequests(ParticleIDsToRemove);
	}

	return ParticleIDsToRemove.Num();
}

void AKawaiiFluidVolume::ClearAllParticles()
{
	if (SimulationModule)
	{
		SimulationModule->ClearAllParticles();
	}

	if (RenderingModule)
	{
		RenderingModule->Cleanup();
	}
}

//========================================
// Event System
//========================================

void AKawaiiFluidVolume::HandleCollisionEvent(const FKawaiiFluidCollisionEvent& Event)
{
	// Broadcast to VolumeComponent's delegate
	if (VolumeComponent)
	{
		VolumeComponent->OnParticleHit.Broadcast(Event);
	}
}
