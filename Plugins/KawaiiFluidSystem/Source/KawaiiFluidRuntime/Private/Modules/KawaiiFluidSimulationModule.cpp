// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Modules/KawaiiFluidSimulationModule.h"

#include "KawaiiFluidSimulationContext.h"
#include "Core/KawaiiFluidSpatialHash.h"
#include "Collision/KawaiiFluidCollider.h"
#include "Components/KawaiiFluidInteractionComponent.h"
#include "Components/KawaiiFluidVolumeComponent.h"
#include "Actors/KawaiiFluidVolume.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Simulation/GPUFluidSimulator.h"
#include "Simulation/Shaders/GPUFluidSimulatorShaders.h"  // For GPU_MORTON_GRID_AXIS_BITS
#include "Simulation/Resources/GPUFluidParticle.h"  // For FGPUSpawnRequest
#include "UObject/UObjectGlobals.h"  // For FCoreUObjectDelegates
#include "UObject/ObjectSaveContext.h"  // For FObjectPreSaveContext
#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"  // For FEditorDelegates
#endif

/**
 * @brief Default constructor initializing default volume dimensions.
 */
UKawaiiFluidSimulationModule::UKawaiiFluidSimulationModule()
{
	const float MediumGridResolution = static_cast<float>(GridResolutionPresetHelper::GetGridResolution(EGridResolutionPreset::Medium));
	const float DefaultVolumeSize = MediumGridResolution * CellSize;
	UniformVolumeSize = DefaultVolumeSize;
	VolumeSize = FVector(DefaultVolumeSize);

	RecalculateVolumeBounds();
}

/**
 * @brief Handles delegate binding after property initialization.
 */
void UKawaiiFluidSimulationModule::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject) && !PreBeginPIEHandle.IsValid())
	{
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &UKawaiiFluidSimulationModule::OnPreBeginPIE);
	}
#endif
}

/**
 * @brief Handles editor delegate binding and volume info synchronization after loading.
 */
void UKawaiiFluidSimulationModule::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (Preset)
	{
		BindToPresetPropertyChanged();
	}

	BindToObjectsReplaced();

	if (!PreBeginPIEHandle.IsValid())
	{
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &UKawaiiFluidSimulationModule::OnPreBeginPIE);
	}

	UpdateVolumeInfoDisplay();
#endif
}

/**
 * @brief Clears stale pointers and re-initializes state after duplication.
 * @param bDuplicateForPIE Whether the duplication is for a PIE session.
 */
void UKawaiiFluidSimulationModule::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	CachedSimulationContext = nullptr;
	WeakGPUSimulator.Reset();
	bGPUSimulationActive = false;

	const int32 PreservedParticleCount = Particles.Num();

	SpatialHash.Reset();

	OwnedVolumeComponent = nullptr;
	PreviousRegisteredVolume.Reset();

	bIsInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidSimulationModule::PostDuplicate - Preserved %d particles, cleared stale pointers (PIE=%d)"),
		PreservedParticleCount, bDuplicateForPIE ? 1 : 0);
}

//========================================
// Serialization (PreSave)
//========================================

/**
 * @brief Synchronizes GPU data to the CPU buffer before saving the object.
 * @param SaveContext Context for the save operation.
 */
void UKawaiiFluidSimulationModule::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	SyncGPUParticlesToCPU();
}

//========================================
// GPU <-> CPU Particle Sync
//========================================

/**
 * @brief Downloads particles belonging to this module from the GPU buffer to the CPU array.
 */
void UKawaiiFluidSimulationModule::SyncGPUParticlesToCPU()
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (!GPUSim)
	{
		return;
	}

	const int32 GPUCount = GPUSim->GetParticleCount();
	if (GPUCount <= 0)
	{
		return;
	}

	TArray<FKawaiiFluidParticle> MyParticles;

	if (GPUSim->IsReady())
	{
		if (GPUSim->GetParticlesBySourceID(CachedSourceID, MyParticles))
		{
			Particles = MoveTemp(MyParticles);
		}
	}
}

/**
 * @brief Uploads existing CPU particles to the GPU simulator and triggers an initialization simulation pass.
 */
void UKawaiiFluidSimulationModule::UploadCPUParticlesToGPU()
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (!GPUSim || !GPUSim->IsReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("UploadCPUParticlesToGPU: GPUSimulator not ready, %d particles waiting"), Particles.Num());
		return;
	}

	const int32 UploadCount = Particles.Num();
	int32 StartID = 0;

	// Upload particles if any
	if (UploadCount > 0)
	{
		// Atomic ID assignment: ignore stored IDs and assign new ones (prevent multi-module collision + overflow reset)
		StartID = GPUSim->AllocateParticleIDs(UploadCount);
		for (int32 i = 0; i < UploadCount; ++i)
		{
			Particles[i].ParticleID = StartID + i;
			Particles[i].SourceID = CachedSourceID;
		}

		// Upload from CPU to GPU (bAppend=true: preserve particles from other components in batching environment)
		GPUSim->UploadParticles(Particles, /*bAppend=*/true);

		// After all appends, create/update GPU buffer + reset SpawnManager state
		GPUSim->FinalizeUpload();
	}

	// Run initialization simulation to preload collision/landscape data (runs even with 0 particles)
	if (UKawaiiFluidSimulationContext* Context = GetSimulationContext())
	{
		FKawaiiFluidSpatialHash* Hash = GetSpatialHash();
		if (!Hash)
		{
			// SpatialHash not initialized yet - initialize now
			InitializeSpatialHash(Preset->SmoothingRadius);
			Hash = GetSpatialHash();
		}

		if (Hash)
		{
			// Build full simulation params (same as normal simulation)
			FKawaiiFluidSimulationParams Params = BuildSimulationParams();

			// Prepare temporary variables for Simulate
			TArray<FKawaiiFluidParticle> EmptyParticles;  // GPU-only mode doesn't need CPU particles
			float TempAccumulatedTime = 0.0f;

			// Run one simulation frame (1 substep) to:
			// - Upload collision primitives to GPU
			// - Upload landscape heightmap to GPU
			// - Stabilize particles + calculate anisotropy (if any)
			Context->Simulate(EmptyParticles, Preset, Params, *Hash, Preset->SubstepDeltaTime, TempAccumulatedTime);
		}
	}

	// Clear CPU array after uploading to GPU (prevent duplicate uploads + save memory)
	if (UploadCount > 0)
	{
		Particles.Empty();
		UE_LOG(LogTemp, Log, TEXT("UploadCPUParticlesToGPU: Uploaded %d particles (SourceID=%d, IDs=%d~%d) to GPU"),
			UploadCount, CachedSourceID, StartID, StartID + UploadCount - 1);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UploadCPUParticlesToGPU: No particles to upload, ran initialization simulation for collision/landscape preload"));
	}
}

/**
 * @brief Performs cleanup before the object is destroyed.
 */
void UKawaiiFluidSimulationModule::BeginDestroy()
{
#if WITH_EDITOR
	UnbindFromPresetPropertyChanged();

	UnbindFromObjectsReplaced();

	if (PreBeginPIEHandle.IsValid())
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		PreBeginPIEHandle.Reset();
	}
#endif

	UnbindFromVolumeDestroyedEvent();

	Super::BeginDestroy();
}

#if WITH_EDITOR
/**
 * @brief Syncs GPU data to CPU before PIE starts.
 */
void UKawaiiFluidSimulationModule::OnPreBeginPIE(bool bIsSimulating)
{
	SyncGPUParticlesToCPU();

	UE_LOG(LogTemp, Log, TEXT("OnPreBeginPIE: Synced %d particles for PIE transfer"), Particles.Num());
}
#endif

#if WITH_EDITOR
/**
 * @brief Handles logic when a property is modified in the Unreal Editor.
 */
void UKawaiiFluidSimulationModule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// When Preset changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, Preset))
	{
		UnbindFromPresetPropertyChanged();

			if (Preset)
		{
			if (SpatialHash.IsValid())
			{
				SpatialHash = MakeShared<FKawaiiFluidSpatialHash>(Preset->SmoothingRadius);
			}
			BindToPresetPropertyChanged();
		}
		UpdateVolumeInfoDisplay();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, TargetSimulationVolume))
	{
		UnbindFromVolumeDestroyedEvent();

		if (UKawaiiFluidVolumeComponent* PrevVolume = PreviousRegisteredVolume.Get())
		{
			PrevVolume->UnregisterModule(this);
		}

		if (TargetSimulationVolume)
		{
			if (UKawaiiFluidVolumeComponent* NewVolume = TargetSimulationVolume->GetVolumeComponent())
			{
				NewVolume->RegisterModule(this);
				PreviousRegisteredVolume = NewVolume;
			}
			BindToVolumeDestroyedEvent();
		}
		else
		{
			PreviousRegisteredVolume = nullptr;
		}

		UpdateVolumeInfoDisplay();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, GridResolutionPreset))
	{
		if (!TargetSimulationVolume)
		{
			UpdateVolumeInfoDisplay();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, bUniformSize))
	{
		if (!TargetSimulationVolume)
		{
			if (bUniformSize)
			{
				UniformVolumeSize = FMath::Max3(VolumeSize.X, VolumeSize.Y, VolumeSize.Z);
			}
			else
			{
				VolumeSize = FVector(UniformVolumeSize);
			}
			RecalculateVolumeBounds();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, UniformVolumeSize))
	{
		if (!TargetSimulationVolume)
		{
			const float EffectiveCellSize = FMath::Max(CellSize, 1.0f);
			const float MaxFullSize = GridResolutionPresetHelper::GetMaxExtentForPreset(EGridResolutionPreset::Large, EffectiveCellSize) * 2.0f;
			UniformVolumeSize = FMath::Clamp(UniformVolumeSize, 10.0f, MaxFullSize);
			RecalculateVolumeBounds();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, VolumeSize))
	{
		if (!TargetSimulationVolume)
		{
			const float EffectiveCellSize = FMath::Max(CellSize, 1.0f);
			const float MaxFullSize = GridResolutionPresetHelper::GetMaxExtentForPreset(EGridResolutionPreset::Large, EffectiveCellSize) * 2.0f;
			VolumeSize.X = FMath::Clamp(VolumeSize.X, 10.0f, MaxFullSize);
			VolumeSize.Y = FMath::Clamp(VolumeSize.Y, 10.0f, MaxFullSize);
			VolumeSize.Z = FMath::Clamp(VolumeSize.Z, 10.0f, MaxFullSize);
			RecalculateVolumeBounds();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, VolumeRotation))
	{
		if (!TargetSimulationVolume)
		{
			RecalculateVolumeBounds();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationModule, CellSize))
	{
		if (!TargetSimulationVolume)
		{
			const float MediumGridResolution = static_cast<float>(GridResolutionPresetHelper::GetGridResolution(EGridResolutionPreset::Medium));
			const float NewDefaultSize = MediumGridResolution * CellSize;
			UniformVolumeSize = NewDefaultSize;
			VolumeSize = FVector(NewDefaultSize);
			RecalculateVolumeBounds();
		}
	}
}
#endif

