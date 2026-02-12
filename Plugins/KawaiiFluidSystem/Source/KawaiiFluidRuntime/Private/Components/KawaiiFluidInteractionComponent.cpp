// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Components/KawaiiFluidInteractionComponent.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Collision/KawaiiFluidMeshCollider.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Simulation/GPUFluidSimulator.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/PositionVertexBuffer.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

/**
 * @brief Default constructor for UKawaiiFluidInteractionComponent.
 */
UKawaiiFluidInteractionComponent::UKawaiiFluidInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	TargetSubsystem = nullptr;
	bAutoCreateCollider = true;
	AutoCollider = nullptr;
}

/**
 * @brief Called when the component is registered. Handles editor-side subsystem registration.
 */
void UKawaiiFluidInteractionComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Register with Subsystem in editor mode (for brush mode)
	UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		if (!TargetSubsystem)
		{
			TargetSubsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>();
		}

		if (bAutoCreateCollider && !AutoCollider)
		{
			CreateAutoCollider();
		}

		RegisterWithSimulator();
	}
#endif
}

/**
 * @brief Called when the component is unregistered.
 */
void UKawaiiFluidInteractionComponent::OnUnregister()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		UnregisterFromSimulator();
	}
#endif

	Super::OnUnregister();
}

/**
 * @brief Called when the game starts. Initializes subsystem reference and boundary particles.
 */
void UKawaiiFluidInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Find subsystem automatically
	if (!TargetSubsystem)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			TargetSubsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>();
		}
	}

	if (bAutoCreateCollider)
	{
		CreateAutoCollider();
	}

	RegisterWithSimulator();

	// Generate boundary particles (Flex-style Adhesion)
	if (bEnableBoundaryParticles)
	{
		GenerateBoundaryParticles();
	}
}

/**
 * @brief Called when the component is destroyed.
 * @param EndPlayReason Termination reason
 */
void UKawaiiFluidInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSimulator();
	Super::EndPlay(EndPlayReason);
}

/**
 * @brief Main update loop. Processes GPU feedback and applies physics forces.
 * @param DeltaTime Frame time
 * @param TickType Tick type
 * @param ThisTickFunction Function reference
 */
void UKawaiiFluidInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!TargetSubsystem)
	{
		return;
	}

	// Process GPU Collision Feedback (Particle -> Player Interaction)
	if (bEnableForceFeedback || bEnableAutoPhysicsForces)
	{
		EnableGPUCollisionFeedbackIfNeeded();
		ProcessCollisionFeedback(DeltaTime);
	}

	// Update boundary particles and debug display (Flex-style Adhesion)
	if (bEnableBoundaryParticles && bBoundaryParticlesInitialized)
	{
		UpdateBoundaryParticlePositions();

		if (bShowBoundaryParticles)
		{
			DrawDebugBoundaryParticles();
		}
	}

	// Apply automatic physics forces (buoyancy + drag) for physics-simulating objects
	if (bEnableAutoPhysicsForces)
	{
		ApplyAutoPhysicsForces(DeltaTime);
	}
}

/**
 * @brief Creates a mesh collider automatically based on the owner's mesh components.
 */
void UKawaiiFluidInteractionComponent::CreateAutoCollider()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	AutoCollider = NewObject<UKawaiiFluidMeshCollider>(Owner);
	if (AutoCollider)
	{
		AutoCollider->RegisterComponent();

		// Auto-set TargetMeshComponent
		USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (SkelMesh)
		{
			AutoCollider->TargetMeshComponent = SkelMesh;
		}
		else
		{
			UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
			if (Capsule)
			{
				AutoCollider->TargetMeshComponent = Capsule;
			}
			else
			{
				UStaticMeshComponent* StaticMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
				if (StaticMesh)
				{
					AutoCollider->TargetMeshComponent = StaticMesh;
				}
			}
		}
	}
}

/**
 * @brief Registers the component and its auto-collider with the fluid subsystem.
 */
void UKawaiiFluidInteractionComponent::RegisterWithSimulator()
{
	if (TargetSubsystem)
	{
		if (AutoCollider)
		{
			TargetSubsystem->RegisterGlobalCollider(AutoCollider);
		}
		TargetSubsystem->RegisterGlobalInteractionComponent(this);
	}
}

/**
 * @brief Unregisters from the fluid subsystem.
 */
void UKawaiiFluidInteractionComponent::UnregisterFromSimulator()
{
	if (TargetSubsystem)
	{
		if (AutoCollider)
		{
			TargetSubsystem->UnregisterGlobalCollider(AutoCollider);
		}
		TargetSubsystem->UnregisterGlobalInteractionComponent(this);
	}
}

/**
 * @brief Detaches all fluid particles currently attached to this component's owner.
 */
void UKawaiiFluidInteractionComponent::DetachAllFluid()
{
	AActor* Owner = GetOwner();

	auto DetachFromParticles = [Owner](TArray<FKawaiiFluidParticle>& Particles)
	{
		for (FKawaiiFluidParticle& Particle : Particles)
		{
			if (Particle.bIsAttached && Particle.AttachedActor.Get() == Owner)
			{
				Particle.bIsAttached = false;
				Particle.AttachedActor.Reset();
				Particle.AttachedBoneName = NAME_None;
				Particle.AttachedLocalOffset = FVector::ZeroVector;
			}
		}
	};

	if (TargetSubsystem)
	{
		for (auto Module : TargetSubsystem->GetAllModules())
		{
			if (!Module) continue;
			DetachFromParticles(Module->GetParticlesMutable());
		}
	}
}

/**
 * @brief Applies an external push force to nearby fluid particles.
 * @param Direction Push direction
 * @param Force Push magnitude
 */
void UKawaiiFluidInteractionComponent::PushFluid(FVector Direction, float Force)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector NormalizedDir = Direction.GetSafeNormal();
	FVector OwnerLocation = Owner->GetActorLocation();

	auto PushParticles = [Owner, NormalizedDir, OwnerLocation, Force](TArray<FKawaiiFluidParticle>& Particles)
	{
		for (FKawaiiFluidParticle& Particle : Particles)
		{
			float Distance = FVector::Dist(Particle.Position, OwnerLocation);

			if (Distance < 200.0f)
			{
				float FallOff = 1.0f - (Distance / 200.0f);
				Particle.Velocity += NormalizedDir * Force * FallOff;

				if (Particle.bIsAttached && Particle.AttachedActor.Get() == Owner)
				{
					Particle.bIsAttached = false;
					Particle.AttachedActor.Reset();
					Particle.AttachedBoneName = NAME_None;
					Particle.AttachedLocalOffset = FVector::ZeroVector;
				}
			}
		}
	};

	if (TargetSubsystem)
	{
		for (auto Module : TargetSubsystem->GetAllModules())
		{
			if (!Module) continue;
			PushParticles(Module->GetParticlesMutable());
		}
	}
}

/**
 * @brief Processes GPU collision feedback data to compute drag forces and buoyancy.
 * @param DeltaTime Time step
 */
