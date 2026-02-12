// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Core/KawaiiFluidSimulationTypes.h"
#include "Components/KawaiiFluidInteractionComponent.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "KawaiiFluidSimulatorSubsystem.generated.h"

class UKawaiiFluidSimulationModule;
class UKawaiiFluidSimulationContext;
class UKawaiiFluidPresetDataAsset;
class UKawaiiFluidVolumeComponent;
class AKawaiiFluidVolume;
class AKawaiiFluidEmitter;
class UKawaiiFluidCollider;
class UKawaiiFluidInteractionComponent;
class AActor;
class ULevel;
class FKawaiiFluidSpatialHash;
struct FKawaiiFluidParticle;
class FGPUFluidSimulator;

/**
 * @struct FContextCacheKey
 * @brief Cache key for looking up simulation contexts based on volume and preset.
 * 
 * @param VolumeComponent Target Volume for Z-Order space bounds (nullptr = component-relative bounds).
 * @param Preset Associated fluid preset data asset.
 */
USTRUCT()
struct FContextCacheKey
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UKawaiiFluidVolumeComponent> VolumeComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UKawaiiFluidPresetDataAsset> Preset = nullptr;

	FContextCacheKey() = default;
	FContextCacheKey(UKawaiiFluidVolumeComponent* InVolumeComponent, UKawaiiFluidPresetDataAsset* InPreset)
		: VolumeComponent(InVolumeComponent), Preset(InPreset) {}

	explicit FContextCacheKey(UKawaiiFluidPresetDataAsset* InPreset)
		: VolumeComponent(nullptr), Preset(InPreset) {}

	bool operator==(const FContextCacheKey& Other) const
	{
		return VolumeComponent == Other.VolumeComponent && Preset == Other.Preset;
	}

	friend uint32 GetTypeHash(const FContextCacheKey& Key)
	{
		uint32 Hash = GetTypeHash(Key.VolumeComponent);
		Hash = HashCombine(Hash, GetTypeHash(Key.Preset));
		return Hash;
	}
};

