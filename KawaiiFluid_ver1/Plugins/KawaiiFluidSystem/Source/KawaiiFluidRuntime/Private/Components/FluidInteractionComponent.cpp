// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/FluidInteractionComponent.h"
#include "Core/FluidSimulator.h"
#include "Collision/MeshFluidCollider.h"
#include "Kismet/GameplayStatics.h"

UFluidInteractionComponent::UFluidInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	TargetSimulator = nullptr;
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

	if (!TargetSimulator)
	{
		AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), AFluidSimulator::StaticClass());
		if (FoundActor)
		{
			TargetSimulator = Cast<AFluidSimulator>(FoundActor);
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

	if (!TargetSimulator)
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
	if (TargetSimulator)
	{
		if (AutoCollider)
		{
			TargetSimulator->RegisterCollider(AutoCollider);
		}
		// 본 레벨 추적을 위해 자신도 등록
		TargetSimulator->RegisterInteractionComponent(this);
	}
}

void UFluidInteractionComponent::UnregisterFromSimulator()
{
	if (TargetSimulator)
	{
		if (AutoCollider)
		{
			TargetSimulator->UnregisterCollider(AutoCollider);
		}
		TargetSimulator->UnregisterInteractionComponent(this);
	}
}

void UFluidInteractionComponent::UpdateAttachedParticleCount()
{
	if (!TargetSimulator)
	{
		AttachedParticleCount = 0;
		return;
	}

	AActor* Owner = GetOwner();
	int32 Count = 0;

	const TArray<FFluidParticle>& Particles = TargetSimulator->GetParticles();
	for (const FFluidParticle& Particle : Particles)
	{
		if (Particle.bIsAttached && Particle.AttachedActor.Get() == Owner)
		{
			++Count;
		}
	}

	AttachedParticleCount = Count;
}

void UFluidInteractionComponent::DetachAllFluid()
{
	if (!TargetSimulator)
	{
		return;
	}

	AActor* Owner = GetOwner();

	TArray<FFluidParticle>& Particles = TargetSimulator->GetParticlesMutable();
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

	AttachedParticleCount = 0;
	bIsWet = false;
}

void UFluidInteractionComponent::PushFluid(FVector Direction, float Force)
{
	if (!TargetSimulator)
	{
		return;
	}

	AActor* Owner = GetOwner();
	FVector NormalizedDir = Direction.GetSafeNormal();

	TArray<FFluidParticle>& Particles = TargetSimulator->GetParticlesMutable();
	for (FFluidParticle& Particle : Particles)
	{
		float Distance = FVector::Dist(Particle.Position, Owner->GetActorLocation());

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
}

void UFluidInteractionComponent::SetTargetSimulator(AFluidSimulator* Simulator)
{
	UnregisterFromSimulator();
	TargetSimulator = Simulator;
	RegisterWithSimulator();
}

void UFluidInteractionComponent::DetectCollidingParticles()
{
	if (!AutoCollider || !TargetSimulator)
	{
		CollidingParticleCount = 0;
		return;
	}

	AActor* Owner = GetOwner();
	int32 Count = 0;

	const TArray<FFluidParticle>& Particles = TargetSimulator->GetParticles();
	
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

	CollidingParticleCount = Count;
}