void UKawaiiFluidInteractionComponent::ProcessCollisionFeedback(float DeltaTime)
{
	AActor* Owner = GetOwner();
	const int32 MyOwnerID = Owner ? Owner->GetUniqueID() : 0;

	if (!TargetSubsystem)
	{
		CurrentContactCount = 0;
		if (PreviousContactCount > 0)
		{
			SmoothedForce = FVector::ZeroVector;
			EstimatedBuoyancyCenterOffset = FVector::ZeroVector;
		}
		else
		{
			SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed);
			EstimatedBuoyancyCenterOffset = FMath::VInterpTo(EstimatedBuoyancyCenterOffset, FVector::ZeroVector, DeltaTime, 2.0f);
		}
		CurrentFluidForce = SmoothedForce;
		CurrentAveragePressure = 0.0f;
		PreviousContactCount = CurrentContactCount;
		return;
	}

	CurrentFluidTagCounts.Empty();
	CurrentContactCount = 0;

	TArray<FGPUCollisionFeedback> AllFeedback;
	int32 TotalFeedbackCount = 0;
	FGPUFluidSimulator* PrimaryGPUSimulator = nullptr;
	UKawaiiFluidSimulationModule* PrimarySourceModule = nullptr;

	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (!Module) continue;
		FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
		if (!GPUSimulator || GPUSimulator->GetParticleCount() <= 0) continue;

		const int32 ModuleContactCount = GPUSimulator->GetContactCountForOwner(MyOwnerID);
		if (ModuleContactCount > 0)
		{
			FName FluidTag = NAME_None;
			if (UKawaiiFluidPresetDataAsset* Preset = Module->GetPreset()) FluidTag = Preset->GetFluidName();
			CurrentFluidTagCounts.FindOrAdd(FluidTag) += ModuleContactCount;
			CurrentContactCount += ModuleContactCount;
		}

		if (GPUSimulator->IsCollisionFeedbackEnabled())
		{
			TArray<FGPUCollisionFeedback> ModuleFeedback;
			int32 ModuleFeedbackCount = 0;
			GPUSimulator->GetAllCollisionFeedback(ModuleFeedback, ModuleFeedbackCount);
			if (ModuleFeedbackCount > 0)
			{
				AllFeedback.Append(ModuleFeedback);
				TotalFeedbackCount += ModuleFeedbackCount;
				if (!PrimaryGPUSimulator) { PrimaryGPUSimulator = GPUSimulator; PrimarySourceModule = Module; }
			}
		}
	}

	FGPUFluidSimulator* GPUSimulator = PrimaryGPUSimulator;
	UKawaiiFluidSimulationModule* SourceModule = PrimarySourceModule;
	int32 FeedbackCount = TotalFeedbackCount;

	if (!GPUSimulator && CurrentContactCount == 0)
	{
		if (PreviousContactCount > 0) { SmoothedForce = FVector::ZeroVector; EstimatedBuoyancyCenterOffset = FVector::ZeroVector; }
		else { SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed); EstimatedBuoyancyCenterOffset = FMath::VInterpTo(EstimatedBuoyancyCenterOffset, FVector::ZeroVector, DeltaTime, 2.0f); }
		CurrentFluidForce = SmoothedForce; CurrentAveragePressure = 0.0f; PreviousContactCount = CurrentContactCount; return;
	}

	if (GPUSimulator && GPUSimulator->IsCollisionFeedbackEnabled() && FeedbackCount > 0)
	{
		if (bEnablePerBoneForce)
		{
			const float ParticleRadius = FMath::Max(SourceModule ? SourceModule->GetParticleRadius() : 3.0f, 0.1f);
			ProcessPerBoneForces(DeltaTime, AllFeedback, FeedbackCount, ParticleRadius);
			ProcessBoneCollisionEvents(DeltaTime, AllFeedback, FeedbackCount);
		}

		if (FeedbackCount > 0)
		{
			const float ParticleRadius = FMath::Max(SourceModule ? SourceModule->GetParticleRadius() : 3.0f, 0.1f);
			const float AreaInM2 = PI * ParticleRadius * ParticleRadius * 0.0001f;

			FVector BodyVelocity = FVector::ZeroVector;
			if (Owner)
			{
				if (UCharacterMovementComponent* MovementComp = Owner->FindComponentByClass<UCharacterMovementComponent>()) BodyVelocity = MovementComp->Velocity;
				else if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent())) BodyVelocity = RootPrimitive->GetPhysicsLinearVelocity();
			}
			const FVector BodyVelocityInMS = BodyVelocity * 0.01f;

			FVector ForceAccum = FVector::ZeroVector;
			float DensitySum = 0.0f;
			int32 ForceContactCount = 0;

			for (int32 i = 0; i < FeedbackCount; ++i)
			{
				const FGPUCollisionFeedback& Feedback = AllFeedback[i];
				if (Feedback.ColliderOwnerID != 0 && Feedback.ColliderOwnerID != MyOwnerID) continue;

				FVector ParticleVelocityInMS = FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z) * 0.01f;
				FVector EffectiveVelocity = bUseRelativeVelocityForForce ? (ParticleVelocityInMS - BodyVelocityInMS) : ParticleVelocityInMS;
				float EffectiveSpeed = EffectiveVelocity.Size();

				DensitySum += Feedback.Density;
				ForceContactCount++;

				if (EffectiveSpeed < SMALL_NUMBER) continue;
				float ImpactMagnitude = 0.5f * Feedback.Density * DragCoefficient * AreaInM2 * EffectiveSpeed * EffectiveSpeed;
				ForceAccum += EffectiveVelocity.GetSafeNormal() * ImpactMagnitude;
			}
			ForceAccum *= 100.0f;
			SmoothedForce = FMath::VInterpTo(SmoothedForce, ForceAccum * DragForceMultiplier, DeltaTime, ForceSmoothingSpeed);
			CurrentFluidForce = SmoothedForce;
			CurrentAveragePressure = (ForceContactCount > 0) ? (DensitySum / ForceContactCount) : 0.0f;
		}
		else
		{
			SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed);
			CurrentFluidForce = SmoothedForce; CurrentAveragePressure = 0.0f;
		}

		TArray<FGPUCollisionFeedback> FluidInteractionSMFeedback;
		int32 FluidInteractionSMFeedbackCount = 0;
		GPUSimulator->GetAllFluidInteractionSMCollisionFeedback(FluidInteractionSMFeedback, FluidInteractionSMFeedbackCount);

		FVector ParticlePositionAccum = FVector::ZeroVector;
		int32 BuoyancyContactCount = 0;
		for (int32 i = 0; i < FluidInteractionSMFeedbackCount; ++i)
		{
			const FGPUCollisionFeedback& Feedback = FluidInteractionSMFeedback[i];
			if (Feedback.ColliderOwnerID != 0 && Feedback.ColliderOwnerID != MyOwnerID) continue;
			FVector ParticlePos(Feedback.ParticlePosition.X, Feedback.ParticlePosition.Y, Feedback.ParticlePosition.Z);
			if (!ParticlePos.IsNearlyZero()) { ParticlePositionAccum += ParticlePos; BuoyancyContactCount++; }
		}

		const float BuoyancyCenterSmoothingSpeed = 0.5f;
		if (BuoyancyContactCount > 0)
		{
			if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
			{
				FVector TargetOffset = (ParticlePositionAccum / BuoyancyContactCount) - RootPrimitive->GetComponentLocation();
				EstimatedBuoyancyCenterOffset = FMath::VInterpTo(EstimatedBuoyancyCenterOffset, TargetOffset, DeltaTime, BuoyancyCenterSmoothingSpeed);
			}
		}
		else
		{
			if (PreviousContactCount > 0 && CurrentContactCount <= 0) EstimatedBuoyancyCenterOffset = FVector::ZeroVector;
			else EstimatedBuoyancyCenterOffset = FMath::VInterpTo(EstimatedBuoyancyCenterOffset, FVector::ZeroVector, DeltaTime, BuoyancyCenterSmoothingSpeed * 0.5f);
		}
	}
	else
	{
		SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed);
		CurrentFluidForce = SmoothedForce; CurrentAveragePressure = 0.0f;
		EstimatedBuoyancyCenterOffset = FMath::VInterpTo(EstimatedBuoyancyCenterOffset, FVector::ZeroVector, DeltaTime, 2.0f);
	}

	if (OnFluidForceUpdate.IsBound()) OnFluidForceUpdate.Broadcast(CurrentFluidForce, CurrentAveragePressure, CurrentContactCount);
	if (bEnableBoneImpactMonitoring && OnBoneFluidImpact.IsBound()) CheckBoneImpacts();
	PreviousContactCount = CurrentContactCount;
	UpdateFluidTagEvents();
}