/**
 * @brief Initialize the simulation module with a specific preset.
 * @param InPreset Data asset defining the fluid behavior.
 */
void UKawaiiFluidSimulationModule::Initialize(UKawaiiFluidPresetDataAsset* InPreset)
{
	if (bIsInitialized)
	{
		return;
	}

	Preset = InPreset;

	float SpatialHashCellSize = 20.0f;
	if (Preset)
	{
		SpatialHashCellSize = Preset->SmoothingRadius;
		if (Preset->SmoothingRadius > 0.0f)
		{
			CellSize = Preset->SmoothingRadius;
		}
	}
	InitializeSpatialHash(SpatialHashCellSize);

	if (!OwnedVolumeComponent)
	{
		OwnedVolumeComponent = NewObject<UKawaiiFluidVolumeComponent>(this, NAME_None, RF_Transient);
		OwnedVolumeComponent->CellSize = CellSize;
		OwnedVolumeComponent->bShowBoundsInEditor = false;
		OwnedVolumeComponent->bShowBoundsAtRuntime = false;
		OwnedVolumeComponent->SetVisibility(false);
		OwnedVolumeComponent->SetHiddenInGame(true);
	}

	RecalculateVolumeBounds();

	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidSimulationModule initialized"));
}

/**
 * @brief Clears simulation data and releases associated resources.
 */
void UKawaiiFluidSimulationModule::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	ClearAllParticles();
	Particles.Empty();
	Colliders.Empty();
	SpatialHash.Reset();
	Preset = nullptr;
	OwnedVolumeComponent = nullptr;
	TargetSimulationVolume = nullptr;

	bIsInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidSimulationModule shutdown"));
}

/**
 * @brief Update the fluid preset and reconstruct associated search structures.
 * @param InPreset New preset data asset.
 */
void UKawaiiFluidSimulationModule::SetPreset(UKawaiiFluidPresetDataAsset* InPreset)
{
	Preset = InPreset;

	if (Preset && SpatialHash.IsValid())
	{
		SpatialHash = MakeShared<FKawaiiFluidSpatialHash>(Preset->SmoothingRadius);
	}
}

/**
 * @brief Allocate a new spatial hash for CPU neighbor lookup.
 * @param InCellSize Dimension of the spatial cells.
 */
void UKawaiiFluidSimulationModule::InitializeSpatialHash(float InCellSize)
{
	SpatialHash = MakeShared<FKawaiiFluidSpatialHash>(InCellSize);
}

/**
 * @brief Get the actor that owns this module.
 * @return AActor pointer.
 */
AActor* UKawaiiFluidSimulationModule::GetOwnerActor() const
{
	if (UActorComponent* OwnerComp = Cast<UActorComponent>(GetOuter()))
	{
		return OwnerComp->GetOwner();
	}
	return nullptr;
}

/**
 * @brief Construct frame-specific simulation parameters for the solver.
 * @return FKawaiiFluidSimulationParams structure.
 */
FKawaiiFluidSimulationParams UKawaiiFluidSimulationModule::BuildSimulationParams() const
{
	FKawaiiFluidSimulationParams Params;

	// External forces
	Params.ExternalForce = AccumulatedExternalForce;

	// Colliders / Interaction components
	Params.Colliders = Colliders;

	// Get particle radius from Preset
	if (Preset)
	{
		Params.ParticleRadius = GetParticleRadius();  // Use getter to respect override
	}

	// Context - Direct access from Module (utilizing Outer chain)
	Params.World = GetWorld();
	Params.IgnoreActor = GetOwnerActor();
	Params.bUseWorldCollision = bUseWorldCollision;

	// Get owner component for simulation origin and static boundary settings
	if (UKawaiiFluidVolumeComponent* VolumeComp = GetTargetVolumeComponent())
	{
		// Volume-based simulation: use VolumeComponent's static boundary settings
		Params.SimulationOrigin = VolumeComp->GetComponentLocation();

		// Static boundary particles (Akinci 2012) - density contribution from walls/floors
		// Default is false due to known issue with particles flying around chaotically
		Params.bEnableStaticBoundaryParticles = VolumeComp->IsStaticBoundaryParticlesEnabled();
		Params.StaticBoundaryParticleSpacing = VolumeComp->GetStaticBoundaryParticleSpacing();
	}
	else
	{
		// Fallback: disable static boundary particles by default
		Params.bEnableStaticBoundaryParticles = false;
		Params.StaticBoundaryParticleSpacing = 5.0f;
	}

	// Unified Simulation Volume for containment collision (always enabled)
	// Check if using external volume
	if (UKawaiiFluidVolumeComponent* ExternalVolume = GetTargetVolumeComponent())
	{
		if (IsUsingExternalVolume())
		{
			// Use external volume's location and user-defined size for containment
			Params.BoundsCenter = ExternalVolume->GetComponentLocation();
			Params.BoundsExtent = ExternalVolume->GetVolumeHalfExtent();
			Params.BoundsRotation = FQuat::Identity;  // External volumes are axis-aligned

			// AABB is the same as OBB for axis-aligned volumes
			Params.WorldBounds = FBox(
				Params.BoundsCenter - Params.BoundsExtent,
				Params.BoundsCenter + Params.BoundsExtent
			);

			// Use external volume's collision parameters
			Params.BoundsRestitution = ExternalVolume->GetWallBounce();
			Params.BoundsFriction = ExternalVolume->GetWallFriction();

			// Skip bounds collision when Unlimited Size mode is enabled
			Params.bSkipBoundsCollision = ExternalVolume->bUseUnlimitedSize;
		}
		else
		{
			// Internal volume - use module's own settings
			Params.BoundsCenter = VolumeCenter;
			Params.BoundsExtent = GridResolutionPresetHelper::ClampExtentToMaxSupported(GetVolumeHalfExtent(), CellSize);
			Params.BoundsRotation = VolumeRotationQuat;

			// Compute AABB from OBB for rotated internal volumes
			FVector RotatedExtents[8];
			for (int32 i = 0; i < 8; ++i)
			{
				FVector Corner(
					(i & 1) ? Params.BoundsExtent.X : -Params.BoundsExtent.X,
					(i & 2) ? Params.BoundsExtent.Y : -Params.BoundsExtent.Y,
					(i & 4) ? Params.BoundsExtent.Z : -Params.BoundsExtent.Z
				);
				RotatedExtents[i] = VolumeRotationQuat.RotateVector(Corner);
			}

			FVector AABBMin = RotatedExtents[0];
			FVector AABBMax = RotatedExtents[0];
			for (int32 i = 1; i < 8; ++i)
			{
				AABBMin = AABBMin.ComponentMin(RotatedExtents[i]);
				AABBMax = AABBMax.ComponentMax(RotatedExtents[i]);
			}

			Params.WorldBounds = FBox(VolumeCenter + AABBMin, VolumeCenter + AABBMax);
			Params.BoundsRestitution = Preset ? Preset->Bounciness : 0.0f;
			Params.BoundsFriction = Preset ? Preset->Friction : 0.5f;
		}
	}
	else
	{
		// Fallback: use internal volume settings
		Params.BoundsCenter = VolumeCenter;
		Params.BoundsExtent = GridResolutionPresetHelper::ClampExtentToMaxSupported(GetVolumeHalfExtent(), CellSize);
		Params.BoundsRotation = VolumeRotationQuat;

		// Compute AABB from OBB
		FVector RotatedExtents[8];
		for (int32 i = 0; i < 8; ++i)
		{
			FVector Corner(
				(i & 1) ? Params.BoundsExtent.X : -Params.BoundsExtent.X,
				(i & 2) ? Params.BoundsExtent.Y : -Params.BoundsExtent.Y,
				(i & 4) ? Params.BoundsExtent.Z : -Params.BoundsExtent.Z
			);
			RotatedExtents[i] = VolumeRotationQuat.RotateVector(Corner);
		}

		FVector AABBMin = RotatedExtents[0];
		FVector AABBMax = RotatedExtents[0];
		for (int32 i = 1; i < 8; ++i)
		{
			AABBMin = AABBMin.ComponentMin(RotatedExtents[i]);
			AABBMax = AABBMax.ComponentMax(RotatedExtents[i]);
		}

		Params.WorldBounds = FBox(VolumeCenter + AABBMin, VolumeCenter + AABBMax);
		Params.BoundsRestitution = Preset ? Preset->Bounciness : 0.0f;
		Params.BoundsFriction = Preset ? Preset->Friction : 0.5f;
	}

	// Event Settings
	Params.bEnableCollisionEvents = bEnableCollisionEvents;
	Params.MinVelocityForEvent = MinVelocityForEvent;
	Params.MaxEventsPerFrame = MaxEventsPerFrame;
	Params.EventCooldownPerParticle = EventCooldownPerParticle;

	if (bEnableCollisionEvents)
	{
		// Connect map for cooldown tracking (const_cast needed - alternative to mutable)
		Params.ParticleLastEventTimePtr = const_cast<TMap<int32, float>*>(&ParticleLastEventTime);

		// Current game time
		if (UWorld* World = GetWorld())
		{
			Params.CurrentGameTime = World->GetTimeSeconds();
		}

		// Callback binding
		if (OnCollisionEventCallback.IsBound())
		{
			Params.OnCollisionEvent = OnCollisionEventCallback;
		}

		// For SourceID filtering (callback only for particles spawned by this Component)
		Params.SourceID = CachedSourceID;
	}

	return Params;
}

/**
 * @brief Processes collision events collected during the simulation pass (both GPU and CPU).
 * @param OwnerIDToIC Mapping of owner IDs to interaction components for fast lookup.
 * @param CPUFeedbackBuffer Buffer containing collision events detected on the CPU.
 */
