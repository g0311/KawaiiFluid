// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rendering/IKawaiiFluidRenderable.h"
#include "Core/KawaiiFluidSimulationTypes.h"
#include "KawaiiFluidComponent.generated.h"

class UKawaiiFluidSimulationModule;
class FKawaiiFluidRenderResource;

/**
 * Particle hit event delegate
 * Called when a particle collides with an actor
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnFluidParticleHitComponent,
	int32, ParticleIndex,
	AActor*, HitActor,
	FVector, HitLocation,
	FVector, HitNormal,
	float, HitSpeed
);

/**
 * Kawaii Fluid Component (통합 컴포넌트)
 *
 * 유체 시뮬레이션을 위한 통합 컴포넌트입니다.
 * 모듈 기반 설계로 시뮬레이션/렌더링/충돌을 분리하여 관리합니다.
 *
 * 시뮬레이션 API는 SimulationModule을 통해 직접 접근:
 * - Component->SimulationModule->SpawnParticles(...)
 * - Component->SimulationModule->ApplyExternalForce(...)
 *
 * 사용:
 * @code
 * FluidComponent = CreateDefaultSubobject<UKawaiiFluidComponent>(TEXT("FluidComponent"));
 * FluidComponent->Preset = MyPreset;
 *
 * // Blueprint/C++에서 모듈 직접 접근
 * FluidComponent->SimulationModule->SpawnParticles(Location, 100, 50.0f);
 * @endcode
 */
UCLASS(ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent, DisplayName="Kawaii Fluid"))
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidComponent : public UActorComponent, public IKawaiiFluidRenderable
{
	GENERATED_BODY()

public:
	UKawaiiFluidComponent();

	//========================================
	// UActorComponent Interface
	//========================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//========================================
	// IKawaiiFluidRenderable Interface
	//========================================

	virtual FKawaiiFluidRenderResource* GetFluidRenderResource() const override;
	virtual bool IsFluidRenderResourceValid() const override;
	virtual float GetParticleRenderRadius() const override;
	virtual FString GetDebugName() const override;
	virtual bool ShouldUseSSFR() const override;
	virtual bool ShouldUseDebugMesh() const override;
	virtual UInstancedStaticMeshComponent* GetDebugMeshComponent() const override;
	virtual int32 GetParticleCount() const override;

	//========================================
	// Modules (Blueprint 직접 접근 가능)
	//========================================

	/** 시뮬레이션 모듈 - 파티클/콜라이더/외력 등 모든 API 제공 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Instanced, Category = "Fluid")
	TObjectPtr<UKawaiiFluidSimulationModule> SimulationModule;
	// TODO: UKawaiiFluidRenderingModule* RenderingModule;

	//========================================
	// Configuration
	//========================================

	/** Use world collision */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Collision")
	bool bUseWorldCollision = true;

	//========================================
	// Auto Spawn
	//========================================

	/** Spawn on begin play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn")
	bool bSpawnOnBeginPlay = true;

	/** Auto spawn count */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn", meta = (ClampMin = "1", ClampMax = "5000", EditCondition = "bSpawnOnBeginPlay"))
	int32 AutoSpawnCount = 100;

	/** Auto spawn radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn", meta = (ClampMin = "1.0", ClampMax = "500.0", EditCondition = "bSpawnOnBeginPlay"))
	float AutoSpawnRadius = 50.0f;

	//========================================
	// Continuous Spawn
	//========================================

	/** Enable continuous particle spawning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn")
	bool bContinuousSpawn = false;

	/** Particles to spawn per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn", meta = (ClampMin = "0.1", ClampMax = "1000.0", EditCondition = "bContinuousSpawn"))
	float ParticlesPerSecond = 10.0f;

	/** Maximum particle count (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn", meta = (ClampMin = "0", EditCondition = "bContinuousSpawn"))
	int32 MaxParticleCount = 500;

	/** Spawn radius for continuous spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn", meta = (ClampMin = "0.0", ClampMax = "100.0", EditCondition = "bContinuousSpawn"))
	float ContinuousSpawnRadius = 10.0f;

	/** Spawn offset from component location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn", meta = (EditCondition = "bContinuousSpawn"))
	FVector SpawnOffset = FVector::ZeroVector;

	/** Initial velocity for spawned particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Spawn", meta = (EditCondition = "bContinuousSpawn"))
	FVector SpawnVelocity = FVector::ZeroVector;

	//========================================
	// Debug Rendering
	//========================================

	/** Particle render radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Debug", meta = (ClampMin = "0.1", ClampMax = "50.0"))
	float ParticleRenderRadius = 5.0f;

	//========================================
	// Events
	//========================================

	/** Particle hit event (Blueprint bindable) */
	UPROPERTY(BlueprintAssignable, Category = "Fluid|Events")
	FOnFluidParticleHitComponent OnParticleHit;

	/** Enable particle hit events (performance consideration - default off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events")
	bool bEnableParticleHitEvents = false;

	/** Minimum velocity for collision event (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events", meta = (ClampMin = "0.0", EditCondition = "bEnableParticleHitEvents"))
	float MinVelocityForEvent = 50.0f;

	/** Maximum events per frame (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events", meta = (ClampMin = "0", EditCondition = "bEnableParticleHitEvents"))
	int32 MaxEventsPerFrame = 10;

	/** Per-particle event cooldown in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid|Events", meta = (ClampMin = "0.0", EditCondition = "bEnableParticleHitEvents"))
	float EventCooldownPerParticle = 0.1f;

	//========================================
	// Component-Level API
	//========================================

	/** Build simulation params (World, IgnoreActor 등 컴포넌트에서만 접근 가능한 값 포함) */
	UFUNCTION(BlueprintCallable, Category = "Fluid")
	FKawaiiFluidSimulationParams BuildSimulationParams();

	/** Check if this component should simulate independently */
	bool ShouldSimulateIndependently() const;

private:
	//========================================
	// Continuous Spawn
	//========================================

	float SpawnAccumulatedTime = 0.0f;
	void ProcessContinuousSpawn(float DeltaTime);

	//========================================
	// Event System
	//========================================

	/** Event count this frame (for MaxEventsPerFrame limiting) */
	int32 EventCountThisFrame = 0;

	/** Handle collision event from simulation context */
	void HandleCollisionEvent(const FKawaiiFluidCollisionEvent& Event);

	//========================================
	// Subsystem Registration
	//========================================

	void RegisterToSubsystem();
	void UnregisterFromSubsystem();
};