/**
 * @brief Checks for fluid tag changes and broadcasts Enter/Exit events.
 */
void UKawaiiFluidInteractionComponent::UpdateFluidTagEvents()
{
	TSet<FName> CurrentlyColliding;
	for (const auto& Pair : CurrentFluidTagCounts)
	{
		if (Pair.Value >= MinParticleCountForFluidEvent) CurrentlyColliding.Add(Pair.Key);
	}

	for (auto& Pair : PreviousFluidTagStates)
	{
		if (Pair.Value && !CurrentlyColliding.Contains(Pair.Key))
		{
			if (OnFluidExit.IsBound()) OnFluidExit.Broadcast(Pair.Key);
			Pair.Value = false;
		}
	}

	for (const FName& Tag : CurrentlyColliding)
	{
		bool* bWasCollidingWithTag = PreviousFluidTagStates.Find(Tag);
		if (!bWasCollidingWithTag || !(*bWasCollidingWithTag))
		{
			int32 Count = CurrentFluidTagCounts.FindOrAdd(Tag);
			if (OnFluidEnter.IsBound()) OnFluidEnter.Broadcast(Tag, Count);
			PreviousFluidTagStates.FindOrAdd(Tag) = true;
		}
	}
}

/**
 * @brief Monitors specified bones for high-speed fluid impacts.
 */
void UKawaiiFluidInteractionComponent::CheckBoneImpacts()
{
	if (MonitoredBones.Num() == 0) return;
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* SkelMesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;

	for (const FName& BoneName : MonitoredBones)
	{
		float ImpactSpeed = GetFluidImpactSpeedForBone(BoneName);
		if (ImpactSpeed > BoneImpactSpeedThreshold)
		{
			float ImpactForce = GetFluidImpactForceMagnitudeForBone(BoneName);
			FVector ImpactDirection = GetFluidImpactDirectionForBone(BoneName);
			OnBoneFluidImpact.Broadcast(BoneName, ImpactSpeed, ImpactForce, ImpactDirection);
		}
	}
}

/**
 * @brief Integrates fluid force with the CharacterMovementComponent.
 * @param ForceScale Magnitude multiplier
 */
void UKawaiiFluidInteractionComponent::ApplyFluidForceToCharacterMovement(float ForceScale)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	UCharacterMovementComponent* MovementComp = Owner->FindComponentByClass<UCharacterMovementComponent>();
	if (!MovementComp) return;

	FVector ScaledForce = CurrentFluidForce * ForceScale;
	if (!ScaledForce.IsNearlyZero()) MovementComp->AddForce(ScaledForce);
}

/**
 * @brief Checks if currently colliding with a specific fluid tag.
 * @param FluidTag Fluid name to check
 * @return True if colliding
 */
bool UKawaiiFluidInteractionComponent::IsCollidingWithFluidTag(FName FluidTag) const
{
	const bool* bIsColliding = PreviousFluidTagStates.Find(FluidTag);
	return bIsColliding && *bIsColliding;
}

/**
 * @brief Calculates average speed of all colliding fluid particles.
 * @return Speed in cm/s
 */
float UKawaiiFluidInteractionComponent::GetFluidImpactSpeed() const
{
	if (!TargetSubsystem) return 0.0f;
	float TotalSpeed = 0.0f; int32 TotalFeedbackCount = 0;
	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (!Module) continue;
		FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
		if (!GPUSimulator) continue;
		TArray<FGPUCollisionFeedback> CollisionFeedbacks; int32 FeedbackCount = 0;
		GPUSimulator->GetAllCollisionFeedback(CollisionFeedbacks, FeedbackCount);
		for (const FGPUCollisionFeedback& Feedback : CollisionFeedbacks)
		{
			TotalSpeed += FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z).Size();
			TotalFeedbackCount++;
		}
	}
	return (TotalFeedbackCount > 0) ? (TotalSpeed / TotalFeedbackCount) : 0.0f;
}

/**
 * @brief Calculates average impact force magnitude across all colliding particles.
 * @return Force in Newtons
 */
float UKawaiiFluidInteractionComponent::GetFluidImpactForceMagnitude() const
{
	if (!TargetSubsystem) return 0.0f;
	const float AreaInM2 = 0.01f; float TotalForceMagnitude = 0.0f;
	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (!Module) continue;
		FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
		if (!GPUSimulator) continue;
		TArray<FGPUCollisionFeedback> CollisionFeedbacks; int32 FeedbackCount = 0;
		GPUSimulator->GetAllCollisionFeedback(CollisionFeedbacks, FeedbackCount);
		for (const FGPUCollisionFeedback& Feedback : CollisionFeedbacks)
		{
			float ParticleSpeed = FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z).Size() * 0.01f;
			TotalForceMagnitude += 0.5f * Feedback.Density * 1.0f * AreaInM2 * ParticleSpeed * ParticleSpeed;
		}
	}
	return TotalForceMagnitude;
}

/**
 * @brief Calculates the average impact direction of colliding fluid.
 * @return Normalized direction vector
 */
FVector UKawaiiFluidInteractionComponent::GetFluidImpactDirection() const
{
	if (!TargetSubsystem) return FVector::ZeroVector;
	FVector TotalVelocity = FVector::ZeroVector; int32 TotalFeedbackCount = 0;
	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (!Module) continue;
		FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
		if (!GPUSimulator) continue;
		TArray<FGPUCollisionFeedback> CollisionFeedbacks; int32 FeedbackCount = 0;
		GPUSimulator->GetAllCollisionFeedback(CollisionFeedbacks, FeedbackCount);
		for (const FGPUCollisionFeedback& Feedback : CollisionFeedbacks)
		{
			TotalVelocity += FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z);
			TotalFeedbackCount++;
		}
	}
	return (TotalFeedbackCount > 0 && !TotalVelocity.IsNearlyZero()) ? TotalVelocity.GetSafeNormal() : FVector::ZeroVector;
}

/**
 * @brief Returns impact speed for a specific bone.
 * @param BoneName Target bone name
 * @return Speed in cm/s
 */
float UKawaiiFluidInteractionComponent::GetFluidImpactSpeedForBone(FName BoneName) const
{
	if (!TargetSubsystem) return 0.0f;
	AActor* Owner = GetOwner(); if (!Owner) return 0.0f;
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return 0.0f;
	int32 TargetBoneIndex = SkelMesh->GetBoneIndex(BoneName);
	if (TargetBoneIndex == INDEX_NONE) return 0.0f;

	float TotalSpeed = 0.0f; int32 TotalFeedbackCount = 0;
	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (!Module) continue;
		FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
		if (!GPUSimulator) continue;
		TArray<FGPUCollisionFeedback> CollisionFeedbacks; int32 FeedbackCount = 0;
		GPUSimulator->GetAllCollisionFeedback(CollisionFeedbacks, FeedbackCount);
		for (const FGPUCollisionFeedback& Feedback : CollisionFeedbacks)
		{
			if (Feedback.BoneIndex != TargetBoneIndex) continue;
			TotalSpeed += FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z).Size();
			TotalFeedbackCount++;
		}
	}
	return (TotalFeedbackCount > 0) ? (TotalSpeed / TotalFeedbackCount) : 0.0f;
}

