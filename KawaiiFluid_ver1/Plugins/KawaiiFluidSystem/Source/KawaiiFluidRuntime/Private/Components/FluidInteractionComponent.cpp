// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/FluidInteractionComponent.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Components/KawaiiFluidSimulationComponent.h"
#include "Collision/MeshFluidCollider.h"

UFluidInteractionComponent::UFluidInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	TargetSubsystem = nullptr;
	bCanAttachFluid = true;
	AdhesionMultiplier = 1.0f;
	DragAlongStrength = 0.5f;
	bAutoCreateCollider = true;

	AttachedParticleCount = 0;
	bIsWet = false;

	AutoCollider = nullptr;
}

void UFluidInteractionComponent::BeginPlay()
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
}

void UFluidInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSimulator();

	Super::EndPlay(EndPlayReason);
}

void UFluidInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!TargetSubsystem)
	{
		return;
	}

	// 기존: 붙은 파티클 추적
	int32 PrevCount = AttachedParticleCount;
	UpdateAttachedParticleCount();

	if (AttachedParticleCount > 0 && PrevCount == 0)
	{
		bIsWet = true;
		OnFluidAttached.Broadcast(AttachedParticleCount);
	}
	else if (AttachedParticleCount == 0 && PrevCount > 0)
	{
		bIsWet = false;
		OnFluidDetached.Broadcast();
	}

	// 새로운: Collider 충돌 감지
	if (bEnableCollisionDetection && AutoCollider)
	{
		DetectCollidingParticles();
		
		// 트리거 이벤트 발생 조건
		bool bIsColliding = (CollidingParticleCount >= MinParticleCountForTrigger);
		
		// Enter 이벤트
		if (bIsColliding && !bWasColliding)
		{
			if (OnFluidColliding.IsBound())
			{
				OnFluidColliding.Broadcast(CollidingParticleCount);
			}
		}
		// Exit 이벤트
		else if (!bIsColliding && bWasColliding)
		{
			if (OnFluidStopColliding.IsBound())
			{
				OnFluidStopColliding.Broadcast();
			}
		}
		
		bWasColliding = bIsColliding;
	}

	// 본 레벨 추적은 FluidSimulator::UpdateAttachedParticlePositions()에서 처리
}

void UFluidInteractionComponent::CreateAutoCollider()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	AutoCollider = NewObject<UMeshFluidCollider>(Owner);
	if (AutoCollider)
	{
		AutoCollider->RegisterComponent();
		AutoCollider->bAllowAdhesion = bCanAttachFluid;
		AutoCollider->AdhesionMultiplier = AdhesionMultiplier;
	}
}

void UFluidInteractionComponent::RegisterWithSimulator()
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

void UFluidInteractionComponent::UnregisterFromSimulator()
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

void UFluidInteractionComponent::UpdateAttachedParticleCount()
{
	AActor* Owner = GetOwner();
	int32 Count = 0;

	if (TargetSubsystem)
	{
		// Iterate all components from subsystem
		for (UKawaiiFluidSimulationComponent* Comp : TargetSubsystem->GetAllComponents())
		{
			if (!Comp) continue;
			for (const FFluidParticle& Particle : Comp->GetParticles())
			{
				if (Particle.bIsAttached && Particle.AttachedActor.Get() == Owner)
				{
					++Count;
				}
			}
		}
	}

	AttachedParticleCount = Count;
}

void UFluidInteractionComponent::DetachAllFluid()
{
	AActor* Owner = GetOwner();

	auto DetachFromParticles = [Owner](TArray<FFluidParticle>& Particles)
	{
		for (FFluidParticle& Particle : Particles)
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
		for (UKawaiiFluidSimulationComponent* Comp : TargetSubsystem->GetAllComponents())
		{
			if (!Comp) continue;
			DetachFromParticles(Comp->GetParticlesMutable());
		}
	}

	AttachedParticleCount = 0;
	bIsWet = false;
}

void UFluidInteractionComponent::PushFluid(FVector Direction, float Force)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector NormalizedDir = Direction.GetSafeNormal();
	FVector OwnerLocation = Owner->GetActorLocation();

	auto PushParticles = [Owner, NormalizedDir, OwnerLocation, Force](TArray<FFluidParticle>& Particles)
	{
		for (FFluidParticle& Particle : Particles)
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
		for (UKawaiiFluidSimulationComponent* Comp : TargetSubsystem->GetAllComponents())
		{
			if (!Comp) continue;
			PushParticles(Comp->GetParticlesMutable());
		}
	}
}

void UFluidInteractionComponent::DetectCollidingParticles()
{
	if (!AutoCollider)
	{
		CollidingParticleCount = 0;
		return;
	}

	AActor* Owner = GetOwner();
	int32 Count = 0;

	auto CountColliding = [this, Owner, &Count](const TArray<FFluidParticle>& Particles)
	{
		for (const FFluidParticle& Particle : Particles)
		{
			// 1. 이미 붙어있으면 충돌 중
			if (Particle.bIsAttached && Particle.AttachedActor.Get() == Owner)
			{
				++Count;
				continue;
			}

			// 2. Collider 안에 있는지 체크 (물리 기반, 정확!)
			if (AutoCollider->IsPointInside(Particle.Position))
			{
				++Count;
			}
		}
	};

	if (TargetSubsystem)
	{
		for (UKawaiiFluidSimulationComponent* Comp : TargetSubsystem->GetAllComponents())
		{
			if (!Comp) continue;
			CountColliding(Comp->GetParticles());
		}
	}

	CollidingParticleCount = Count;
}
