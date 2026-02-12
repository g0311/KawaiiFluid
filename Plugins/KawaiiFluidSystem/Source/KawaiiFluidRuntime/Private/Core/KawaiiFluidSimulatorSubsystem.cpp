// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Core/KawaiiFluidSimulationContext.h"
#include "Core/KawaiiFluidSpatialHash.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Components/KawaiiFluidVolumeComponent.h"
#include "Actors/KawaiiFluidVolume.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Rendering/KawaiiFluidRenderer.h"
#include "Rendering/Resources/KawaiiFluidRenderResource.h"
#include "Components/KawaiiFluidInteractionComponent.h"
#include "Collision/KawaiiFluidCollider.h"
#include "Simulation/GPUFluidSimulator.h"
#include "Engine/Level.h"
#include "Engine/World.h"

// Profiling
DECLARE_STATS_GROUP(TEXT("KawaiiFluidSubsystem"), STATGROUP_KawaiiFluidSubsystem, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Subsystem Tick"), STAT_SubsystemTick, STATGROUP_KawaiiFluidSubsystem);
DECLARE_CYCLE_STAT(TEXT("Simulate Independent"), STAT_SimulateIndependent, STATGROUP_KawaiiFluidSubsystem);
DECLARE_CYCLE_STAT(TEXT("Simulate Batched"), STAT_SimulateBatched, STATGROUP_KawaiiFluidSubsystem);
DECLARE_CYCLE_STAT(TEXT("Merge Particles"), STAT_MergeParticles, STATGROUP_KawaiiFluidSubsystem);
DECLARE_CYCLE_STAT(TEXT("Split Particles"), STAT_SplitParticles, STATGROUP_KawaiiFluidSubsystem);

/**
 * @brief Default constructor for UKawaiiFluidSimulatorSubsystem.
 */
UKawaiiFluidSimulatorSubsystem::UKawaiiFluidSimulatorSubsystem()
{
}

/**
 * @brief Destructor for UKawaiiFluidSimulatorSubsystem.
 */
UKawaiiFluidSimulatorSubsystem::~UKawaiiFluidSimulatorSubsystem()
{
}

/**
 * @brief Initialize the subsystem and register world delegates.
 * @param Collection Subsystem collection for dependency initialization.
 */
void UKawaiiFluidSimulatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create shared spatial hash with default cell size
	SharedSpatialHash = MakeShared<FKawaiiFluidSpatialHash>(20.0f);

	// Create default context
	DefaultContext = NewObject<UKawaiiFluidSimulationContext>(this);

	if (UWorld* World = GetWorld())
	{
		OnActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &UKawaiiFluidSimulatorSubsystem::HandleActorSpawned));
	}

	OnLevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(
		this, &UKawaiiFluidSimulatorSubsystem::HandleLevelAdded);
	OnLevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(
		this, &UKawaiiFluidSimulatorSubsystem::HandleLevelRemoved);

	// Bind simulation to post-actor tick (AFTER animation evaluation)
	// This fixes 1-frame delay in bone attachment following
	OnPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(
		this, &UKawaiiFluidSimulatorSubsystem::HandlePostActorTick);

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidSimulatorSubsystem initialized (simulation runs post-actor tick)"));
}

/**
 * @brief Cleanup the subsystem and unregister all world delegates.
 */
void UKawaiiFluidSimulatorSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (OnActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(OnActorSpawnedHandle);
		}
	}

	if (OnLevelAddedHandle.IsValid())
	{
		FWorldDelegates::LevelAddedToWorld.Remove(OnLevelAddedHandle);
	}

	if (OnLevelRemovedHandle.IsValid())
	{
		FWorldDelegates::LevelRemovedFromWorld.Remove(OnLevelRemovedHandle);
	}

	// Unbind post-actor tick delegate
	if (OnPostActorTickHandle.IsValid())
	{
		FWorldDelegates::OnWorldPostActorTick.Remove(OnPostActorTickHandle);
	}

	AllModules.Empty();
	AllVolumes.Empty();
	AllVolumeComponents.Empty();
	GlobalColliders.Empty();
	GlobalInteractionComponents.Empty();
	ContextCache.Empty();
	DefaultContext = nullptr;
	SharedSpatialHash.Reset();

	Super::Deinitialize();

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidSimulatorSubsystem deinitialized"));
}