/**
 * @class UKawaiiFluidSimulatorSubsystem
 * @brief Orchestration subsystem that manages all fluid simulations in the world.
 * 
 * Responsible for component registration, batched simulation management,
 * global collider coordination, and providing a query API for fluid particles.
 * 
 * @param AllModules All currently registered simulation modules.
 * @param UsedSourceIDs Bitfield tracking assigned SourceIDs for GPU tracking.
 * @param NextSourceIDHint Optimization hint for the next SourceID allocation.
 * @param AllVolumes All registered fluid volume actors (New Architecture).
 * @param AllVolumeComponents All registered simulation volume components (Legacy).
 * @param GlobalColliders Colliders that affect all fluid simulations globally.
 * @param GlobalInteractionComponents Interaction components used for global bone tracking.
 * @param ContextCache Mapping of Volume/Preset pairs to active simulation contexts.
 * @param DefaultContext Fallback context used when no specific volume is assigned.
 * @param SharedSpatialHash Shared resource used for neighbor finding in batched simulations.
 * @param ModuleBatchInfos Metadata describing the current simulation batches.
 * @param MergedFluidParticleBuffer Temporary buffer holding merged particles for batch processing.
 * @param EventCountThisFrame Atomic counter for tracking collision events within a frame.
 * @param CPUCollisionFeedbackBuffer Buffer for deferred collision event processing on the CPU.
 * @param CPUCollisionFeedbackLock Synchronization lock for the CPU feedback buffer.
 * @param OnActorSpawnedHandle Delegate handle for tracking actor spawning.
 * @param OnLevelAddedHandle Delegate handle for tracking level addition.
 * @param OnLevelRemovedHandle Delegate handle for tracking level removal.
 * @param OnPostActorTickHandle Delegate handle for the post-actor tick simulation pass.
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidSimulatorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UKawaiiFluidSimulatorSubsystem();
	virtual ~UKawaiiFluidSimulatorSubsystem() override;

	//========================================
	// USubsystem Interface
	//========================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	//========================================
	// Module Registration
	//========================================

	void RegisterModule(UKawaiiFluidSimulationModule* Module);

	void UnregisterModule(UKawaiiFluidSimulationModule* Module);

	const TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& GetAllModules() const { return AllModules; }

	//========================================
	// SourceID Allocation
	//========================================

	int32 AllocateSourceID();

	void ReleaseSourceID(int32 SourceID);

	//========================================
	// Volume Actor Registration
	//========================================

	void RegisterVolume(AKawaiiFluidVolume* Volume);

	void UnregisterVolume(AKawaiiFluidVolume* Volume);

	const TArray<TObjectPtr<AKawaiiFluidVolume>>& GetAllVolumes() const { return AllVolumes; }

	//========================================
	// Volume Component Registration (Legacy)
	//========================================

	void RegisterVolumeComponent(UKawaiiFluidVolumeComponent* VolumeComponent);

	void UnregisterVolumeComponent(UKawaiiFluidVolumeComponent* VolumeComponent);

	const TArray<TObjectPtr<UKawaiiFluidVolumeComponent>>& GetAllVolumeComponents() const { return AllVolumeComponents; }

	//========================================
	// Global Colliders
	//========================================

	UFUNCTION(BlueprintCallable, Category = "KawaiiFluid")
	void RegisterGlobalCollider(UKawaiiFluidCollider* Collider);

	UFUNCTION(BlueprintCallable, Category = "KawaiiFluid")
	void UnregisterGlobalCollider(UKawaiiFluidCollider* Collider);

	const TArray<TObjectPtr<UKawaiiFluidCollider>>& GetGlobalColliders() const { return GlobalColliders; }

	//========================================
	// Global Interaction Components
	//========================================

	void RegisterGlobalInteractionComponent(UKawaiiFluidInteractionComponent* Component);

	void UnregisterGlobalInteractionComponent(UKawaiiFluidInteractionComponent* Component);

	const TArray<TObjectPtr<UKawaiiFluidInteractionComponent>>& GetGlobalInteractionComponents() const { return GlobalInteractionComponents; }

	//========================================
	// Query API
	//========================================

	UFUNCTION(BlueprintCallable, Category = "KawaiiFluid|Query")
	TArray<FKawaiiFluidParticle> GetAllParticlesInRadius(FVector Location, float Radius) const;

	UFUNCTION(BlueprintCallable, Category = "KawaiiFluid|Query")
	int32 GetTotalParticleCount() const;

	UFUNCTION(BlueprintCallable, Category = "KawaiiFluid|Query")
	int32 GetModuleCount() const { return AllModules.Num(); }

	UFUNCTION(BlueprintPure, Category = "KawaiiFluid|Query")
	UKawaiiFluidPresetDataAsset* GetPresetBySourceID(int32 SourceID) const;

	UKawaiiFluidSimulationModule* GetModuleBySourceID(int32 SourceID) const;

	//========================================
	// Context Management
	//========================================

	UKawaiiFluidSimulationContext* GetOrCreateContext(UKawaiiFluidVolumeComponent* VolumeComponent, UKawaiiFluidPresetDataAsset* Preset);

	UKawaiiFluidSimulationContext* GetOrCreateContext(UKawaiiFluidPresetDataAsset* Preset)
	{
		return GetOrCreateContext(nullptr, Preset);
	}

	void GetAllGPUSimulators(TArray<FGPUFluidSimulator*>& OutSimulators) const;

private:
	//========================================
	// Module Management
	//========================================

	UPROPERTY()
	TArray<TObjectPtr<UKawaiiFluidSimulationModule>> AllModules;

	//========================================
	// SourceID Allocation State
	//========================================

	TBitArray<> UsedSourceIDs;

	int32 NextSourceIDHint = 0;

	//========================================
	// Volume Actor Management
	//========================================

	UPROPERTY()
	TArray<TObjectPtr<AKawaiiFluidVolume>> AllVolumes;

	//========================================
	// Volume Component Management (Legacy)
	//========================================

	UPROPERTY()
	TArray<TObjectPtr<UKawaiiFluidVolumeComponent>> AllVolumeComponents;

	//========================================
	// Component Management
	//========================================

	UPROPERTY()
	TArray<TObjectPtr<UKawaiiFluidCollider>> GlobalColliders;

	UPROPERTY()
	TArray<TObjectPtr<UKawaiiFluidInteractionComponent>> GlobalInteractionComponents;

	UPROPERTY()
	TMap<FContextCacheKey, TObjectPtr<UKawaiiFluidSimulationContext>> ContextCache;

	UPROPERTY()
	TObjectPtr<UKawaiiFluidSimulationContext> DefaultContext;

	//========================================
	// Batching Resources
	//========================================

	TSharedPtr<FKawaiiFluidSpatialHash> SharedSpatialHash;

	TArray<FKawaiiFluidModuleBatchInfo> ModuleBatchInfos;

	TArray<FKawaiiFluidParticle> MergedFluidParticleBuffer;

	std::atomic<int32> EventCountThisFrame{0};

	//========================================
	// CPU Collision Feedback Buffer
	//========================================

	TArray<FKawaiiFluidCollisionEvent> CPUCollisionFeedbackBuffer;

	FCriticalSection CPUCollisionFeedbackLock;

	//========================================
	// Simulation Methods
	//========================================

	void SimulateIndependentFluidComponents(float DeltaTime);

	void SimulateBatchedFluidComponents(float DeltaTime);

	TMap<FContextCacheKey, TArray<TObjectPtr<UKawaiiFluidSimulationModule>>> GroupModulesByContext() const;

	void MergeModuleParticles(const TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& Modules);

	void SplitModuleParticles(const TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& Modules);

	FKawaiiFluidSimulationParams BuildMergedModuleSimulationParams(const TArray<TObjectPtr<UKawaiiFluidSimulationModule>>& Modules);

	//========================================
	// World Change Tracking
	//========================================

	void HandleActorSpawned(AActor* Actor);
	void HandleActorDestroyed(AActor* Actor);
	void HandleLevelAdded(ULevel* InLevel, UWorld* InWorld);
	void HandleLevelRemoved(ULevel* InLevel, UWorld* InWorld);
	void MarkAllContextsWorldCollisionDirty();

	void HandlePostActorTick(UWorld* World, ELevelTick TickType, float DeltaTime);

	FDelegateHandle OnActorSpawnedHandle;
	FDelegateHandle OnLevelAddedHandle;
	FDelegateHandle OnLevelRemovedHandle;
	FDelegateHandle OnPostActorTickHandle;
};