/**
 * @brief Returns impact force magnitude for a specific bone.
 * @param BoneName Target bone name
 * @return Force in Newtons
 */
float UKawaiiFluidInteractionComponent::GetFluidImpactForceMagnitudeForBone(FName BoneName) const
{
	if (!TargetSubsystem) return 0.0f;
	AActor* Owner = GetOwner(); if (!Owner) return 0.0f;
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return 0.0f;
	int32 TargetBoneIndex = SkelMesh->GetBoneIndex(BoneName);
	if (TargetBoneIndex == INDEX_NONE) return 0.0f;

	const float AreaInM2 = 0.01f; float TotalForceMagnitude = 0.0f;
	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (!Module) continue;
		FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
		if (!GPUSimulator) continue;
		TArray<FGPUCollisionFeedback> CollisionFeedbacks; int32 FeedbackCount = 0;
		GPUSimulator->GetAllCollisionFeedback(CollisionFeedbacks, FeedbackCount);
		for (const FGPUCollisionFeedback& Feedback : CollisionFeedbacks)
		{
			if (Feedback.BoneIndex != TargetBoneIndex) continue;
			float ParticleSpeed = FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z).Size() * 0.01f;
			TotalForceMagnitude += 0.5f * Feedback.Density * 1.0f * AreaInM2 * ParticleSpeed * ParticleSpeed;
		}
	}
	return TotalForceMagnitude;
}

/**
 * @brief Returns normalized impact direction for a specific bone.
 * @param BoneName Target bone name
 * @return Local direction vector
 */
FVector UKawaiiFluidInteractionComponent::GetFluidImpactDirectionForBone(FName BoneName) const
{
	if (!TargetSubsystem) return FVector::ZeroVector;
	AActor* Owner = GetOwner(); if (!Owner) return FVector::ZeroVector;
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return FVector::ZeroVector;
	int32 TargetBoneIndex = SkelMesh->GetBoneIndex(BoneName);
	if (TargetBoneIndex == INDEX_NONE) return FVector::ZeroVector;

	FVector TotalVelocity = FVector::ZeroVector; int32 TotalFeedbackCount = 0;
	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (!Module) continue;
		FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
		if (!GPUSimulator) continue;
		TArray<FGPUCollisionFeedback> CollisionFeedbacks; int32 FeedbackCount = 0;
		GPUSimulator->GetAllCollisionFeedback(CollisionFeedbacks, FeedbackCount);
		for (const FGPUCollisionFeedback& Feedback : CollisionFeedbacks)
		{
			if (Feedback.BoneIndex != TargetBoneIndex) continue;
			TotalVelocity += FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z);
			TotalFeedbackCount++;
		}
	}
	return (TotalFeedbackCount > 0 && !TotalVelocity.IsNearlyZero()) ? Owner->GetActorTransform().InverseTransformVectorNoScale(TotalVelocity.GetSafeNormal()) : FVector::ZeroVector;
}

/**
 * @brief Internal helper to ensure GPU collision feedback is enabled across all modules.
 */
void UKawaiiFluidInteractionComponent::EnableGPUCollisionFeedbackIfNeeded()
{
	if (bGPUFeedbackEnabled || !TargetSubsystem) return;
	for (UKawaiiFluidSimulationModule* Module : TargetSubsystem->GetAllModules())
	{
		if (Module)
		{
			if (FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator()) { GPUSimulator->SetCollisionFeedbackEnabled(true); bGPUFeedbackEnabled = true; }
		}
	}
}

/**
 * @brief Caches bone names for fast index-to-name lookup.
 */
void UKawaiiFluidInteractionComponent::InitializeBoneNameCache()
{
	if (bBoneNameCacheInitialized) return;
	AActor* Owner = GetOwner(); if (!Owner) return;
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh || !SkelMesh->GetSkeletalMeshAsset()) return;

	const FReferenceSkeleton& RefSkeleton = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 BoneCount = RefSkeleton.GetNum();
	BoneIndexToNameCache.Empty(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i) BoneIndexToNameCache.Add(i, RefSkeleton.GetBoneName(i));
	bBoneNameCacheInitialized = true;
}

/**
 * @brief Computes smoothed per-bone fluid forces based on GPU feedback.
 * @param DeltaTime Time step
 * @param AllFeedback Array of feedback data
 * @param FeedbackCount Number of entries
 * @param ParticleRadius Simulation particle radius
 */
void UKawaiiFluidInteractionComponent::ProcessPerBoneForces(float DeltaTime, const TArray<FGPUCollisionFeedback>& AllFeedback, int32 FeedbackCount, float ParticleRadius)
{
	AActor* Owner = GetOwner(); const int32 MyOwnerID = Owner ? Owner->GetUniqueID() : 0;
	if (!bBoneNameCacheInitialized) InitializeBoneNameCache();

	TMap<int32, FVector> RawBoneForces;
	const float AreaInM2 = PI * ParticleRadius * ParticleRadius * 0.0001f;

	for (int32 i = 0; i < FeedbackCount; ++i)
	{
		const FGPUCollisionFeedback& Feedback = AllFeedback[i];
		if ((Feedback.ColliderOwnerID != 0 && Feedback.ColliderOwnerID != MyOwnerID) || Feedback.BoneIndex < 0) continue;

		float ParticleSpeed = FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z).Size() * 0.01f;
		if (ParticleSpeed < SMALL_NUMBER) continue;

		FVector ImpactForce = FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z).GetSafeNormal() * (0.5f * Feedback.Density * DragCoefficient * AreaInM2 * ParticleSpeed * ParticleSpeed);
		RawBoneForces.FindOrAdd(Feedback.BoneIndex, FVector::ZeroVector) += ImpactForce * 100.0f * PerBoneForceMultiplier;
	}

	TArray<int32> BonesToRemove;
	for (auto& Pair : SmoothedPerBoneForces)
	{
		const int32 BoneIdx = Pair.Key; FVector* RawForce = RawBoneForces.Find(BoneIdx);
		Pair.Value = FMath::VInterpTo(Pair.Value, RawForce ? *RawForce : FVector::ZeroVector, DeltaTime, PerBoneForceSmoothingSpeed);
		CurrentPerBoneForces.FindOrAdd(BoneIdx) = Pair.Value;
		if (Pair.Value.SizeSquared() < 0.01f && !RawForce) BonesToRemove.Add(BoneIdx);
	}

	for (const auto& Pair : RawBoneForces)
	{
		if (!SmoothedPerBoneForces.Contains(Pair.Key))
		{
			FVector& Smoothed = SmoothedPerBoneForces.Add(Pair.Key, FVector::ZeroVector);
			Smoothed = FMath::VInterpTo(Smoothed, Pair.Value, DeltaTime, PerBoneForceSmoothingSpeed);
			CurrentPerBoneForces.FindOrAdd(Pair.Key) = Smoothed;
		}
	}
	for (int32 BoneIdx : BonesToRemove) { SmoothedPerBoneForces.Remove(BoneIdx); CurrentPerBoneForces.Remove(BoneIdx); }
}

/**
 * @brief Returns the fluid force vector for a specific bone.
 * @param BoneIndex Target bone index
 * @return Force vector
 */