/**
 * @brief Get the unique stat ID for profiling this subsystem.
 * @return TStatId for the subsystem.
 */
TStatId UKawaiiFluidSimulatorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKawaiiFluidSimulatorSubsystem, STATGROUP_Tickables);
}

/**
 * @brief Perform per-frame updates before the main simulation pass.
 * @param DeltaTime The time elapsed since the last frame.
 */
void UKawaiiFluidSimulatorSubsystem::Tick(float DeltaTime)
{
	// NOTE: Simulation moved to HandlePostActorTick() for correct bone transform timing
	// Reset event counter at frame start
	EventCountThisFrame.store(0, std::memory_order_relaxed);
}

/**
 * @brief Main simulation entry point triggered after actor animation evaluation.
 * @param World The world being ticked.
 * @param TickType Type of level tick being performed.
 * @param DeltaTime The time elapsed since the last frame.
 */
void UKawaiiFluidSimulatorSubsystem::HandlePostActorTick(UWorld* World, ELevelTick TickType, float DeltaTime)
{
	// Only process for our world
	if (World != GetWorld())
	{
		return;
	}

	// Skip if paused or not a game tick
	if (TickType == LEVELTICK_ViewportsOnly)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_SubsystemTick);
	TRACE_CPUPROFILER_EVENT_SCOPE(KawaiiFluidSubsystem_PostActorTick);

	//========================================
	// Module-based simulation (runs AFTER animation evaluation)
	// This ensures bone transforms are up-to-date for attachment following
	//========================================
	if (AllModules.Num() > 0)
	{
		SimulateIndependentFluidComponents(DeltaTime);
		SimulateBatchedFluidComponents(DeltaTime);

		//========================================
		// Collision Feedback Processing (GPU + CPU)
		//========================================
		// Build OwnerID -> InteractionComponent map (once, for O(1) lookup)
		TMap<int32, UKawaiiFluidInteractionComponent*> OwnerIDToIC;
		OwnerIDToIC.Reserve(GlobalInteractionComponents.Num());
		for (UKawaiiFluidInteractionComponent* IC : GlobalInteractionComponents)
		{
			if (IC && IC->GetOwner())
			{
				OwnerIDToIC.Add(IC->GetOwner()->GetUniqueID(), IC);
			}
		}

		// Call ProcessCollisionFeedback for each Module
		for (UKawaiiFluidSimulationModule* Module : AllModules)
		{
			if (Module && Module->bEnableCollisionEvents)
			{
				Module->ProcessCollisionFeedback(OwnerIDToIC, CPUCollisionFeedbackBuffer);
			}
		}

		// Clear CPU buffer
		CPUCollisionFeedbackBuffer.Reset();
	}
}

/**
 * @brief Register a new simulation module with the subsystem.
 * @param Module The module to register.
 */
void UKawaiiFluidSimulatorSubsystem::RegisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module && !AllModules.Contains(Module))
	{
		AllModules.Add(Module);

		// Allocate SourceID for per-component GPU counter tracking (0 ~ MaxSourceCount-1)
		const int32 NewSourceID = AllocateSourceID();
		Module->SetSourceID(NewSourceID);

		// Early GPU setup: Initialize GPU state at registration time
		UKawaiiFluidPresetDataAsset* Preset = Module->GetPreset();

		if (Preset)
		{
			UKawaiiFluidVolumeComponent* TargetVolume = Module->GetTargetVolumeComponent();
			UKawaiiFluidSimulationContext* Context = GetOrCreateContext(TargetVolume, Preset);
			if (Context)
			{
				Module->SetSimulationContext(Context);

				if (!Context->GetCachedPreset())
				{
					Context->SetCachedPreset(Preset);
				}

				if (!Context->IsGPUSimulatorReady() && TargetVolume)
				{
					Context->InitializeGPUSimulator(TargetVolume->MaxParticleCount);
				}

				if (Context->IsGPUSimulatorReady())
				{
					Module->SetGPUSimulator(Context->GetGPUSimulatorShared());
					Module->SetGPUSimulationActive(true);
					Module->UploadCPUParticlesToGPU();
				}

				if (!Context->HasValidRenderResource())
				{
					Context->InitializeRenderResource();
				}

				if (AKawaiiFluidVolume* OwnerVolume = Cast<AKawaiiFluidVolume>(Module->GetOuter()))
				{
					if (UKawaiiFluidRenderingModule* RenderingMod = OwnerVolume->GetRenderingModule())
					{
						if (UKawaiiFluidRenderer* MR = RenderingMod->GetMetaballRenderer())
						{
							MR->SetSimulationContext(Context);
						}
					}
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("SimulationModule registered: SourceID=%d, GPUActive=%d"),
			NewSourceID, Module->IsGPUSimulationActive() ? 1 : 0);
	}
}

/**
 * @brief Unregister a simulation module and release its allocated resources.
 * @param Module The module to unregister.
 */
void UKawaiiFluidSimulatorSubsystem::UnregisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module)
	{
		const int32 SourceID = Module->GetSourceID();
		if (SourceID >= 0)
		{
			ReleaseSourceID(SourceID);
			Module->SetSourceID(EGPUParticleSource::InvalidSourceID);
		}
	}

	AllModules.Remove(Module);
	UE_LOG(LogTemp, Verbose, TEXT("SimulationModule unregistered"));
}