void UKawaiiFluidSimulationModule::ProcessCollisionFeedback(
	const TMap<int32, UKawaiiFluidInteractionComponent*>& OwnerIDToIC,
	const TArray<FKawaiiFluidCollisionEvent>& CPUFeedbackBuffer)
{
	if (!OnCollisionEventCallback.IsBound() || !bEnableCollisionEvents)
	{
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	int32 EventCount = 0;

	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (bGPUSimulationActive && GPUSim)
	{
		TArray<FGPUCollisionFeedback> GPUFeedbacks;
		int32 GPUFeedbackCount = 0;
		GPUSim->GetAllCollisionFeedback(GPUFeedbacks, GPUFeedbackCount);

		for (int32 i = 0; i < GPUFeedbackCount && EventCount < MaxEventsPerFrame; ++i)
		{
			const FGPUCollisionFeedback& Feedback = GPUFeedbacks[i];

			if (CachedSourceID >= 0 && Feedback.ParticleSourceID != CachedSourceID)
			{
				continue;
			}

			const float HitSpeed = FVector3f(Feedback.ParticleVelocity).Length();
			if (HitSpeed < MinVelocityForEvent)
			{
				continue;
			}

			if (const float* LastTime = ParticleLastEventTime.Find(Feedback.ParticleIndex))
			{
				if (CurrentTime - *LastTime < EventCooldownPerParticle)
				{
					continue;
				}
			}

			ParticleLastEventTime.Add(Feedback.ParticleIndex, CurrentTime);

			FKawaiiFluidCollisionEvent Event;
			Event.ParticleIndex = Feedback.ParticleIndex;
			Event.SourceID = Feedback.ParticleSourceID;
			Event.ColliderOwnerID = Feedback.ColliderOwnerID;
			Event.BoneIndex = Feedback.BoneIndex;
			Event.HitLocation = FVector(Feedback.ImpactNormal * (-Feedback.Penetration));
			Event.HitNormal = FVector(Feedback.ImpactNormal);
			Event.HitSpeed = HitSpeed;
			Event.SourceModule = const_cast<UKawaiiFluidSimulationModule*>(this);

			if (const UKawaiiFluidInteractionComponent* const* FoundIC = OwnerIDToIC.Find(Feedback.ColliderOwnerID))
			{
				Event.HitInteractionComponent = const_cast<UKawaiiFluidInteractionComponent*>(*FoundIC);
				Event.HitActor = (*FoundIC)->GetOwner();
			}

			OnCollisionEventCallback.Execute(Event);
			++EventCount;
		}
	}

	for (const FKawaiiFluidCollisionEvent& BufferEvent : CPUFeedbackBuffer)
	{
		if (EventCount >= MaxEventsPerFrame)
		{
			break;
		}

		if (CachedSourceID >= 0 && BufferEvent.SourceID != CachedSourceID)
		{
			continue;
		}

		if (const float* LastTime = ParticleLastEventTime.Find(BufferEvent.ParticleIndex))
		{
			if (CurrentTime - *LastTime < EventCooldownPerParticle)
			{
				continue;
			}
		}

		ParticleLastEventTime.Add(BufferEvent.ParticleIndex, CurrentTime);

		FKawaiiFluidCollisionEvent Event = BufferEvent;
		Event.SourceModule = const_cast<UKawaiiFluidSimulationModule*>(this);

		if (!Event.HitInteractionComponent && Event.ColliderOwnerID >= 0)
		{
			if (const UKawaiiFluidInteractionComponent* const* FoundIC = OwnerIDToIC.Find(Event.ColliderOwnerID))
			{
				Event.HitInteractionComponent = const_cast<UKawaiiFluidInteractionComponent*>(*FoundIC);
				if (!Event.HitActor)
				{
					Event.HitActor = (*FoundIC)->GetOwner();
				}
			}
		}

		OnCollisionEventCallback.Execute(Event);
		++EventCount;
	}
}

/**
 * @brief Spawns a single fluid particle at the specified location.
 * @param Position World-space position.
 * @param Velocity Initial velocity vector.
 * @return Unique identifier assigned by the GPU (async, may return -1).
 */
int32 UKawaiiFluidSimulationModule::SpawnParticle(FVector Position, FVector Velocity)
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (!GPUSim)
	{
		return -1;
	}

	const float Mass = Preset ? Preset->ParticleMass : 1.0f;
	const float Radius = Preset ? Preset->ParticleRadius : 5.0f;

	FGPUSpawnRequest Request;
	Request.Position = FVector3f(Position);
	Request.Velocity = FVector3f(Velocity);
	Request.Mass = Mass;
	Request.Radius = Radius;
	Request.SourceID = CachedSourceID;

	TArray<FGPUSpawnRequest> Requests;
	Requests.Add(Request);
	GPUSim->AddSpawnRequests(Requests);

	return -1;
}

/**
 * @brief Spawns multiple particles with a random distribution within a radius.
 * @param Location Center of the spawn region.
 * @param Count Number of particles to spawn.
 * @param SpawnRadius Radius of the random distribution.
 */
void UKawaiiFluidSimulationModule::SpawnParticles(FVector Location, int32 Count, float SpawnRadius)
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (!GPUSim)
	{
		return;
	}

	const float Mass = Preset ? Preset->ParticleMass : 1.0f;
	const float Radius = Preset ? Preset->ParticleRadius : 5.0f;

	TArray<FGPUSpawnRequest> SpawnRequests;
	SpawnRequests.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FVector RandomOffset = FMath::VRand() * FMath::FRandRange(0.0f, SpawnRadius);
		FVector SpawnPos = Location + RandomOffset;

		FGPUSpawnRequest Request;
		Request.Position = FVector3f(SpawnPos);
		Request.Velocity = FVector3f::ZeroVector;
		Request.Mass = Mass;
		Request.Radius = Radius;
		Request.SourceID = CachedSourceID;
		SpawnRequests.Add(Request);
	}

	GPUSim->AddSpawnRequests(SpawnRequests);
}

/**
 * @brief Spawns particles in a spherical region with a grid-based distribution.
 * @param Center Center of the sphere.
 * @param Radius Radius of the sphere.
 * @param Spacing Distance between grid points.
 * @param bJitter Whether to apply random offsets to grid points.
 * @param JitterAmount Magnitude of random variation (0 to 0.5).
 * @param Velocity Initial velocity applied to all spawned particles.
 * @param Rotation Orientation of the spawn volume (affects velocity).
 * @return Total number of spawned particles.
 */
int32 UKawaiiFluidSimulationModule::SpawnParticlesSphere(FVector Center, float Radius, float Spacing,
                                                         bool bJitter, float JitterAmount, FVector Velocity,
                                                         FRotator Rotation)
{
	if (Spacing <= 0.0f || Radius <= 0.0f)
	{
		return 0;
	}

	// Calculate bounding box range for sphere
	const int32 GridSize = FMath::CeilToInt(Radius / Spacing);
	const float RadiusSq = Radius * Radius;
	const float HalfSpacing = Spacing * 0.5f;
	const float JitterRange = Spacing * JitterAmount;

	// FRotator -> FQuat conversion
	const FQuat RotationQuat = Rotation.Quaternion();

	// Apply rotation to velocity (sphere doesn't need position rotation)
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;

	// Calculate estimated particle count (sphere volume / particle volume)
	const float EstimatedCount = (4.0f / 3.0f * PI * Radius * Radius * Radius) / (Spacing * Spacing * Spacing);

	// GPU mode: batch spawn requests for efficiency
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (bGPUSimulationActive && GPUSim)
	{
		const float Mass = Preset ? Preset->ParticleMass : 1.0f;
		const float ParticleRadius = Preset ? Preset->ParticleRadius : 5.0f;

		TArray<FGPUSpawnRequest> SpawnRequests;
		SpawnRequests.Reserve(FMath::CeilToInt(EstimatedCount));

		for (int32 x = -GridSize; x <= GridSize; ++x)
		{
			for (int32 y = -GridSize; y <= GridSize; ++y)
			{
				for (int32 z = -GridSize; z <= GridSize; ++z)
				{
					FVector LocalPos(x * Spacing, y * Spacing, z * Spacing);

					if (LocalPos.SizeSquared() <= RadiusSq)
					{
						FVector SpawnPos = Center + LocalPos;

						if (bJitter && JitterRange > 0.0f)
						{
							SpawnPos += FVector(
								FMath::FRandRange(-JitterRange, JitterRange),
								FMath::FRandRange(-JitterRange, JitterRange),
								FMath::FRandRange(-JitterRange, JitterRange)
							);
						}

						FGPUSpawnRequest Request;
						Request.Position = FVector3f(SpawnPos);
						Request.Velocity = FVector3f(WorldVelocity);
						Request.Mass = Mass;
						Request.Radius = ParticleRadius;
						Request.SourceID = CachedSourceID;  // Propagate source identification
						SpawnRequests.Add(Request);
						++SpawnedCount;
					}
				}
			}
		}

		if (SpawnRequests.Num() > 0)
		{
			GPUSim->AddSpawnRequests(SpawnRequests);
		}
		return SpawnedCount;
	}

	return 0;
}