FVector UKawaiiFluidInteractionComponent::GetFluidForceForBone(int32 BoneIndex) const
{
	const FVector* Force = CurrentPerBoneForces.Find(BoneIndex);
	return Force ? *Force : FVector::ZeroVector;
}

/**
 * @brief Returns the fluid force vector for a specific bone name.
 * @param BoneName Target bone name
 * @return Force vector
 */
FVector UKawaiiFluidInteractionComponent::GetFluidForceForBoneByName(FName BoneName) const
{
	AActor* Owner = GetOwner(); if (!Owner) return FVector::ZeroVector;
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return FVector::ZeroVector;
	int32 BoneIndex = SkelMesh->GetBoneIndex(BoneName);
	return (BoneIndex == INDEX_NONE) ? FVector::ZeroVector : GetFluidForceForBone(BoneIndex);
}

/**
 * @brief Populates an array with indices of bones currently receiving fluid force.
 * @param OutBoneIndices Output array
 */
void UKawaiiFluidInteractionComponent::GetActiveBoneIndices(TArray<int32>& OutBoneIndices) const
{
	OutBoneIndices.Empty(CurrentPerBoneForces.Num());
	for (const auto& Pair : CurrentPerBoneForces) if (Pair.Value.SizeSquared() > 0.01f) OutBoneIndices.Add(Pair.Key);
}

/**
 * @brief Identifies the bone receiving the maximum fluid force.
 * @param OutBoneIndex Strongest bone index
 * @param OutForce Force vector
 * @return True if any bone has force
 */
bool UKawaiiFluidInteractionComponent::GetStrongestBoneForce(int32& OutBoneIndex, FVector& OutForce) const
{
	OutBoneIndex = -1; OutForce = FVector::ZeroVector; float MaxForceSq = 0.0f;
	for (const auto& Pair : CurrentPerBoneForces)
	{
		float ForceSq = Pair.Value.SizeSquared();
		if (ForceSq > MaxForceSq) { MaxForceSq = ForceSq; OutBoneIndex = Pair.Key; OutForce = Pair.Value; }
	}
	return OutBoneIndex >= 0;
}

/**
 * @brief Processes per-bone collision events for VFX triggering.
 * @param DeltaTime Time step
 * @param AllFeedback Feedback data
 * @param FeedbackCount Entry count
 */
void UKawaiiFluidInteractionComponent::ProcessBoneCollisionEvents(float DeltaTime, const TArray<FGPUCollisionFeedback>& AllFeedback, int32 FeedbackCount)
{
	AActor* Owner = GetOwner(); const int32 MyOwnerID = Owner ? Owner->GetUniqueID() : 0;

	TArray<int32> ExpiredCooldowns;
	for (auto& Pair : BoneEventCooldownTimers) { Pair.Value -= DeltaTime; if (Pair.Value <= 0.0f) ExpiredCooldowns.Add(Pair.Key); }
	for (int32 BoneIdx : ExpiredCooldowns) BoneEventCooldownTimers.Remove(BoneIdx);

	TMap<int32, int32> NewBoneContactCounts; TMap<int32, FVector> BoneVelocitySums; TMap<int32, int32> BoneVelocityCounts;
	TMap<int32, FVector> BoneImpactOffsetSums; TMap<int32, int32> BoneImpactOffsetCounts; TMap<int32, TMap<int32, int32>> BoneSourceCounts;

	for (int32 i = 0; i < FeedbackCount; ++i)
	{
		const FGPUCollisionFeedback& Feedback = AllFeedback[i];
		if ((Feedback.ColliderOwnerID != 0 && Feedback.ColliderOwnerID != MyOwnerID) || Feedback.BoneIndex < 0) continue;

		NewBoneContactCounts.FindOrAdd(Feedback.BoneIndex, 0)++;
		BoneVelocitySums.FindOrAdd(Feedback.BoneIndex, FVector::ZeroVector) += FVector(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z);
		BoneVelocityCounts.FindOrAdd(Feedback.BoneIndex, 0)++;
		BoneImpactOffsetSums.FindOrAdd(Feedback.BoneIndex, FVector::ZeroVector) += FVector(Feedback.ImpactOffset.X, Feedback.ImpactOffset.Y, Feedback.ImpactOffset.Z);
		BoneImpactOffsetCounts.FindOrAdd(Feedback.BoneIndex, 0)++;
		BoneSourceCounts.FindOrAdd(Feedback.BoneIndex).FindOrAdd(Feedback.ParticleSourceID, 0)++;
	}

	CurrentBoneAverageVelocities.Empty();
	for (const auto& Pair : BoneVelocitySums) CurrentBoneAverageVelocities.Add(Pair.Key, Pair.Value / FMath::Max(1, BoneVelocityCounts.FindOrAdd(Pair.Key, 1)));
	CurrentBoneContactCounts = NewBoneContactCounts;

	if (bEnableBoneCollisionEvents && OnBoneParticleCollision.IsBound())
	{
		TSet<int32> CurrentContactBones;
		for (const auto& Pair : CurrentBoneContactCounts) if (Pair.Value >= MinParticleCountForBoneEvent) CurrentContactBones.Add(Pair.Key);

		for (int32 BoneIdx : CurrentContactBones)
		{
			if (BoneEventCooldownTimers.Contains(BoneIdx)) continue;
			int32 ContactCount = CurrentBoneContactCounts.FindOrAdd(BoneIdx, 0);
			FVector AverageVelocity = CurrentBoneAverageVelocities.FindOrAdd(BoneIdx, FVector::ZeroVector);
			FVector ImpactOffsetAverage = (BoneImpactOffsetCounts.FindOrAdd(BoneIdx, 0) > 0) ? (*BoneImpactOffsetSums.Find(BoneIdx) / *BoneImpactOffsetCounts.Find(BoneIdx)) : FVector::ZeroVector;
			FName BoneName = GetBoneNameFromIndex(BoneIdx); FName FluidName = NAME_None;

			if (TargetSubsystem)
			{
				const TMap<int32, int32>* SCounts = BoneSourceCounts.Find(BoneIdx);
				if (SCounts)
				{
					int32 MaxSID = -1; int32 MaxC = 0;
					for (const auto& SPair : *SCounts) if (SPair.Value > MaxC) { MaxC = SPair.Value; MaxSID = SPair.Key; }
					if (MaxSID >= 0) if (UKawaiiFluidPresetDataAsset* P = TargetSubsystem->GetPresetBySourceID(MaxSID)) FluidName = P->GetFluidName();
				}
			}
			OnBoneParticleCollision.Broadcast(BoneIdx, BoneName, ContactCount, AverageVelocity, FluidName, ImpactOffsetAverage);
			BoneEventCooldownTimers.Add(BoneIdx, BoneEventCooldown);
		}
		PreviousContactBones = CurrentContactBones;
	}
}

/**
 * @brief Returns the number of particles currently in contact with a specific bone.
 * @param BoneIndex Target bone index
 * @return Particle count
 */
int32 UKawaiiFluidInteractionComponent::GetBoneContactCount(int32 BoneIndex) const
{
	const int32* Count = CurrentBoneContactCounts.Find(BoneIndex); return Count ? *Count : 0;
}

/**
 * @brief Populates an array with indices of bones currently in contact with fluid.
 * @param OutBoneIndices Output array
 */
void UKawaiiFluidInteractionComponent::GetBonesWithContacts(TArray<int32>& OutBoneIndices) const
{
	OutBoneIndices.Empty(CurrentBoneContactCounts.Num());
	for (const auto& Pair : CurrentBoneContactCounts) if (Pair.Value > 0) OutBoneIndices.Add(Pair.Key);
}