/**
 * @brief Allocate a unique SourceID for tracking a particle source on the GPU.
 * @return An integer index (0 ~ MaxSourceCount-1) or InvalidSourceID if no slots are available.
 */
int32 UKawaiiFluidSimulatorSubsystem::AllocateSourceID()
{
	if (UsedSourceIDs.Num() == 0)
	{
		UsedSourceIDs.Init(false, EGPUParticleSource::MaxSourceCount);
	}

	for (int32 i = 0; i < EGPUParticleSource::MaxSourceCount; ++i)
	{
		const int32 Index = (NextSourceIDHint + i) % EGPUParticleSource::MaxSourceCount;
		if (!UsedSourceIDs[Index])
		{
			UsedSourceIDs[Index] = true;
			NextSourceIDHint = (Index + 1) % EGPUParticleSource::MaxSourceCount;
			return Index;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Subsystem::AllocateSourceID FAILED - all %d slots in use!"), EGPUParticleSource::MaxSourceCount);
	return EGPUParticleSource::InvalidSourceID;
}

/**
 * @brief Release a SourceID back to the pool for reuse by other components.
 * @param SourceID The ID to release.
 */
void UKawaiiFluidSimulatorSubsystem::ReleaseSourceID(int32 SourceID)
{
	if (SourceID >= 0 && SourceID < UsedSourceIDs.Num())
	{
		UsedSourceIDs[SourceID] = false;
	}
}

/**
 * @brief Register a fluid volume actor which acts as a solver container.
 * @param Volume The volume actor to register.
 */
void UKawaiiFluidSimulatorSubsystem::RegisterVolume(AKawaiiFluidVolume* Volume)
{
	if (Volume && !AllVolumes.Contains(Volume))
	{
		AllVolumes.Add(Volume);
		UE_LOG(LogTemp, Log, TEXT("KawaiiFluidVolume registered: %s"), *Volume->GetName());
	}
}

/**
 * @brief Unregister a fluid volume actor.
 * @param Volume The volume actor to unregister.
 */
void UKawaiiFluidSimulatorSubsystem::UnregisterVolume(AKawaiiFluidVolume* Volume)
{
	AllVolumes.Remove(Volume);
}

/**
 * @brief Register a legacy simulation volume component.
 * @param VolumeComponent The component to register.
 */
void UKawaiiFluidSimulatorSubsystem::RegisterVolumeComponent(UKawaiiFluidVolumeComponent* VolumeComponent)
{
	if (VolumeComponent && !AllVolumeComponents.Contains(VolumeComponent))
	{
		AllVolumeComponents.Add(VolumeComponent);
		UE_LOG(LogTemp, Log, TEXT("VolumeComponent registered: %s"), *VolumeComponent->GetName());
	}
}

/**
 * @brief Unregister a legacy simulation volume component.
 * @param VolumeComponent The component to unregister.
 */
void UKawaiiFluidSimulatorSubsystem::UnregisterVolumeComponent(UKawaiiFluidVolumeComponent* VolumeComponent)
{
	AllVolumeComponents.Remove(VolumeComponent);
}

/**
 * @brief Register a collider that should affect all fluid simulations globally.
 * @param Collider The collider component to register.
 */
void UKawaiiFluidSimulatorSubsystem::RegisterGlobalCollider(UKawaiiFluidCollider* Collider)
{
	if (Collider && !GlobalColliders.Contains(Collider))
	{
		GlobalColliders.Add(Collider);
	}
}

/**
 * @brief Unregister a global collider.
 * @param Collider The collider component to unregister.
 */
void UKawaiiFluidSimulatorSubsystem::UnregisterGlobalCollider(UKawaiiFluidCollider* Collider)
{
	GlobalColliders.Remove(Collider);
}

/**
 * @brief Register an interaction component for global bone tracking.
 * @param Component The interaction component to register.
 */
void UKawaiiFluidSimulatorSubsystem::RegisterGlobalInteractionComponent(UKawaiiFluidInteractionComponent* Component)
{
	if (Component && !GlobalInteractionComponents.Contains(Component))
	{
		GlobalInteractionComponents.Add(Component);
	}
}

/**
 * @brief Unregister a global interaction component.
 * @param Component The interaction component to unregister.
 */
void UKawaiiFluidSimulatorSubsystem::UnregisterGlobalInteractionComponent(UKawaiiFluidInteractionComponent* Component)
{
	GlobalInteractionComponents.Remove(Component);
}

/**
 * @brief Retrieve all fluid particles within a specified radius of a world location.
 * @param Location The center of the search sphere.
 * @param Radius The radius of the search sphere.
 * @return Array of particles within the radius.
 */
TArray<FKawaiiFluidParticle> UKawaiiFluidSimulatorSubsystem::GetAllParticlesInRadius(FVector Location, float Radius) const
{
	TArray<FKawaiiFluidParticle> Result;
	const float RadiusSq = Radius * Radius;

	for (const UKawaiiFluidSimulationModule* Module : AllModules)
	{
		if (!Module) continue;

		for (const FKawaiiFluidParticle& Particle : Module->GetParticles())
		{
			if (FVector::DistSquared(Particle.Position, Location) <= RadiusSq)
			{
				Result.Add(Particle);
			}
		}
	}

	return Result;
}

/**
 * @brief Get the total number of fluid particles currently simulated in the world.
 * @return Total particle count.
 */
int32 UKawaiiFluidSimulatorSubsystem::GetTotalParticleCount() const
{
	int32 Total = 0;
	for (const UKawaiiFluidSimulationModule* Module : AllModules)
	{
		if (Module) Total += Module->GetParticleCount();
	}
	return Total;
}

/**
 * @brief Find the preset data asset associated with a specific SourceID.
 * @param SourceID The source slot ID to look up.
 * @return The associated preset, or nullptr if not found.
 */
UKawaiiFluidPresetDataAsset* UKawaiiFluidSimulatorSubsystem::GetPresetBySourceID(int32 SourceID) const
{
	UKawaiiFluidSimulationModule* Module = GetModuleBySourceID(SourceID);
	return Module ? Module->GetPreset() : nullptr;
}

/**
 * @brief Find the simulation module associated with a specific SourceID.
 * @param SourceID The source slot ID to look up.
 * @return The associated module, or nullptr if not found.
 */
UKawaiiFluidSimulationModule* UKawaiiFluidSimulatorSubsystem::GetModuleBySourceID(int32 SourceID) const
{
	if (SourceID < 0) return nullptr;

	for (UKawaiiFluidSimulationModule* Module : AllModules)
	{
		if (Module && Module->GetSourceID() == SourceID) return Module;
	}

	for (UKawaiiFluidSimulationModule* Module : AllModules)
	{
		if (Module)
		{
			UObject* Outer = Module->GetOuter();
			if (Outer && Outer->GetUniqueID() == SourceID) return Module;
		}
	}

	return nullptr;
}

/**
 * @brief Collect all active GPU simulator instances for deferred execution.
 * @param OutSimulators Array to be populated with simulator pointers.
 */
void UKawaiiFluidSimulatorSubsystem::GetAllGPUSimulators(TArray<FGPUFluidSimulator*>& OutSimulators) const
{
	OutSimulators.Reset();

	for (const auto& Pair : ContextCache)
	{
		if (UKawaiiFluidSimulationContext* Context = Pair.Value)
		{
			if (FGPUFluidSimulator* Simulator = Context->GetGPUSimulator())
			{
				OutSimulators.AddUnique(Simulator);
			}
		}
	}

	if (DefaultContext)
	{
		if (FGPUFluidSimulator* Simulator = DefaultContext->GetGPUSimulator())
		{
			OutSimulators.AddUnique(Simulator);
		}
	}
}

/**
 * @brief Get an existing simulation context or create a new one for a volume/preset pair.
 * @param VolumeComponent The volume component defining the spatial hash bounds.
 * @param Preset The preset defining physics parameters.
 * @return The matching simulation context instance.
 */
UKawaiiFluidSimulationContext* UKawaiiFluidSimulatorSubsystem::GetOrCreateContext(
	UKawaiiFluidVolumeComponent* VolumeComponent, UKawaiiFluidPresetDataAsset* Preset)
{
	if (!Preset) return DefaultContext;

	FContextCacheKey CacheKey(VolumeComponent, Preset);
	if (TObjectPtr<UKawaiiFluidSimulationContext>* Found = ContextCache.Find(CacheKey))
	{
		return *Found;
	}

	UKawaiiFluidSimulationContext* NewContext = NewObject<UKawaiiFluidSimulationContext>(this);
	NewContext->InitializeSolvers(Preset);
	NewContext->SetTargetVolumeComponent(VolumeComponent);

	ContextCache.Add(CacheKey, NewContext);

	return NewContext;
}

/**
 * @brief Simulate fluid modules that are configured for independent (non-batched) simulation.
 * @param DeltaTime Frame delta time.
 */
void UKawaiiFluidSimulatorSubsystem::SimulateIndependentFluidComponents(float DeltaTime)
{
	for (UKawaiiFluidSimulationModule* Module : AllModules)
	{
		if (!Module || !Module->IsSimulationEnabled() || Module->GetParticleCount() == 0 || !Module->IsIndependentSimulation())
		{
			continue;
		}

		UKawaiiFluidPresetDataAsset* EffectivePreset = Module->GetPreset();
		if (!EffectivePreset) continue;

		UKawaiiFluidVolumeComponent* TargetVolume = Module->GetTargetVolumeComponent();
		UKawaiiFluidSimulationContext* Context = GetOrCreateContext(TargetVolume, EffectivePreset);
		if (!Context) continue;

		FKawaiiFluidSpatialHash* SpatialHash = Module->GetSpatialHash();
		if (!SpatialHash) continue;

		FKawaiiFluidSimulationParams Params = Module->BuildSimulationParams();
		Params.Colliders.Append(GlobalColliders);
		Params.InteractionComponents.Append(GlobalInteractionComponents);
		Params.CPUCollisionFeedbackBufferPtr = &CPUCollisionFeedbackBuffer;
		Params.CPUCollisionFeedbackLockPtr = &CPUCollisionFeedbackLock;

		if (!Context->IsGPUSimulatorReady() && TargetVolume)
		{
			Context->InitializeGPUSimulator(TargetVolume->MaxParticleCount);
		}

		if (Context->IsGPUSimulatorReady())
		{
			Module->SetGPUSimulator(Context->GetGPUSimulatorShared());
			Module->SetGPUSimulationActive(true);
		}

		TArray<FKawaiiFluidParticle>& Particles = Module->GetParticlesMutable();
		float AccumulatedTime = Module->GetAccumulatedTime();

		Context->Simulate(Particles, EffectivePreset, Params, *SpatialHash, DeltaTime, AccumulatedTime);

		Module->SetAccumulatedTime(AccumulatedTime);
		Module->ResetExternalForce();
	}
}

/**
 * @brief Group and simulate fluid modules that share the same volume and preset.
 * @param DeltaTime Frame delta time.
 */
void UKawaiiFluidSimulatorSubsystem::SimulateBatchedFluidComponents(float DeltaTime)
{
	TMap<FContextCacheKey, TArray<TObjectPtr<UKawaiiFluidSimulationModule>>> ContextGroups = GroupModulesByContext();

	for (auto& Pair : ContextGroups)
	{
		const FContextCacheKey& CacheKey = Pair.Key;
		TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& Modules = Pair.Value;

		UKawaiiFluidPresetDataAsset* Preset = CacheKey.Preset;
		if (!Preset || Modules.Num() == 0) continue;

		UKawaiiFluidSimulationContext* Context = GetOrCreateContext(CacheKey.VolumeComponent, Preset);
		if (!Context) continue;

		SharedSpatialHash->SetCellSize(Preset->SmoothingRadius);
		MergeModuleParticles(Modules);

		if (MergedFluidParticleBuffer.Num() == 0) continue;

		FKawaiiFluidSimulationParams Params = BuildMergedModuleSimulationParams(Modules);
		Params.Colliders.Append(GlobalColliders);
		Params.InteractionComponents.Append(GlobalInteractionComponents);
		Params.CPUCollisionFeedbackBufferPtr = &CPUCollisionFeedbackBuffer;
		Params.CPUCollisionFeedbackLockPtr = &CPUCollisionFeedbackLock;

		if (!Context->IsGPUSimulatorReady() && CacheKey.VolumeComponent)
		{
			Context->InitializeGPUSimulator(CacheKey.VolumeComponent->MaxParticleCount);
		}

		if (Context->IsGPUSimulatorReady())
		{
			TSharedPtr<FGPUFluidSimulator> BatchGPUSimulator = Context->GetGPUSimulatorShared();
			for (UKawaiiFluidSimulationModule* Module : Modules)
			{
				if (Module)
				{
					Module->SetGPUSimulator(BatchGPUSimulator);
					Module->SetGPUSimulationActive(true);
				}
			}
		}

		float AccumulatedTime = 0.0f;
		if (Modules.Num() > 0 && Modules[0]) AccumulatedTime = Modules[0]->GetAccumulatedTime();

		Context->Simulate(MergedFluidParticleBuffer, Preset, Params, *SharedSpatialHash, DeltaTime, AccumulatedTime);

		for (UKawaiiFluidSimulationModule* Module : Modules)
		{
			if (Module)
			{
				Module->SetAccumulatedTime(AccumulatedTime);
				Module->ResetExternalForce();
			}
		}

		SplitModuleParticles(Modules);
	}
}

/**
 * @brief Group simulation modules based on their associated context key (Volume + Preset).
 * @return Map of context keys to arrays of modules.
 */
TMap<FContextCacheKey, TArray<TObjectPtr<UKawaiiFluidSimulationModule>>>
UKawaiiFluidSimulatorSubsystem::GroupModulesByContext() const
{
	TMap<FContextCacheKey, TArray<TObjectPtr<UKawaiiFluidSimulationModule>>> Result;

	for (UKawaiiFluidSimulationModule* Module : AllModules)
	{
		if (Module && Module->IsSimulationEnabled() && !Module->IsIndependentSimulation() && 
			Module->GetParticleCount() > 0 && Module->GetPreset())
		{
			UKawaiiFluidVolumeComponent* TargetVolume = Module->GetTargetVolumeComponent();
			FContextCacheKey Key(TargetVolume, Module->GetPreset());
			Result.FindOrAdd(Key).Add(Module);
		}
	}

	return Result;
}

/**
 * @brief Merge particles from multiple modules into a single unified buffer for batched simulation.
 * @param Modules The modules whose particles should be merged.
 */
void UKawaiiFluidSimulatorSubsystem::MergeModuleParticles(const TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& Modules)
{
	int32 TotalParticles = 0;
	for (const UKawaiiFluidSimulationModule* Module : Modules)
	{
		if (Module) TotalParticles += Module->GetParticleCount();
	}

	MergedFluidParticleBuffer.Reset(TotalParticles);
	ModuleBatchInfos.Reset(Modules.Num());

	int32 CurrentOffset = 0;
	for (UKawaiiFluidSimulationModule* Module : Modules)
	{
		if (!Module) continue;

		const TArray<FKawaiiFluidParticle>& Particles = Module->GetParticles();
		const int32 Count = Particles.Num();

		ModuleBatchInfos.Add(FKawaiiFluidModuleBatchInfo(Module, CurrentOffset, Count));

		for (const FKawaiiFluidParticle& Particle : Particles)
		{
			MergedFluidParticleBuffer.Add(Particle);
		}

		CurrentOffset += Count;
	}
}

/**
 * @brief Split particles from the merged buffer back into their respective modules after simulation.
 * @param Modules The modules to receive their updated particles.
 */
void UKawaiiFluidSimulatorSubsystem::SplitModuleParticles(const TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& Modules)
{
	for (const FKawaiiFluidModuleBatchInfo& Info : ModuleBatchInfos)
	{
		UKawaiiFluidSimulationModule* Module = Info.Module;
		if (!Module) continue;

		TArray<FKawaiiFluidParticle>& Particles = Module->GetParticlesMutable();
		if (Particles.Num() != Info.ParticleCount) continue;

		for (int32 i = 0; i < Info.ParticleCount; ++i)
		{
			Particles[i] = MergedFluidParticleBuffer[Info.StartIndex + i];
		}
	}
}

/**
 * @brief Construct a unified simulation parameters structure for a batched simulation pass.
 * @param Modules The modules to aggregate parameters from.
 * @return Unified simulation parameters.
 */
FKawaiiFluidSimulationParams UKawaiiFluidSimulatorSubsystem::BuildMergedModuleSimulationParams(
	const TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& Modules)
{
	FKawaiiFluidSimulationParams Params;
	Params.World = GetWorld();
	Params.EventCountPtr = &EventCountThisFrame;

	FVector TotalForce = FVector::ZeroVector;
	bool bAnyUseWorldCollision = false;

	for (const UKawaiiFluidSimulationModule* Module : Modules)
	{
		if (!Module) continue;

		TotalForce += Module->GetAccumulatedExternalForce();
		Params.Colliders.Append(Module->GetColliders());
		if (Module->bUseWorldCollision) bAnyUseWorldCollision = true;
	}

	Params.ExternalForce = TotalForce;
	Params.InteractionComponents.Append(GlobalInteractionComponents);
	Params.bUseWorldCollision = bAnyUseWorldCollision;

	if (Modules.Num() > 0 && Modules[0])
	{
		UKawaiiFluidPresetDataAsset* Preset = Modules[0]->GetPreset();
		if (Preset) Params.ParticleRadius = Preset->ParticleRadius;
	}

	if (UWorld* World = GetWorld()) Params.CurrentGameTime = World->GetTimeSeconds();

	return Params;
}

/**
 * @brief Internal handler for actor spawn events to invalidate collision caches.
 */
void UKawaiiFluidSimulatorSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (!Actor || Actor->GetWorld() != GetWorld()) return;
	MarkAllContextsWorldCollisionDirty();
}

/**
 * @brief Internal handler for actor destruction events to invalidate collision caches.
 */
void UKawaiiFluidSimulatorSubsystem::HandleActorDestroyed(AActor* Actor)
{
	if (!Actor || Actor->GetWorld() != GetWorld()) return;
	MarkAllContextsWorldCollisionDirty();
}

/**
 * @brief Internal handler for level addition to invalidate collision caches.
 */
void UKawaiiFluidSimulatorSubsystem::HandleLevelAdded(ULevel* InLevel, UWorld* InWorld)
{
	if (!InWorld || InWorld != GetWorld()) return;
	MarkAllContextsWorldCollisionDirty();
}

/**
 * @brief Internal handler for level removal to invalidate collision caches.
 */
void UKawaiiFluidSimulatorSubsystem::HandleLevelRemoved(ULevel* InLevel, UWorld* InWorld)
{
	if (!InWorld || InWorld != GetWorld()) return;
	MarkAllContextsWorldCollisionDirty();
}

/**
 * @brief Mark all active simulation contexts as having dirty world collision data.
 */
void UKawaiiFluidSimulatorSubsystem::MarkAllContextsWorldCollisionDirty()
{
	for (TPair<FContextCacheKey, TObjectPtr<UKawaiiFluidSimulationContext>>& Pair : ContextCache)
	{
		if (Pair.Value) Pair.Value->MarkGPUWorldCollisionCacheDirty();
	}

	if (DefaultContext) DefaultContext->MarkGPUWorldCollisionCacheDirty();
}