int32 UKawaiiFluidSimulationModule::SpawnParticlesBox(FVector Center, FVector Extent, float Spacing,
                                                      bool bJitter, float JitterAmount, FVector Velocity,
                                                      FRotator Rotation)
{
	if (Spacing <= 0.0f)
	{
		return 0;
	}

	// Calculate grid count for each axis
	const int32 CountX = FMath::Max(1, FMath::CeilToInt(Extent.X * 2.0f / Spacing));
	const int32 CountY = FMath::Max(1, FMath::CeilToInt(Extent.Y * 2.0f / Spacing));
	const int32 CountZ = FMath::Max(1, FMath::CeilToInt(Extent.Z * 2.0f / Spacing));

	const float JitterRange = Spacing * JitterAmount;
	const FVector LocalStartOffset = -Extent + FVector(Spacing * 0.5f);

	// FRotator -> FQuat conversion
	const FQuat RotationQuat = Rotation.Quaternion();

	// Apply rotation to velocity
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;
	const int32 TotalCount = CountX * CountY * CountZ;
	Particles.Reserve(Particles.Num() + TotalCount);

	for (int32 x = 0; x < CountX; ++x)
	{
		for (int32 y = 0; y < CountY; ++y)
		{
			for (int32 z = 0; z < CountZ; ++z)
			{
				// Calculate local position
				FVector LocalPos = LocalStartOffset + FVector(x * Spacing, y * Spacing, z * Spacing);

				// Apply jitter (in local space)
				if (bJitter && JitterRange > 0.0f)
				{
					LocalPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				// Calculate world position after applying rotation
				const FVector WorldPos = Center + RotationQuat.RotateVector(LocalPos);

				SpawnParticle(WorldPos, WorldVelocity);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

int32 UKawaiiFluidSimulationModule::SpawnParticlesCylinder(FVector Center, float Radius, float HalfHeight, float Spacing,
                                                           bool bJitter, float JitterAmount, FVector Velocity,
                                                           FRotator Rotation)
{
	if (Spacing <= 0.0f || Radius <= 0.0f || HalfHeight <= 0.0f)
	{
		return 0;
	}

	// Calculate bounding box range for cylinder
	const int32 GridSizeXY = FMath::CeilToInt(Radius / Spacing);
	const int32 GridSizeZ = FMath::CeilToInt(HalfHeight / Spacing);
	const float RadiusSq = Radius * Radius;
	const float JitterRange = Spacing * JitterAmount;

	// FRotator -> FQuat conversion
	const FQuat RotationQuat = Rotation.Quaternion();

	// Apply rotation to velocity
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;

	// Calculate estimated particle count (cylinder volume / particle volume)
	const float EstimatedCount = (PI * Radius * Radius * HalfHeight * 2.0f) / (Spacing * Spacing * Spacing);
	Particles.Reserve(Particles.Num() + FMath::CeilToInt(EstimatedCount));

	// Iterate through grid
	for (int32 x = -GridSizeXY; x <= GridSizeXY; ++x)
	{
		for (int32 y = -GridSizeXY; y <= GridSizeXY; ++y)
		{
			// Check if inside circle in XY plane
			const float DistSqXY = x * x * Spacing * Spacing + y * y * Spacing * Spacing;
			if (DistSqXY > RadiusSq)
			{
				continue;
			}

			for (int32 z = -GridSizeZ; z <= GridSizeZ; ++z)
			{
				FVector LocalPos(x * Spacing, y * Spacing, z * Spacing);

				// Apply jitter (in local space)
				if (bJitter && JitterRange > 0.0f)
				{
					LocalPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				// Calculate world position after applying rotation
				const FVector WorldPos = Center + RotationQuat.RotateVector(LocalPos);

				SpawnParticle(WorldPos, WorldVelocity);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

//=============================================================================
// Hexagonal Close Packing Spawn Functions
//=============================================================================

int32 UKawaiiFluidSimulationModule::SpawnParticlesBoxHexagonal(FVector Center, FVector Extent, float Spacing,
                                                                bool bJitter, float JitterAmount, FVector Velocity,
                                                                FRotator Rotation)
{
	if (Spacing <= 0.0f)
	{
		return 0;
	}

	// HCP density compensation: HCP is ~1.42x denser than cubic for the same spacing
	// To achieve similar number density, multiply spacing by (1/0.707)^(1/3) ≈ 1.122
	const float HCPCompensation = 1.122f;
	const float AdjustedSpacing = Spacing * HCPCompensation;

	// Hexagonal Close Packing (HCP) constants
	// XY plane: hexagonal grid with row offset
	// Z layers: alternating offset for close packing
	const float RowSpacingY = AdjustedSpacing * 0.866025f;  // sqrt(3)/2
	const float LayerSpacingZ = AdjustedSpacing * 0.816497f;  // sqrt(2/3)
	const float JitterRange = AdjustedSpacing * JitterAmount;

	// Calculate grid counts
	const int32 CountX = FMath::Max(1, FMath::CeilToInt(Extent.X * 2.0f / AdjustedSpacing));
	const int32 CountY = FMath::Max(1, FMath::CeilToInt(Extent.Y * 2.0f / RowSpacingY));
	const int32 CountZ = FMath::Max(1, FMath::CeilToInt(Extent.Z * 2.0f / LayerSpacingZ));

	const FQuat RotationQuat = Rotation.Quaternion();
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;
	const int32 EstimatedTotal = CountX * CountY * CountZ;
	Particles.Reserve(Particles.Num() + EstimatedTotal);

	// Start position (bottom-left-back corner)
	const FVector LocalStart(-Extent.X + AdjustedSpacing * 0.5f, -Extent.Y + RowSpacingY * 0.5f, -Extent.Z + LayerSpacingZ * 0.5f);

	for (int32 z = 0; z < CountZ; ++z)
	{
		// Z layer offset for HCP (ABC stacking pattern)
		const float ZLayerOffsetX = (z % 3 == 1) ? AdjustedSpacing * 0.5f : ((z % 3 == 2) ? AdjustedSpacing * 0.25f : 0.0f);
		const float ZLayerOffsetY = (z % 3 == 1) ? RowSpacingY / 3.0f : ((z % 3 == 2) ? RowSpacingY * 2.0f / 3.0f : 0.0f);

		for (int32 y = 0; y < CountY; ++y)
		{
			// Row offset for hexagonal pattern in XY plane
			const float RowOffsetX = (y % 2 == 1) ? AdjustedSpacing * 0.5f : 0.0f;

			for (int32 x = 0; x < CountX; ++x)
			{
				FVector LocalPos(
					LocalStart.X + x * AdjustedSpacing + RowOffsetX + ZLayerOffsetX,
					LocalStart.Y + y * RowSpacingY + ZLayerOffsetY,
					LocalStart.Z + z * LayerSpacingZ
				);

				// Check bounds
				if (FMath::Abs(LocalPos.X) > Extent.X ||
				    FMath::Abs(LocalPos.Y) > Extent.Y ||
				    FMath::Abs(LocalPos.Z) > Extent.Z)
				{
					continue;
				}

				// Apply jitter
				if (bJitter && JitterRange > 0.0f)
				{
					LocalPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				const FVector WorldPos = Center + RotationQuat.RotateVector(LocalPos);
				SpawnParticle(WorldPos, WorldVelocity);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

int32 UKawaiiFluidSimulationModule::SpawnParticlesSphereHexagonal(FVector Center, float Radius, float Spacing,
                                                                   bool bJitter, float JitterAmount, FVector Velocity,
                                                                   FRotator Rotation)
{
	if (Spacing <= 0.0f || Radius <= 0.0f)
	{
		return 0;
	}

	// HCP density compensation: HCP is ~1.42x denser than cubic for the same spacing
	const float HCPCompensation = 1.122f;
	const float AdjustedSpacing = Spacing * HCPCompensation;

	// Use Box Hexagonal spawn and filter by sphere radius
	const float RowSpacingY = AdjustedSpacing * 0.866025f;
	const float LayerSpacingZ = AdjustedSpacing * 0.816497f;
	const float JitterRange = AdjustedSpacing * JitterAmount;
	const float RadiusSq = Radius * Radius;

	const int32 GridSize = FMath::CeilToInt(Radius / AdjustedSpacing) + 1;
	const int32 GridSizeY = FMath::CeilToInt(Radius / RowSpacingY) + 1;
	const int32 GridSizeZ = FMath::CeilToInt(Radius / LayerSpacingZ) + 1;

	const FQuat RotationQuat = Rotation.Quaternion();
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;
	const float EstimatedCount = (4.0f / 3.0f) * PI * Radius * Radius * Radius / (AdjustedSpacing * AdjustedSpacing * AdjustedSpacing);
	Particles.Reserve(Particles.Num() + FMath::CeilToInt(EstimatedCount));

	for (int32 z = -GridSizeZ; z <= GridSizeZ; ++z)
	{
		const float ZLayerOffsetX = (((z + GridSizeZ) % 3) == 1) ? AdjustedSpacing * 0.5f : ((((z + GridSizeZ) % 3) == 2) ? AdjustedSpacing * 0.25f : 0.0f);
		const float ZLayerOffsetY = (((z + GridSizeZ) % 3) == 1) ? RowSpacingY / 3.0f : ((((z + GridSizeZ) % 3) == 2) ? RowSpacingY * 2.0f / 3.0f : 0.0f);

		for (int32 y = -GridSizeY; y <= GridSizeY; ++y)
		{
			const float RowOffsetX = (((y + GridSizeY) % 2) == 1) ? AdjustedSpacing * 0.5f : 0.0f;

			for (int32 x = -GridSize; x <= GridSize; ++x)
			{
				FVector LocalPos(
					x * AdjustedSpacing + RowOffsetX + ZLayerOffsetX,
					y * RowSpacingY + ZLayerOffsetY,
					z * LayerSpacingZ
				);

				// Check sphere bounds
				if (LocalPos.SizeSquared() > RadiusSq)
				{
					continue;
				}

				// Apply jitter
				if (bJitter && JitterRange > 0.0f)
				{
					LocalPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				const FVector WorldPos = Center + RotationQuat.RotateVector(LocalPos);
				SpawnParticle(WorldPos, WorldVelocity);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

int32 UKawaiiFluidSimulationModule::SpawnParticlesCylinderHexagonal(FVector Center, float Radius, float HalfHeight, float Spacing,
                                                                     bool bJitter, float JitterAmount, FVector Velocity,
                                                                     FRotator Rotation)
{
	if (Spacing <= 0.0f || Radius <= 0.0f || HalfHeight <= 0.0f)
	{
		return 0;
	}

	// HCP density compensation: HCP is ~1.42x denser than cubic for the same spacing
	const float HCPCompensation = 1.122f;
	const float AdjustedSpacing = Spacing * HCPCompensation;

	const float RowSpacingY = AdjustedSpacing * 0.866025f;
	const float LayerSpacingZ = AdjustedSpacing * 0.816497f;
	const float JitterRange = AdjustedSpacing * JitterAmount;
	const float RadiusSq = Radius * Radius;

	const int32 GridSizeXY = FMath::CeilToInt(Radius / AdjustedSpacing) + 1;
	const int32 GridSizeY = FMath::CeilToInt(Radius / RowSpacingY) + 1;
	const int32 GridSizeZ = FMath::CeilToInt(HalfHeight / LayerSpacingZ);

	const FQuat RotationQuat = Rotation.Quaternion();
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;
	const float EstimatedCount = PI * Radius * Radius * HalfHeight * 2.0f / (AdjustedSpacing * AdjustedSpacing * AdjustedSpacing);
	Particles.Reserve(Particles.Num() + FMath::CeilToInt(EstimatedCount));

	for (int32 z = -GridSizeZ; z <= GridSizeZ; ++z)
	{
		const float ZLayerOffsetX = (((z + GridSizeZ) % 3) == 1) ? AdjustedSpacing * 0.5f : ((((z + GridSizeZ) % 3) == 2) ? AdjustedSpacing * 0.25f : 0.0f);
		const float ZLayerOffsetY = (((z + GridSizeZ) % 3) == 1) ? RowSpacingY / 3.0f : ((((z + GridSizeZ) % 3) == 2) ? RowSpacingY * 2.0f / 3.0f : 0.0f);

		for (int32 y = -GridSizeY; y <= GridSizeY; ++y)
		{
			const float RowOffsetX = (((y + GridSizeY) % 2) == 1) ? AdjustedSpacing * 0.5f : 0.0f;

			for (int32 x = -GridSizeXY; x <= GridSizeXY; ++x)
			{
				FVector LocalPos(
					x * AdjustedSpacing + RowOffsetX + ZLayerOffsetX,
					y * RowSpacingY + ZLayerOffsetY,
					z * LayerSpacingZ
				);

				// Check cylinder XY bounds
				const float DistSqXY = LocalPos.X * LocalPos.X + LocalPos.Y * LocalPos.Y;
				if (DistSqXY > RadiusSq)
				{
					continue;
				}

				// Check Z bounds
				if (FMath::Abs(LocalPos.Z) > HalfHeight)
				{
					continue;
				}

				// Apply jitter
				if (bJitter && JitterRange > 0.0f)
				{
					LocalPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				const FVector WorldPos = Center + RotationQuat.RotateVector(LocalPos);
				SpawnParticle(WorldPos, WorldVelocity);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

int32 UKawaiiFluidSimulationModule::SpawnParticleDirectional(FVector Position, FVector Direction, float Speed,
                                                             float Radius, float ConeAngle)
{
	// Normalize direction
	FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector(0, 0, -1);  // Default: downward direction
	}

	FVector SpawnPos = Position;
	FVector SpawnVel = Dir * Speed;

	// Apply stream radius (position dispersion)
	if (Radius > 0.0f)
	{
		// Random offset in plane perpendicular to direction
		FVector Right, Up;
		Dir.FindBestAxisVectors(Right, Up);

		const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float RandomRadius = FMath::FRandRange(0.0f, Radius);

		SpawnPos += Right * FMath::Cos(RandomAngle) * RandomRadius;
		SpawnPos += Up * FMath::Sin(RandomAngle) * RandomRadius;
	}

	// Apply cone spray angle (velocity direction dispersion)
	if (ConeAngle > 0.0f)
	{
		// Random direction within spray angle range
		const float HalfAngleRad = FMath::DegreesToRadians(ConeAngle * 0.5f);
		const float RandomPhi = FMath::FRandRange(0.0f, 2.0f * PI);
		const float RandomTheta = FMath::FRandRange(0.0f, HalfAngleRad);

		// Generate random direction in local coordinate system
		FVector Right, Up;
		Dir.FindBestAxisVectors(Right, Up);

		const float SinTheta = FMath::Sin(RandomTheta);
		FVector RandomDir = Dir * FMath::Cos(RandomTheta)
		                  + Right * SinTheta * FMath::Cos(RandomPhi)
		                  + Up * SinTheta * FMath::Sin(RandomPhi);

		SpawnVel = RandomDir.GetSafeNormal() * Speed;
	}

	return SpawnParticle(SpawnPos, SpawnVel);
}

int32 UKawaiiFluidSimulationModule::SpawnParticleDirectionalHexLayer(FVector Position, FVector Direction, float Speed,
                                                                      float Radius, float Spacing, float Jitter)
{
	TArray<FGPUSpawnRequest> BatchRequests;
	int32 SpawnedCount = SpawnParticleDirectionalHexLayerBatch(Position, Direction, Speed, Radius, Spacing, Jitter, BatchRequests);

	// Send batch requests
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (BatchRequests.Num() > 0 && GPUSim)
	{
		GPUSim->AddSpawnRequests(BatchRequests);
	}

	return SpawnedCount;
}

int32 UKawaiiFluidSimulationModule::SpawnParticleDirectionalHexLayerBatch(FVector Position, FVector Direction, float Speed,
                                                                          float Radius, float Spacing, float Jitter,
                                                                          TArray<FGPUSpawnRequest>& OutBatch)
{
	// Normalize direction
	FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector(0, 0, -1);  // Default: downward direction
	}

	// Auto-calculate spacing from ParticleSpacing (matches Mass calculation)
	if (Spacing <= 0.0f)
	{
		Spacing = Preset ? Preset->ParticleSpacing : 10.0f;
	}

	// Limit jitter range (0 ~ 0.5)
	Jitter = FMath::Clamp(Jitter, 0.0f, 0.5f);
	const float MaxJitterOffset = Spacing * Jitter;
	const bool bApplyJitter = Jitter > KINDA_SMALL_NUMBER;

	// Create local coordinate system perpendicular to direction
	FVector Right, Up;
	Dir.FindBestAxisVectors(Right, Up);

	// Hexagonal packing constants
	const float RowSpacing = Spacing * FMath::Sqrt(3.0f) * 0.5f;  // ~0.866 * Spacing
	const float RadiusSq = Radius * Radius;

	// Calculate number of rows
	const int32 NumRows = FMath::CeilToInt(Radius / RowSpacing) * 2 + 1;
	const int32 HalfRows = NumRows / 2;

	int32 SpawnedCount = 0;
	const FVector SpawnVel = Dir * Speed;

	const float ParticleMass = Preset ? Preset->ParticleMass : 1.0f;
	const float ParticleRadius = Preset ? Preset->ParticleRadius : 5.0f;

	// Iterate through hexagonal grid
	for (int32 RowIdx = -HalfRows; RowIdx <= HalfRows; ++RowIdx)
	{
		const float LocalY = RowIdx * RowSpacing;
		const float LocalYSq = LocalY * LocalY;

		// Calculate maximum X range inside circle for this row
		if (LocalYSq > RadiusSq)
		{
			continue;  // Row outside circle
		}

		const float MaxX = FMath::Sqrt(RadiusSq - LocalYSq);

		// Apply X offset for odd rows (Hexagonal Packing)
		const float XOffset = (FMath::Abs(RowIdx) % 2 != 0) ? Spacing * 0.5f : 0.0f;

		// Calculate X start point (symmetric around center)
		const int32 NumCols = FMath::FloorToInt(MaxX / Spacing);

		for (int32 ColIdx = -NumCols; ColIdx <= NumCols; ++ColIdx)
		{
			float LocalX = ColIdx * Spacing + XOffset;
			float LocalYFinal = LocalY;

			// Apply jitter: add random offset
			if (bApplyJitter)
			{
				LocalX += FMath::FRandRange(-MaxJitterOffset, MaxJitterOffset);
				LocalYFinal += FMath::FRandRange(-MaxJitterOffset, MaxJitterOffset);
			}

			// Check if inside circle (verify still inside after jitter)
			if (LocalX * LocalX + LocalYFinal * LocalYFinal <= RadiusSq)
			{
				FVector SpawnPos = Position + Right * LocalX + Up * LocalYFinal;
				
				// Create request and add to batch
				FGPUSpawnRequest Request;
				Request.Position = FVector3f(SpawnPos);
				Request.Velocity = FVector3f(SpawnVel);
				Request.Mass = ParticleMass;
				Request.Radius = ParticleRadius;
				Request.SourceID = CachedSourceID;

				OutBatch.Add(Request);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

void UKawaiiFluidSimulationModule::ClearAllParticles()
{
	Particles.Empty();

	// GPU-driven despawn: remove all particles with this Module's SourceID
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (bGPUSimulationActive && GPUSim)
	{
		GPUSim->AddGPUDespawnSourceRequest(CachedSourceID);

		// Cancel pending spawns to prevent ghost particles after despawn
		if (FGPUSpawnManager* SpawnMgr = GPUSim->GetSpawnManager())
		{
			SpawnMgr->CancelPendingSpawnsForSource(CachedSourceID);
		}

		UE_LOG(LogTemp, Log, TEXT("ClearAllParticles: SourceID=%d (GPU despawn by source)"), CachedSourceID);
	}
}

void UKawaiiFluidSimulationModule::DespawnByBrushGPU(FVector Center, float Radius)
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (bGPUSimulationActive && GPUSim)
	{
		GPUSim->AddGPUDespawnBrushRequest(FVector3f(Center), Radius);
	}
}

void UKawaiiFluidSimulationModule::DespawnBySourceGPU(int32 SourceID)
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (bGPUSimulationActive && GPUSim)
	{
		GPUSim->AddGPUDespawnSourceRequest(SourceID);
	}
}


TArray<FVector> UKawaiiFluidSimulationModule::GetParticlePositions() const
{
	TArray<FVector> Positions;
	Positions.Reserve(Particles.Num());

	for (const FKawaiiFluidParticle& Particle : Particles)
	{
		Positions.Add(Particle.Position);
	}

	return Positions;
}

TArray<FVector> UKawaiiFluidSimulationModule::GetParticleVelocities() const
{
	TArray<FVector> Velocities;
	Velocities.Reserve(Particles.Num());

	for (const FKawaiiFluidParticle& Particle : Particles)
	{
		Velocities.Add(Particle.Velocity);
	}

	return Velocities;
}

void UKawaiiFluidSimulationModule::ApplyExternalForce(FVector Force)
{
	AccumulatedExternalForce += Force;
}

void UKawaiiFluidSimulationModule::ApplyForceToParticle(int32 ParticleIndex, FVector Force)
{
	if (Particles.IsValidIndex(ParticleIndex))
	{
		Particles[ParticleIndex].Velocity += Force;
	}
}

void UKawaiiFluidSimulationModule::RegisterCollider(UKawaiiFluidCollider* Collider)
{
	if (Collider && !Colliders.Contains(Collider))
	{
		Colliders.Add(Collider);
	}
}

void UKawaiiFluidSimulationModule::UnregisterCollider(UKawaiiFluidCollider* Collider)
{
	Colliders.Remove(Collider);
}

TArray<int32> UKawaiiFluidSimulationModule::GetParticlesInRadius(FVector Location, float Radius) const
{
	TArray<int32> Result;
	const float RadiusSq = Radius * Radius;

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		const float DistSq = FVector::DistSquared(Particles[i].Position, Location);
		if (DistSq <= RadiusSq)
		{
			Result.Add(i);
		}
	}

	return Result;
}

TArray<int32> UKawaiiFluidSimulationModule::GetParticlesInBox(FVector Center, FVector Extent) const
{
	TArray<int32> Result;
	const FBox Box(Center - Extent, Center + Extent);

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		if (Box.IsInside(Particles[i].Position))
		{
			Result.Add(i);
		}
	}

	return Result;
}

bool UKawaiiFluidSimulationModule::GetParticleInfo(int32 ParticleIndex, FVector& OutPosition, FVector& OutVelocity, float& OutDensity) const
{
	if (!Particles.IsValidIndex(ParticleIndex))
	{
		return false;
	}

	const FKawaiiFluidParticle& Particle = Particles[ParticleIndex];
	OutPosition = Particle.Position;
	OutVelocity = Particle.Velocity;
	OutDensity = Particle.Density;

	return true;
}

//========================================
// IKawaiiFluidDataProvider Interface
//========================================

float UKawaiiFluidSimulationModule::GetParticleRadius() const
{
	if (Preset)
	{
		return Preset->ParticleRadius;
	}

	return 10.0f; // Default value
}

FString UKawaiiFluidSimulationModule::GetDebugName() const
{
	AActor* Owner = GetOwnerActor();
	return FString::Printf(TEXT("SimulationModule_%s"),
		Owner ? *Owner->GetName() : TEXT("NoOwner"));
}

//========================================
// GPU Buffer Access (Phase 2)
//========================================

bool UKawaiiFluidSimulationModule::IsGPUSimulationActive() const
{
	return bGPUSimulationActive && WeakGPUSimulator.IsValid();
}

int32 UKawaiiFluidSimulationModule::GetGPUParticleCount() const
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (GPUSim && GPUSim->IsReady())
	{
		return GPUSim->GetParticleCount();
	}
	return 0;
}

int32 UKawaiiFluidSimulationModule::GetParticleCountForSource(int32 SourceID) const
{
	TSharedPtr<FGPUFluidSimulator> GPUSim = WeakGPUSimulator.Pin();
	if (GPUSim && GPUSim->IsReady())
	{
		FGPUSpawnManager* SpawnManager = GPUSim->GetSpawnManager();
		if (SpawnManager)
		{
			return SpawnManager->GetParticleCountForSource(SourceID);
		}
	}
	return -1;
}

//========================================
// Spawn functions with explicit count
//========================================

int32 UKawaiiFluidSimulationModule::SpawnParticlesSphereByCount(FVector Center, float Radius, int32 Count,
                                                                bool bJitter, float JitterAmount, FVector Velocity,
                                                                FRotator Rotation)
{
	if (Count <= 0 || Radius <= 0.0f)
	{
		return 0;
	}

	// Calculate spacing from count: sphere volume = (4/3)πr³, volume per particle = spacing³
	// Count = Volume / spacing³ → spacing = (Volume / Count)^(1/3)
	const float Volume = (4.0f / 3.0f) * PI * Radius * Radius * Radius;
	const float Spacing = FMath::Pow(Volume / Count, 1.0f / 3.0f);

	// Spawn based on actual grid (to match results of spacing-based function)
	const int32 GridSize = FMath::CeilToInt(Radius / Spacing);
	const float RadiusSq = Radius * Radius;
	const float JitterRange = Spacing * JitterAmount;

	// FRotator -> FQuat conversion
	const FQuat RotationQuat = Rotation.Quaternion();

	// Apply rotation to velocity (sphere doesn't need position rotation)
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;
	Particles.Reserve(Particles.Num() + Count);

	// Iterate through grid
	for (int32 x = -GridSize; x <= GridSize && SpawnedCount < Count; ++x)
	{
		for (int32 y = -GridSize; y <= GridSize && SpawnedCount < Count; ++y)
		{
			for (int32 z = -GridSize; z <= GridSize && SpawnedCount < Count; ++z)
			{
				FVector LocalPos(x * Spacing, y * Spacing, z * Spacing);

				// Check if inside sphere
				if (LocalPos.SizeSquared() <= RadiusSq)
				{
					FVector SpawnPos = Center + LocalPos;

					// Apply jitter
					if (bJitter && JitterRange > 0.0f)
					{
						SpawnPos += FVector(
							FMath::FRandRange(-JitterRange, JitterRange),
							FMath::FRandRange(-JitterRange, JitterRange),
							FMath::FRandRange(-JitterRange, JitterRange)
						);
					}

					SpawnParticle(SpawnPos, WorldVelocity);
					++SpawnedCount;
				}
			}
		}
	}

	return SpawnedCount;
}

int32 UKawaiiFluidSimulationModule::SpawnParticlesBoxByCount(FVector Center, FVector Extent, int32 Count,
                                                             bool bJitter, float JitterAmount, FVector Velocity,
                                                             FRotator Rotation)
{
	if (Count <= 0)
	{
		return 0;
	}

	// Calculate spacing from count: box volume = 8 * Extent.X * Extent.Y * Extent.Z
	const float Volume = 8.0f * Extent.X * Extent.Y * Extent.Z;

	if (Volume <= 0.0f)
	{
		return 0;
	}

	// Maintain ratio of each axis for uniform distribution
	// n = nx * ny * nz, maintaining ratio gives nx:ny:nz = Ex:Ey:Ez
	// spacing = (Volume / Count)^(1/3)
	const float Spacing = FMath::Pow(Volume / Count, 1.0f / 3.0f);

	const int32 CountX = FMath::Max(1, FMath::RoundToInt(Extent.X * 2.0f / Spacing));
	const int32 CountY = FMath::Max(1, FMath::RoundToInt(Extent.Y * 2.0f / Spacing));
	const int32 CountZ = FMath::Max(1, FMath::RoundToInt(Extent.Z * 2.0f / Spacing));

	const float JitterRange = Spacing * JitterAmount;
	const FVector LocalStartOffset = -Extent + FVector(Spacing * 0.5f);

	// FRotator -> FQuat conversion
	const FQuat RotationQuat = Rotation.Quaternion();

	// Apply rotation to velocity
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;
	Particles.Reserve(Particles.Num() + Count);

	for (int32 x = 0; x < CountX && SpawnedCount < Count; ++x)
	{
		for (int32 y = 0; y < CountY && SpawnedCount < Count; ++y)
		{
			for (int32 z = 0; z < CountZ && SpawnedCount < Count; ++z)
			{
				// Calculate local position
				FVector LocalPos = LocalStartOffset + FVector(x * Spacing, y * Spacing, z * Spacing);

				// Apply jitter (in local space)
				if (bJitter && JitterRange > 0.0f)
				{
					LocalPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				// Calculate world position after applying rotation
				const FVector WorldPos = Center + RotationQuat.RotateVector(LocalPos);

				SpawnParticle(WorldPos, WorldVelocity);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

int32 UKawaiiFluidSimulationModule::SpawnParticlesCylinderByCount(FVector Center, float Radius, float HalfHeight, int32 Count,
                                                                   bool bJitter, float JitterAmount, FVector Velocity,
                                                                   FRotator Rotation)
{
	if (Count <= 0 || Radius <= 0.0f || HalfHeight <= 0.0f)
	{
		return 0;
	}

	// Calculate spacing from count: cylinder volume = π * r² * 2h
	const float Volume = PI * Radius * Radius * HalfHeight * 2.0f;
	const float Spacing = FMath::Pow(Volume / Count, 1.0f / 3.0f);

	const int32 GridSizeXY = FMath::CeilToInt(Radius / Spacing);
	const int32 GridSizeZ = FMath::CeilToInt(HalfHeight / Spacing);
	const float RadiusSq = Radius * Radius;
	const float JitterRange = Spacing * JitterAmount;

	// FRotator -> FQuat conversion
	const FQuat RotationQuat = Rotation.Quaternion();

	// Apply rotation to velocity
	const FVector WorldVelocity = RotationQuat.RotateVector(Velocity);

	int32 SpawnedCount = 0;
	Particles.Reserve(Particles.Num() + Count);

	// Iterate through grid
	for (int32 x = -GridSizeXY; x <= GridSizeXY && SpawnedCount < Count; ++x)
	{
		for (int32 y = -GridSizeXY; y <= GridSizeXY && SpawnedCount < Count; ++y)
		{
			// Check if inside circle in XY plane
			const float DistSqXY = x * x * Spacing * Spacing + y * y * Spacing * Spacing;
			if (DistSqXY > RadiusSq)
			{
				continue;
			}

			for (int32 z = -GridSizeZ; z <= GridSizeZ && SpawnedCount < Count; ++z)
			{
				FVector LocalPos(x * Spacing, y * Spacing, z * Spacing);

				// Apply jitter (in local space)
				if (bJitter && JitterRange > 0.0f)
				{
					LocalPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				// Calculate world position after applying rotation
				const FVector WorldPos = Center + RotationQuat.RotateVector(LocalPos);

				SpawnParticle(WorldPos, WorldVelocity);
				++SpawnedCount;
			}
		}
	}

	return SpawnedCount;
}

//========================================
// Simulation Bounds API
//========================================

void UKawaiiFluidSimulationModule::SetSimulationVolume(const FVector& Size, const FRotator& Rotation, float Bounce, float Friction)
{
	// Size is full size, convert to half-extent for internal storage
	const FVector HalfExtent = Size * 0.5f;

	// Clamp half-extent to maximum supported by Large preset
	const float EffectiveCellSize = FMath::Max(CellSize, 1.0f);
	const FVector ClampedHalfExtent = GridResolutionPresetHelper::ClampExtentToMaxSupported(HalfExtent, EffectiveCellSize);
	const FVector ClampedFullSize = ClampedHalfExtent * 2.0f;

	// Update the appropriate size property based on current bUniformSize setting
	// Note: bUniformSize is user-controlled from editor, don't modify it here
	if (bUniformSize)
	{
		UniformVolumeSize = FMath::Max3(ClampedFullSize.X, ClampedFullSize.Y, ClampedFullSize.Z);
	}
	else
	{
		VolumeSize = ClampedFullSize;
	}

	VolumeRotation = Rotation;
	// Note: Bounce/Friction parameters are ignored. Use Preset's Restitution/Friction instead.

	// Recalculate bounds (auto-selects Z-Order preset)
	RecalculateVolumeBounds();
}

/**
 * @brief Manually resolves particle collisions with the volume boundaries on the CPU.
 * 
 * Particles are clamped to the box volume and their velocity is reflected or dampened 
 * based on wall bounce and friction settings.
 */
void UKawaiiFluidSimulationModule::ResolveVolumeBoundaryCollisions()
{
	// Determine bounds parameters based on whether using external or internal volume
	FVector EffectiveCenter;
	FVector EffectiveHalfExtent;
	FQuat EffectiveRotation;
	float EffectiveBounce;
	float EffectiveFriction;

	// Get bounce/friction from Preset
	const float PresetBounce = Preset ? Preset->Bounciness : 0.0f;
	const float PresetFriction = Preset ? Preset->Friction : 0.5f;

	if (UKawaiiFluidVolumeComponent* ExternalVolume = GetTargetVolumeComponent())
	{
		if (IsUsingExternalVolume())
		{
			// Use external volume's parameters
			EffectiveCenter = ExternalVolume->GetComponentLocation();
			EffectiveHalfExtent = ExternalVolume->GetVolumeHalfExtent();
			EffectiveRotation = FQuat::Identity;  // External volumes are axis-aligned
			EffectiveBounce = ExternalVolume->GetWallBounce();
			EffectiveFriction = ExternalVolume->GetWallFriction();
		}
		else
		{
			// Use internal volume parameters
			EffectiveCenter = VolumeCenter;
			EffectiveHalfExtent = GridResolutionPresetHelper::ClampExtentToMaxSupported(GetVolumeHalfExtent(), CellSize);
			EffectiveRotation = VolumeRotationQuat;
			EffectiveBounce = PresetBounce;
			EffectiveFriction = PresetFriction;
		}
	}
	else
	{
		// Fallback to internal parameters
		EffectiveCenter = VolumeCenter;
		EffectiveHalfExtent = GridResolutionPresetHelper::ClampExtentToMaxSupported(GetVolumeHalfExtent(), CellSize);
		EffectiveRotation = VolumeRotationQuat;
		EffectiveBounce = PresetBounce;
		EffectiveFriction = PresetFriction;
	}

	// OBB (Oriented Bounding Box) collision handling
	// Transform particles to local space, check AABB collision, then transform back to world
	const FQuat InverseRotation = EffectiveRotation.Inverse();
	const FVector LocalBoxMin = -EffectiveHalfExtent;
	const FVector LocalBoxMax = EffectiveHalfExtent;

	for (FKawaiiFluidParticle& P : Particles)
	{
		// World -> Local transformation
		FVector LocalPos = InverseRotation.RotateVector(P.PredictedPosition - EffectiveCenter);
		FVector LocalVel = InverseRotation.RotateVector(P.Velocity);
		bool bCollided = false;

		// Check AABB collision in local space
		// Check X axis
		if (LocalPos.X < LocalBoxMin.X)
		{
			LocalPos.X = LocalBoxMin.X;
			if (LocalVel.X < 0.0f)
			{
				LocalVel.X = -LocalVel.X * EffectiveBounce;
			}
			LocalVel.Y *= (1.0f - EffectiveFriction);
			LocalVel.Z *= (1.0f - EffectiveFriction);
			bCollided = true;
		}
		else if (LocalPos.X > LocalBoxMax.X)
		{
			LocalPos.X = LocalBoxMax.X;
			if (LocalVel.X > 0.0f)
			{
				LocalVel.X = -LocalVel.X * EffectiveBounce;
			}
			LocalVel.Y *= (1.0f - EffectiveFriction);
			LocalVel.Z *= (1.0f - EffectiveFriction);
			bCollided = true;
		}

		// Check Y axis
		if (LocalPos.Y < LocalBoxMin.Y)
		{
			LocalPos.Y = LocalBoxMin.Y;
			if (LocalVel.Y < 0.0f)
			{
				LocalVel.Y = -LocalVel.Y * EffectiveBounce;
			}
			LocalVel.X *= (1.0f - EffectiveFriction);
			LocalVel.Z *= (1.0f - EffectiveFriction);
			bCollided = true;
		}
		else if (LocalPos.Y > LocalBoxMax.Y)
		{
			LocalPos.Y = LocalBoxMax.Y;
			if (LocalVel.Y > 0.0f)
			{
				LocalVel.Y = -LocalVel.Y * EffectiveBounce;
			}
			LocalVel.X *= (1.0f - EffectiveFriction);
			LocalVel.Z *= (1.0f - EffectiveFriction);
			bCollided = true;
		}

		// Check Z axis
		if (LocalPos.Z < LocalBoxMin.Z)
		{
			LocalPos.Z = LocalBoxMin.Z;
			if (LocalVel.Z < 0.0f)
			{
				LocalVel.Z = -LocalVel.Z * EffectiveBounce;
			}
			LocalVel.X *= (1.0f - EffectiveFriction);
			LocalVel.Y *= (1.0f - EffectiveFriction);
			bCollided = true;
		}
		else if (LocalPos.Z > LocalBoxMax.Z)
		{
			LocalPos.Z = LocalBoxMax.Z;
			if (LocalVel.Z > 0.0f)
			{
				LocalVel.Z = -LocalVel.Z * EffectiveBounce;
			}
			LocalVel.X *= (1.0f - EffectiveFriction);
			LocalVel.Y *= (1.0f - EffectiveFriction);
			bCollided = true;
		}

		// Update Position/Velocity by transforming Local -> World
		if (bCollided)
		{
			P.PredictedPosition = EffectiveCenter + EffectiveRotation.RotateVector(LocalPos);
			P.Position = P.PredictedPosition;
			P.Velocity = EffectiveRotation.RotateVector(LocalVel);
		}
	}
}

// Legacy - redirects to new function
void UKawaiiFluidSimulationModule::ResolveContainmentCollisions()
{
	ResolveVolumeBoundaryCollisions();
}

//========================================
// Source Identification
//========================================

void UKawaiiFluidSimulationModule::SetSourceID(int32 InSourceID)
{
	CachedSourceID = InSourceID;
	UE_LOG(LogTemp, Log, TEXT("SimulationModule::SetSourceID = %d"), CachedSourceID);
}

//========================================
// Simulation Volume
//========================================

UKawaiiFluidVolumeComponent* UKawaiiFluidSimulationModule::GetTargetVolumeComponent() const
{
	// If external volume actor is set, use its component
	if (TargetSimulationVolume)
	{
		return TargetSimulationVolume->GetVolumeComponent();
	}

	// Otherwise return internal owned volume component
	return OwnedVolumeComponent;
}

void UKawaiiFluidSimulationModule::SetTargetSimulationVolume(AKawaiiFluidVolume* NewSimulationVolume)
{
	if (TargetSimulationVolume == NewSimulationVolume)
	{
		return;
	}

	// Unbind from old volume's OnDestroyed event
	UnbindFromVolumeDestroyedEvent();

	// Unregister from old volume
	if (TargetSimulationVolume)
	{
		if (UKawaiiFluidVolumeComponent* OldVolume = TargetSimulationVolume->GetVolumeComponent())
		{
			OldVolume->UnregisterModule(this);
		}
	}

	TargetSimulationVolume = NewSimulationVolume;

	// Register to new volume and update tracking
	if (TargetSimulationVolume)
	{
		if (UKawaiiFluidVolumeComponent* NewVolume = TargetSimulationVolume->GetVolumeComponent())
		{
			NewVolume->RegisterModule(this);
			PreviousRegisteredVolume = NewVolume;
		}
		// Bind to new volume's OnDestroyed event
		BindToVolumeDestroyedEvent();
	}
	else
	{
		PreviousRegisteredVolume = nullptr;
	}

	// Update volume info display (handles CellSize derivation)
	UpdateVolumeInfoDisplay();
}

void UKawaiiFluidSimulationModule::RecalculateVolumeBounds()
{
	// Ensure valid CellSize
	CellSize = FMath::Max(CellSize, 1.0f);

	// Get the maximum half-extent supported by Large preset
	const float LargeMaxHalfExtent = GridResolutionPresetHelper::GetMaxExtentForPreset(EGridResolutionPreset::Large, CellSize);

	// Get current half-extent from VolumeSize (full size / 2)
	const FVector OriginalHalfExtent = GetVolumeHalfExtent();

	// First pass: Clamp half-extent to Large max (without rotation)
	FVector WorkingHalfExtent = GridResolutionPresetHelper::ClampExtentToMaxSupported(OriginalHalfExtent, CellSize);

	// Compute rotation quaternion
	VolumeRotationQuat = VolumeRotation.Quaternion();

	// Calculate the AABB extent for the rotated OBB
	FVector EffectiveHalfExtent = WorkingHalfExtent;

	// Helper lambda to compute AABB half-extent from OBB half-extent and rotation
	auto ComputeRotatedAABBHalfExtent = [this](const FVector& OBBHalfExtent) -> FVector
	{
		if (VolumeRotationQuat.Equals(FQuat::Identity))
		{
			return OBBHalfExtent;
		}

		FVector RotatedCorners[8];
		for (int32 i = 0; i < 8; ++i)
		{
			FVector Corner(
				(i & 1) ? OBBHalfExtent.X : -OBBHalfExtent.X,
				(i & 2) ? OBBHalfExtent.Y : -OBBHalfExtent.Y,
				(i & 4) ? OBBHalfExtent.Z : -OBBHalfExtent.Z
			);
			RotatedCorners[i] = VolumeRotationQuat.RotateVector(Corner);
		}

		FVector AABBMin = RotatedCorners[0];
		FVector AABBMax = RotatedCorners[0];
		for (int32 i = 1; i < 8; ++i)
		{
			AABBMin = AABBMin.ComponentMin(RotatedCorners[i]);
			AABBMax = AABBMax.ComponentMax(RotatedCorners[i]);
		}

		return FVector(
			FMath::Max(FMath::Abs(AABBMin.X), FMath::Abs(AABBMax.X)),
			FMath::Max(FMath::Abs(AABBMin.Y), FMath::Abs(AABBMax.Y)),
			FMath::Max(FMath::Abs(AABBMin.Z), FMath::Abs(AABBMax.Z))
		);
	};

	if (!VolumeRotationQuat.Equals(FQuat::Identity))
	{
		// Compute rotated AABB
		EffectiveHalfExtent = ComputeRotatedAABBHalfExtent(WorkingHalfExtent);

		// Check if rotated AABB exceeds Large preset limits
		const float MaxAABBHalfExtent = FMath::Max3(EffectiveHalfExtent.X, EffectiveHalfExtent.Y, EffectiveHalfExtent.Z);
		if (MaxAABBHalfExtent > LargeMaxHalfExtent)
		{
			// Scale down the original extent proportionally so rotated AABB fits within Large
			const float ScaleFactor = LargeMaxHalfExtent / MaxAABBHalfExtent;
			WorkingHalfExtent = WorkingHalfExtent * ScaleFactor;

			// Recompute rotated AABB with scaled extent
			EffectiveHalfExtent = ComputeRotatedAABBHalfExtent(WorkingHalfExtent);
		}
	}

	// Apply final extent if different from original (update VolumeSize/UniformVolumeSize)
	if (!WorkingHalfExtent.Equals(OriginalHalfExtent, 0.01f))
	{
		const bool bWasRotated = !VolumeRotationQuat.Equals(FQuat::Identity);
		const FVector OriginalSize = OriginalHalfExtent * 2.0f;
		const FVector NewSize = WorkingHalfExtent * 2.0f;

		if (bWasRotated)
		{
			const float RotatedAABB = FMath::Max3(ComputeRotatedAABBHalfExtent(OriginalHalfExtent).X,
			                                       ComputeRotatedAABBHalfExtent(OriginalHalfExtent).Y,
			                                       ComputeRotatedAABBHalfExtent(OriginalHalfExtent).Z);
			UE_LOG(LogTemp, Warning, TEXT("VolumeSize adjusted: Rotated AABB (%.1f cm) exceeds limit (%.1f cm). Size scaled from (%s) to (%s)"),
				RotatedAABB * 2.0f, LargeMaxHalfExtent * 2.0f, *OriginalSize.ToString(), *NewSize.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("VolumeSize exceeds limit (%.1f cm per axis). Clamped from (%s) to (%s)"),
				LargeMaxHalfExtent * 2.0f, *OriginalSize.ToString(), *NewSize.ToString());
		}

		// Update the stored size values
		VolumeSize = NewSize;
		if (bUniformSize)
		{
			UniformVolumeSize = FMath::Max3(NewSize.X, NewSize.Y, NewSize.Z);
		}
	}

	// Auto-select the smallest Z-Order preset that can contain the volume
	GridResolutionPreset = GridResolutionPresetHelper::SelectPresetForExtent(EffectiveHalfExtent, CellSize);

	// Update grid parameters from auto-selected preset
	GridAxisBits = GridResolutionPresetHelper::GetAxisBits(GridResolutionPreset);
	GridResolution = GridResolutionPresetHelper::GetGridResolution(GridResolutionPreset);
	MaxCells = GridResolutionPresetHelper::GetMaxCells(GridResolutionPreset);

	// Calculate Z-Order bounds extent from grid resolution and cell size
	BoundsExtent = static_cast<float>(GridResolution) * CellSize;

	// Get owner location for bounds center
	FVector OwnerLocation = FVector::ZeroVector;
	if (AActor* Owner = GetOwnerActor())
	{
		OwnerLocation = Owner->GetActorLocation();
	}

	// Update volume center
	VolumeCenter = OwnerLocation;

	// Calculate world bounds (centered on owner) - this is the Z-Order space bounds
	const float HalfExtent = BoundsExtent * 0.5f;
	WorldBoundsMin = OwnerLocation - FVector(HalfExtent, HalfExtent, HalfExtent);
	WorldBoundsMax = OwnerLocation + FVector(HalfExtent, HalfExtent, HalfExtent);

	// Update owned volume component if exists
	// Sync all bounds since OwnedVolumeComponent doesn't have a proper transform
	if (OwnedVolumeComponent)
	{
		OwnedVolumeComponent->GridResolutionPreset = GridResolutionPreset;
		OwnedVolumeComponent->GridAxisBits = GridAxisBits;
		OwnedVolumeComponent->GridResolution = GridResolution;
		OwnedVolumeComponent->MaxCells = MaxCells;
		OwnedVolumeComponent->CellSize = CellSize;
		OwnedVolumeComponent->BoundsExtent = BoundsExtent;
		OwnedVolumeComponent->WorldBoundsMin = WorldBoundsMin;
		OwnedVolumeComponent->WorldBoundsMax = WorldBoundsMax;
	}
}

/**
 * @brief Syncs internal volume properties with the target simulation volume or derives them from the current preset.
 */
void UKawaiiFluidSimulationModule::UpdateVolumeInfoDisplay()
{
	if (TargetSimulationVolume)
	{
		// Read info from external volume
		if (UKawaiiFluidVolumeComponent* ExternalVolume = TargetSimulationVolume->GetVolumeComponent())
		{
			// Copy all info from external volume (preset is controlled by external)
			GridResolutionPreset = ExternalVolume->GridResolutionPreset;
			GridAxisBits = ExternalVolume->GridAxisBits;
			GridResolution = ExternalVolume->GridResolution;
			MaxCells = ExternalVolume->MaxCells;
			BoundsExtent = ExternalVolume->BoundsExtent;
			WorldBoundsMin = ExternalVolume->GetWorldBoundsMin();
			WorldBoundsMax = ExternalVolume->GetWorldBoundsMax();
			CellSize = ExternalVolume->CellSize;

			// Note: Bounce/Friction are now obtained from Preset directly (no longer copied from volume)

			// Copy size parameters for consistency (read-only display when using external)
			bUniformSize = ExternalVolume->bUniformSize;
			UniformVolumeSize = ExternalVolume->UniformVolumeSize;
			VolumeSize = ExternalVolume->VolumeSize;
		}
	}
	else
	{
		// Derive CellSize from Preset's SmoothingRadius
		if (Preset && Preset->SmoothingRadius > 0.0f)
		{
			CellSize = Preset->SmoothingRadius;

			// Always recalculate volume size based on Medium preset
			// This ensures the default grid preset is Medium for the given CellSize
			const float MediumGridResolution = static_cast<float>(GridResolutionPresetHelper::GetGridResolution(EGridResolutionPreset::Medium));
			const float NewDefaultSize = MediumGridResolution * CellSize;
			UniformVolumeSize = NewDefaultSize;
			VolumeSize = FVector(NewDefaultSize);
		}
		// Calculate bounds from internal settings (uses GridResolutionPreset)
		RecalculateVolumeBounds();
	}
}

/**
 * @brief Logic executed when the target simulation volume actor is destroyed.
 * @param DestroyedActor The actor being destroyed.
 */
void UKawaiiFluidSimulationModule::OnTargetVolumeDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == TargetSimulationVolume)
	{
		if (UKawaiiFluidVolumeComponent* VolumeComp = PreviousRegisteredVolume.Get())
		{
			VolumeComp->UnregisterModule(this);
		}

		TargetSimulationVolume = nullptr;
		PreviousRegisteredVolume = nullptr;
		bBoundToVolumeDestroyed = false;

		UpdateVolumeInfoDisplay();
	}
}

/**
 * @brief Binds to the target volume's destruction event for automatic cleanup.
 */
void UKawaiiFluidSimulationModule::BindToVolumeDestroyedEvent()
{
	UnbindFromVolumeDestroyedEvent();

	if (TargetSimulationVolume && IsValid(TargetSimulationVolume))
	{
		TargetSimulationVolume->OnDestroyed.AddDynamic(this, &UKawaiiFluidSimulationModule::OnTargetVolumeDestroyed);
		bBoundToVolumeDestroyed = true;
	}
}

/**
 * @brief Safely unbinds from the target volume's destruction event.
 */
void UKawaiiFluidSimulationModule::UnbindFromVolumeDestroyedEvent()
{
	if (bBoundToVolumeDestroyed)
	{
		if (TargetSimulationVolume && IsValid(TargetSimulationVolume))
		{
			TargetSimulationVolume->OnDestroyed.RemoveDynamic(this, &UKawaiiFluidSimulationModule::OnTargetVolumeDestroyed);
		}
		bBoundToVolumeDestroyed = false;
	}
}

/**
 * @brief Synchronizes the module state when the preset is changed by an external source.
 * @param NewPreset The newly assigned preset asset.
 */
void UKawaiiFluidSimulationModule::OnPresetChangedExternal(UKawaiiFluidPresetDataAsset* NewPreset)
{
#if WITH_EDITOR
	// Unbind from old preset's delegate
	UnbindFromPresetPropertyChanged();
#endif

	// Update preset reference
	Preset = NewPreset;

	// Update SpatialHash if it exists
	if (SpatialHash.IsValid() && Preset)
	{
		SpatialHash = MakeShared<FKawaiiFluidSpatialHash>(Preset->SmoothingRadius);
	}

#if WITH_EDITOR
	// Bind to new preset's property changed delegate
	if (Preset)
	{
		BindToPresetPropertyChanged();
	}
#endif

	// Update CellSize and bounds display
	UpdateVolumeInfoDisplay();
}

#if WITH_EDITOR
/**
 * @brief Logic executed when a fluid preset property is modified in the editor.
 * @param ChangedPreset The modified preset.
 */
void UKawaiiFluidSimulationModule::OnPresetPropertyChanged(UKawaiiFluidPresetDataAsset* ChangedPreset)
{
	if (ChangedPreset != Preset)
	{
		return;
	}

	if (SpatialHash.IsValid() && Preset)
	{
		SpatialHash = MakeShared<FKawaiiFluidSpatialHash>(Preset->SmoothingRadius);
	}

	UpdateVolumeInfoDisplay();
}

/**
 * @brief Binds to the current preset's property change delegate.
 */
void UKawaiiFluidSimulationModule::BindToPresetPropertyChanged()
{
	UnbindFromPresetPropertyChanged();

	if (Preset)
	{
		PresetPropertyChangedHandle = Preset->OnPropertyChanged.AddUObject(
			this, &UKawaiiFluidSimulationModule::OnPresetPropertyChanged);
	}
}

/**
 * @brief Unbinds from the preset's property change delegate.
 */
void UKawaiiFluidSimulationModule::UnbindFromPresetPropertyChanged()
{
	if (PresetPropertyChangedHandle.IsValid())
	{
		if (Preset)
		{
			Preset->OnPropertyChanged.Remove(PresetPropertyChangedHandle);
		}
		PresetPropertyChangedHandle.Reset();
	}
}

/**
 * @brief Logic executed when objects are replaced during an asset reload.
 * @param ReplacementMap Map of old objects to new objects.
 */
void UKawaiiFluidSimulationModule::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	if (Preset)
	{
		if (UObject* const* NewPresetPtr = ReplacementMap.Find(Preset))
		{
			UKawaiiFluidPresetDataAsset* NewPreset = Cast<UKawaiiFluidPresetDataAsset>(*NewPresetPtr);

			UE_LOG(LogTemp, Log, TEXT("SimulationModule: Preset replaced via reload (Old=%p, New=%p)"),
				Preset.Get(), NewPreset);

			UnbindFromPresetPropertyChanged();

			Preset = NewPreset;
		
			if (SpatialHash.IsValid() && Preset)
			{
				SpatialHash = MakeShared<FKawaiiFluidSpatialHash>(Preset->SmoothingRadius);
			}

			if (Preset)
			{
				BindToPresetPropertyChanged();
			}

			UpdateVolumeInfoDisplay();
		}
	}
}

/**
 * @brief Binds to the global object replacement delegate.
 */
void UKawaiiFluidSimulationModule::BindToObjectsReplaced()
{
	UnbindFromObjectsReplaced();

	ObjectsReplacedHandle = FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(
		this, &UKawaiiFluidSimulationModule::OnObjectsReplaced);
}

/**
 * @brief Unbinds from the global object replacement delegate.
 */
void UKawaiiFluidSimulationModule::UnbindFromObjectsReplaced()
{
	if (ObjectsReplacedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectsReplaced.Remove(ObjectsReplacedHandle);
		ObjectsReplacedHandle.Reset();
	}
}
#endif