/**
 * @brief Returns the name of a bone based on its index.
 * @param BoneIndex Target bone index
 * @return Bone name
 */
FName UKawaiiFluidInteractionComponent::GetBoneNameFromIndex(int32 BoneIndex) const
{
	const FName* CachedName = BoneIndexToNameCache.Find(BoneIndex); if (CachedName) return *CachedName;
	AActor* Owner = GetOwner(); if (!Owner) return NAME_None;
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh || !SkelMesh->GetSkeletalMeshAsset()) return NAME_None;
	const FReferenceSkeleton& RefSkeleton = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	return (BoneIndex >= 0 && BoneIndex < RefSkeleton.GetNum()) ? RefSkeleton.GetBoneName(BoneIndex) : NAME_None;
}

/**
 * @brief Returns the owner's SkeletalMeshComponent.
 * @return USkeletalMeshComponent pointer
 */
USkeletalMeshComponent* UKawaiiFluidInteractionComponent::GetOwnerSkeletalMesh() const
{
	AActor* Owner = GetOwner(); return Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

/**
 * @brief Identifies the bone receiving the most particle contacts.
 * @param OutBoneIndex Most contacted bone index
 * @param OutContactCount Maximum contact count
 * @return True if any bone is contacted
 */
bool UKawaiiFluidInteractionComponent::GetMostContactedBone(int32& OutBoneIndex, int32& OutContactCount) const
{
	OutBoneIndex = -1; OutContactCount = 0;
	for (const auto& Pair : CurrentBoneContactCounts) if (Pair.Value > OutContactCount) { OutContactCount = Pair.Value; OutBoneIndex = Pair.Key; }
	return OutBoneIndex >= 0;
}

/**
 * @brief Samples a triangular surface for boundary particle generation.
 * @param V0 Vertex 0
 * @param V1 Vertex 1
 * @param V2 Vertex 2
 * @param Spacing Desired particle spacing
 * @param OutPoints Output array of sampled points
 */
void UKawaiiFluidInteractionComponent::SampleTriangleSurface(const FVector& V0, const FVector& V1, const FVector& V2, float Spacing, TArray<FVector>& OutPoints)
{
	FVector E1 = V1 - V0, E2 = V2 - V0; float L1 = E1.Size(), L2 = E2.Size();
	if (L1 < SMALL_NUMBER || L2 < SMALL_NUMBER) return;
	float Area = FVector::CrossProduct(E1, E2).Size() * 0.5f;
	if (Area < Spacing * Spacing * 0.1f) { OutPoints.Add((V0 + V1 + V2) / 3.0f); return; }
	int32 NSU = FMath::Max(1, FMath::CeilToInt(L1 / Spacing)), NSV = FMath::Max(1, FMath::CeilToInt(L2 / Spacing));
	for (int32 i = 0; i <= NSU; ++i)
	{
		float u = static_cast<float>(i) / NSU;
		for (int32 j = 0; j <= NSV; ++j) { float v = static_cast<float>(j) / NSV; if (u + v <= 1.0f) OutPoints.Add(V0 + E1 * u + E2 * v); }
	}
}

/**
 * @brief Generates boundary particles based on the owner's collision geometry.
 */
void UKawaiiFluidInteractionComponent::GenerateBoundaryParticles()
{
	AActor* Owner = GetOwner(); if (!Owner) return;
	BoundaryParticleLocalPositions.Empty(); BoundaryParticleBoneIndices.Empty(); BoundaryParticlePositions.Empty();
	BoundaryParticleVertexIndices.Empty(); BoundaryParticleLocalNormals.Empty(); bIsSkeletalMesh = false;

	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (SkelMesh && SkelMesh->GetSkeletalMeshAsset())
	{
		bIsSkeletalMesh = true; USkeletalMesh* SkeletalMesh = SkelMesh->GetSkeletalMeshAsset();
		UPhysicsAsset* PhysAsset = SkeletalMesh->GetPhysicsAsset();
		if (!PhysAsset) return;
		const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
		for (USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
		{
			if (!BodySetup) continue;
			int32 BoneIndex = RefSkeleton.FindBoneIndex(BodySetup->BoneName);
			if (BoneIndex != INDEX_NONE) SampleAggGeomSurfaces(BodySetup->AggGeom, BoneIndex);
		}
	}
	else
	{
		UStaticMeshComponent* StaticMeshComp = Owner->FindComponentByClass<UStaticMeshComponent>();
		if (StaticMeshComp && StaticMeshComp->GetStaticMesh())
		{
			UBodySetup* BodySetup = StaticMeshComp->GetStaticMesh()->GetBodySetup();
			if (BodySetup) SampleAggGeomSurfaces(BodySetup->AggGeom, -1);
		}
	}

	BoundaryParticlePositions.SetNum(BoundaryParticleLocalPositions.Num());
	BoundaryParticleNormals.SetNum(BoundaryParticleLocalNormals.Num());
	bBoundaryParticlesInitialized = BoundaryParticleLocalPositions.Num() > 0;
	if (bBoundaryParticlesInitialized) UpdateBoundaryParticlePositions();
}

/**
 * @brief Updates boundary particle world-space positions based on current bone/actor transforms.
 */
void UKawaiiFluidInteractionComponent::UpdateBoundaryParticlePositions()
{
	AActor* Owner = GetOwner(); if (!Owner || BoundaryParticlePositions.Num() == 0) return;
	const int32 NumParticles = BoundaryParticlePositions.Num();
	const bool bHasNormals = (BoundaryParticleLocalNormals.Num() == NumParticles);

	if (bIsSkeletalMesh)
	{
		USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (SkelMesh && BoundaryParticleLocalPositions.Num() == NumParticles)
		{
			for (int32 i = 0; i < NumParticles; ++i)
			{
				int32 BoneIdx = BoundaryParticleBoneIndices[i];
				FTransform BT = (BoneIdx >= 0) ? SkelMesh->GetBoneTransform(BoneIdx) : SkelMesh->GetComponentTransform();
				BoundaryParticlePositions[i] = BT.TransformPosition(BoundaryParticleLocalPositions[i]);
				if (bHasNormals) BoundaryParticleNormals[i] = BT.TransformVectorNoScale(BoundaryParticleLocalNormals[i]);
			}
		}
	}
	else
	{
		USceneComponent* RootComp = Owner->GetRootComponent();
		if (RootComp && BoundaryParticleLocalPositions.Num() == NumParticles)
		{
			FTransform CT = RootComp->GetComponentTransform();
			for (int32 i = 0; i < NumParticles; ++i)
			{
				BoundaryParticlePositions[i] = CT.TransformPosition(BoundaryParticleLocalPositions[i]);
				if (bHasNormals) BoundaryParticleNormals[i] = CT.TransformVectorNoScale(BoundaryParticleLocalNormals[i]);
			}
		}
	}
}

/**
 * @brief Renders debug visualization for boundary particles and their normals.
 */
void UKawaiiFluidInteractionComponent::DrawDebugBoundaryParticles()
{
	UWorld* World = GetWorld(); if (!World || BoundaryParticlePositions.Num() == 0) return;
	const int32 NumParticles = BoundaryParticlePositions.Num();
	const bool bHasNormals = (BoundaryParticleNormals.Num() == NumParticles);

	for (int32 i = 0; i < NumParticles; ++i)
	{
		DrawDebugPoint(World, BoundaryParticlePositions[i], BoundaryParticleDebugSize, BoundaryParticleDebugColor, false, -1.0f, SDPG_Foreground);
		if (bShowBoundaryNormals && bHasNormals) DrawDebugDirectionalArrow(World, BoundaryParticlePositions[i], BoundaryParticlePositions[i] + BoundaryParticleNormals[i] * BoundaryNormalLength, 3.0f, FColor::Yellow, false, -1.0f, SDPG_Foreground, 1.0f);
	}
}

/**
 * @brief Regenerates the boundary particle system manually.
 */
void UKawaiiFluidInteractionComponent::RegenerateBoundaryParticles()
{
	bBoundaryParticlesInitialized = false; bIsSkeletalMesh = false;
	BoundaryParticleLocalPositions.Empty(); BoundaryParticleBoneIndices.Empty(); BoundaryParticlePositions.Empty();
	BoundaryParticleVertexIndices.Empty(); BoundaryParticleNormals.Empty(); BoundaryParticleLocalNormals.Empty();
	if (bEnableBoundaryParticles) GenerateBoundaryParticles();
}

/**
 * @brief Collects boundary particle data for CPU-side GPU buffer updates.
 * @param O Output boundary particle data structure
 */
void UKawaiiFluidInteractionComponent::CollectGPUBoundaryParticles(FGPUBoundaryParticles& O) const
{
	if (!bBoundaryParticlesInitialized || BoundaryParticlePositions.Num() == 0) return;
	const int32 N = BoundaryParticlePositions.Num(); O.Particles.Reserve(O.Particles.Num() + N);
	for (int32 i = 0; i < N; ++i)
	{
		FGPUBoundaryParticle G;
		G.Position = FVector3f(BoundaryParticlePositions[i]);
		G.Normal = FVector3f(BoundaryParticleNormals[i]);
		G.OwnerID = GetUniqueID();
		O.Particles.Add(G);
	}
}

/**
 * @brief Collects local boundary particle data for GPU skinning.
 * @param O Output local boundary particle array
 * @param P Volume contribution (Psi)
 * @param F Friction coefficient
 */
void UKawaiiFluidInteractionComponent::CollectLocalBoundaryParticles(TArray<FGPUBoundaryParticleLocal>& O, float P, float F) const
{
	if (!bBoundaryParticlesInitialized) return;
	const int32 N = BoundaryParticleLocalPositions.Num();
	for (int32 i = 0; i < N; ++i)
	{
		FGPUBoundaryParticleLocal L;
		L.LocalPosition = FVector3f(BoundaryParticleLocalPositions[i]);
		L.LocalNormal = FVector3f(BoundaryParticleLocalNormals[i]);
		L.BoneIndex = BoundaryParticleBoneIndices[i];
		L.Psi = P;
		O.Add(L);
	}
}

/**
 * @brief Collects bone and component transforms for GPU boundary skinning.
 * @param BT Output array of bone matrices
 * @param CT Output component transform matrix
 */
void UKawaiiFluidInteractionComponent::CollectBoneTransformsForBoundary(TArray<FMatrix>& BT, FMatrix& CT) const
{
	BT.Empty(); CT = FMatrix::Identity; AActor* Owner = GetOwner(); if (!Owner) return;
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (SkelMesh) { const TArray<FTransform>& BTs = SkelMesh->GetComponentSpaceTransforms(); BT.Reserve(BTs.Num()); for (const FTransform& T : BTs) BT.Add(T.ToMatrixWithScale()); CT = SkelMesh->GetComponentToWorld().ToMatrixWithScale(); }
	else if (USceneComponent* RC = Owner->GetRootComponent()) CT = RC->GetComponentToWorld().ToMatrixWithScale();
}

/**
 * @brief Identifies the primary physics body for force application.
 * @return UPrimitiveComponent pointer
 */
UPrimitiveComponent* UKawaiiFluidInteractionComponent::FindPhysicsBody() const
{
	AActor* Owner = GetOwner(); if (!Owner) return nullptr;
	UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	return (Root && Root->IsSimulatingPhysics()) ? Root : Owner->FindComponentByClass<UPrimitiveComponent>();
}

/**
 * @brief Estimates submerged volume based on particle contacts.
 * @param CC Contact count
 * @param PR Particle radius
 * @return Submerged volume in cm³
 */
float UKawaiiFluidInteractionComponent::CalculateSubmergedVolumeFromContacts(int32 CC, float PR) const
{
	return CC * (4.0f / 3.0f) * PI * FMath::Pow(PR, 3.0f) * 1.25f;
}

/**
 * @brief Calculates buoyancy force magnitude and direction.
 * @param SV Submerged volume
 * @param FD Fluid density (kg/m³)
 * @param G Gravity vector
 * @return Buoyancy force vector
 */
FVector UKawaiiFluidInteractionComponent::CalculateBuoyancyForce(float SV, float FD, const FVector& G) const
{
	return -G * (SV * 0.000001f) * FD * 1.0f * BuoyancyMultiplier;
}

/**
 * @brief Returns the current fluid density from the target subsystem.
 * @return Density in kg/m³
 */
float UKawaiiFluidInteractionComponent::GetCurrentFluidDensity() const
{
	if (TargetSubsystem) for (auto M : TargetSubsystem->GetAllModules()) if (M && M->GetPreset()) return M->GetPreset()->Density;
	return 1000.0f;
}

/**
 * @brief Returns the particle radius used in the simulation.
 * @return Radius in cm
 */
float UKawaiiFluidInteractionComponent::GetCurrentParticleRadius() const
{
	if (TargetSubsystem) for (auto M : TargetSubsystem->GetAllModules()) if (M) return M->GetParticleRadius();
	return 5.0f;
}

/**
 * @brief Returns the current world gravity vector.
 * @return Gravity in cm/s²
 */
FVector UKawaiiFluidInteractionComponent::GetCurrentGravity() const
{
	if (TargetSubsystem) for (auto M : TargetSubsystem->GetAllModules()) if (M && M->GetPreset()) return M->GetPreset()->Gravity;
	return FVector(0, 0, -980.0f);
}

/**
 * @brief Applies automatic buoyancy and drag forces to physics bodies.
 * @param DT Time step
 */
void UKawaiiFluidInteractionComponent::ApplyAutoPhysicsForces(float DT)
{
	UPrimitiveComponent* PB = FindPhysicsBody(); if (!PB || !PB->IsSimulatingPhysics()) return;
	float SV = 0.0f;
	if (SubmergedVolumeMethod == ESubmergedVolumeMethod::ContactBased) SV = CalculateSubmergedVolumeFromContacts(CurrentContactCount, GetCurrentParticleRadius());
	else SV = PB->Bounds.GetBox().GetSize().X * PB->Bounds.GetBox().GetSize().Y * PB->Bounds.GetBox().GetSize().Z * FixedSubmersionRatio;
	EstimatedSubmergedVolume = SV;

	if (bApplyBuoyancy && SV > 0.0f)
	{
		FVector G = GetCurrentGravity();
		FVector BF = CalculateBuoyancyForce(SV, GetCurrentFluidDensity(), G);
		FVector V = PB->GetPhysicsLinearVelocity();
		BF -= V * BuoyancyDamping * (SV / FMath::Max(1.0f, PB->Bounds.GetBox().GetSize().Size()));
		CurrentBuoyancyForce = BF;
		PB->AddForce(BF, NAME_None, false);
		if (EstimatedBuoyancyCenterOffset.Size() > 1.0f) PB->AddForceAtLocation(BF * 0.1f, PB->GetComponentLocation() + EstimatedBuoyancyCenterOffset);
	}

	if (bApplyDrag)
	{
		FVector DF = CurrentFluidForce * PhysicsDragMultiplier;
		if (!DF.IsNearlyZero()) PB->AddForce(DF, NAME_None, false);
		FVector RV = (CurrentFluidForce.GetSafeNormal() * GetFluidImpactSpeed()) - PB->GetPhysicsLinearVelocity();
		PB->AddForce(RV * FluidLinearDamping * (SV / 1000.0f), NAME_None, false);
		PB->SetAngularDamping(FMath::Max(PB->GetAngularDamping(), FluidAngularDamping));
	}
}

/**
 * @brief Samples surfaces of aggregate geometry for boundary particles.
 * @param AggGeom Aggregate geometry
 * @param BoneIdx Associated bone index
 */
void UKawaiiFluidInteractionComponent::SampleAggGeomSurfaces(const FKAggregateGeom& AggGeom, int32 BoneIdx)
{
	for (const FKSphereElem& S : AggGeom.SphereElems) SampleSphereSurface(S, BoneIdx, FTransform(S.Center));
	for (const FKSphylElem& C : AggGeom.SphylElems) SampleCapsuleSurface(C, BoneIdx);
	for (const FKBoxElem& B : AggGeom.BoxElems) SampleBoxSurface(B, BoneIdx);
	for (const FKConvexElem& C : AggGeom.ConvexElems) SampleConvexSurface(C, BoneIdx);
}

/**
 * @brief Samples a sphere surface.
 */
void UKawaiiFluidInteractionComponent::SampleSphereSurface(const FKSphereElem& S, int32 BoneIdx, const FTransform& LT)
{
	float R = S.Radius; int32 NS = FMath::Max(8, FMath::CeilToInt(4.0f * PI * R * R / (BoundaryParticleSpacing * BoundaryParticleSpacing)));
	for (int32 i = 0; i < NS; ++i)
	{
		float Phi = acos(1.0f - 2.0f * (i + 0.5f) / NS), Theta = PI * (1.0f + pow(5.0f, 0.5f)) * (i + 0.5f);
		FVector LP = FVector(sin(Phi) * cos(Theta), sin(Phi) * sin(Theta), cos(Phi)) * R;
		BoundaryParticleLocalPositions.Add(LT.TransformPosition(LP));
		BoundaryParticleLocalNormals.Add(LT.TransformVectorNoScale(LP.GetSafeNormal()));
		BoundaryParticleBoneIndices.Add(BoneIdx);
	}
}

/**
 * @brief Samples a capsule surface.
 */
void UKawaiiFluidInteractionComponent::SampleCapsuleSurface(const FKSphylElem& C, int32 BoneIdx)
{
	float R = C.Radius, HH = C.Length * 0.5f; FTransform T = C.GetTransform();
	int32 NSZ = FMath::Max(1, FMath::CeilToInt(C.Length / BoundaryParticleSpacing)), NSC = FMath::Max(4, FMath::CeilToInt(2.0f * PI * R / BoundaryParticleSpacing));
	for (int32 i = 0; i <= NSZ; ++i)
	{
		float z = -HH + (i * C.Length / NSZ);
		for (int32 j = 0; j < NSC; ++j)
		{
			float A = 2.0f * PI * j / NSC; FVector LP(R * cos(A), R * sin(A), z);
			BoundaryParticleLocalPositions.Add(T.TransformPosition(LP));
			BoundaryParticleLocalNormals.Add(T.TransformVectorNoScale(FVector(cos(A), sin(A), 0)));
			BoundaryParticleBoneIndices.Add(BoneIdx);
		}
	}
	SampleHemisphere(T, R, HH, 1, BoneIdx, NSC * 2); SampleHemisphere(T, R, -HH, -1, BoneIdx, NSC * 2);
}

/**
 * @brief Samples a box surface.
 */
void UKawaiiFluidInteractionComponent::SampleBoxSurface(const FKBoxElem& B, int32 BoneIdx)
{
	FVector HS(B.X * 0.5f, B.Y * 0.5f, B.Z * 0.5f); FTransform T = B.GetTransform();
	auto SF = [&](FVector N, FVector U, FVector V, float UL, float VL)
	{
		int32 NU = FMath::Max(1, FMath::CeilToInt(UL / BoundaryParticleSpacing)), NV = FMath::Max(1, FMath::CeilToInt(VL / BoundaryParticleSpacing));
		for (int32 i = 0; i <= NU; ++i) for (int32 j = 0; j <= NV; ++j)
		{
			FVector LP = N + U * (-0.5f + (float)i / NU) * UL + V * (-0.5f + (float)j / NV) * VL;
			BoundaryParticleLocalPositions.Add(T.TransformPosition(LP)); BoundaryParticleLocalNormals.Add(T.TransformVectorNoScale(N.GetSafeNormal()));
			BoundaryParticleBoneIndices.Add(BoneIdx);
		}
	};
	SF(FVector(HS.X, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), HS.Y * 2, HS.Z * 2); SF(FVector(-HS.X, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), HS.Y * 2, HS.Z * 2);
	SF(FVector(0, HS.Y, 0), FVector(1, 0, 0), FVector(0, 0, 1), HS.X * 2, HS.Z * 2); SF(FVector(0, -HS.Y, 0), FVector(1, 0, 0), FVector(0, 0, 1), HS.X * 2, HS.Z * 2);
	SF(FVector(0, 0, HS.Z), FVector(1, 0, 0), FVector(0, 1, 0), HS.X * 2, HS.Y * 2); SF(FVector(0, 0, -HS.Z), FVector(1, 0, 0), FVector(0, 1, 0), HS.X * 2, HS.Y * 2);
}

/**
 * @brief Samples a convex surface.
 */
void UKawaiiFluidInteractionComponent::SampleConvexSurface(const FKConvexElem& C, int32 BoneIdx)
{
	FTransform T = C.GetTransform();
	for (int32 i = 0; i < C.IndexData.Num(); i += 3) SampleTriangleSurface(T.TransformPosition(C.VertexData[C.IndexData[i]]), T.TransformPosition(C.VertexData[C.IndexData[i + 1]]), T.TransformPosition(C.VertexData[C.IndexData[i + 2]]), BoundaryParticleSpacing, BoundaryParticleLocalPositions);
	for (int32 i = 0; i < BoundaryParticleLocalPositions.Num(); ++i) { BoundaryParticleBoneIndices.Add(BoneIdx); BoundaryParticleLocalNormals.Add(FVector::UpVector); }
}

/**
 * @brief Samples a hemispherical surface for capsule caps.
 */
void UKawaiiFluidInteractionComponent::SampleHemisphere(const FTransform& T, float R, float ZO, int32 ZD, int32 BoneIdx, int32 NS)
{
	for (int32 i = 0; i < NS / 2; ++i)
	{
		float Phi = acos((float)i / (NS / 2)), Theta = PI * (1.0f + pow(5.0f, 0.5f)) * i;
		FVector LP = FVector(sin(Phi) * cos(Theta), sin(Phi) * sin(Theta), cos(Phi) * ZD) * R + FVector(0, 0, ZO);
		BoundaryParticleLocalPositions.Add(T.TransformPosition(LP)); BoundaryParticleLocalNormals.Add(T.TransformVectorNoScale(FVector(LP.X, LP.Y, (LP.Z - ZO)).GetSafeNormal()));
		BoundaryParticleBoneIndices.Add(BoneIdx);
	}
}