// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/FluidInteractionComponent.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Core/SpatialHash.h"
#include "Collision/MeshFluidCollider.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GPU/GPUFluidSimulator.h"
#include "GPU/GPUFluidParticle.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/PositionVertexBuffer.h"

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

void UFluidInteractionComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// 에디터 모드에서 Subsystem에 등록 (브러시 모드용)
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

void UFluidInteractionComponent::OnUnregister()
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

	// 경계 입자 생성 (Flex-style Adhesion)
	if (bEnableBoundaryParticles)
	{
		GenerateBoundaryParticles();
	}
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

	// Per-Polygon Collision AABB 디버그 시각화
	if (bUsePerPolygonCollision && bDrawPerPolygonAABB)
	{
		FBox AABB = GetPerPolygonFilterAABB();
		if (AABB.IsValid)
		{
			DrawDebugBox(
				GetWorld(),
				AABB.GetCenter(),
				AABB.GetExtent(),
				FColor::Cyan,
				false,  // bPersistentLines
				-1.0f,  // LifeTime (매 프레임 갱신)
				0,      // DepthPriority
				2.0f    // Thickness
			);
		}
	}

	// GPU Collision Feedback 처리 (Particle -> Player Interaction)
	if (bEnableForceFeedback)
	{
		// GPU 피드백 자동 활성화 (첫 틱에서)
		EnableGPUCollisionFeedbackIfNeeded();

		ProcessCollisionFeedback(DeltaTime);
	}

	// 경계 입자 업데이트 및 디버그 표시 (Flex-style Adhesion)
	if (bEnableBoundaryParticles && bBoundaryParticlesInitialized)
	{
		UpdateBoundaryParticlePositions();

		if (bShowBoundaryParticles)
		{
			DrawDebugBoundaryParticles();
		}
	}
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

		// TargetMeshComponent 자동 설정
		// 우선순위: SkeletalMeshComponent > CapsuleComponent > StaticMeshComponent
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
		// Iterate all Modules from subsystem
		for (auto* Module : TargetSubsystem->GetAllModules())
		{
			if (!Module) continue;
			for (const FFluidParticle& Particle : Module->GetParticles())
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
		for (auto Module : TargetSubsystem->GetAllModules())
		{
			if (!Module) continue;
			DetachFromParticles(Module->GetParticlesMutable());
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
		for (auto Module : TargetSubsystem->GetAllModules())
		{
			if (!Module) continue;
			PushParticles(Module->GetParticlesMutable());
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

	// 캐시 갱신
	AutoCollider->CacheCollisionShapes();
	if (!AutoCollider->IsCacheValid())
	{
		CollidingParticleCount = 0;
		return;
	}

	AActor* Owner = GetOwner();
	int32 Count = 0;
	FBox ColliderBounds = AutoCollider->GetCachedBounds();

	if (TargetSubsystem)
	{
		TArray<int32> CandidateIndices;

		for (auto* Module : TargetSubsystem->GetAllModules())
		{
			if (!Module) continue;

			FSpatialHash* SpatialHash = Module->GetSpatialHash();
			const TArray<FFluidParticle>& Particles = Module->GetParticles();

			if (SpatialHash)
			{
				// SpatialHash로 바운딩 박스 내 파티클만 쿼리
				SpatialHash->QueryBox(ColliderBounds, CandidateIndices);

				for (int32 Idx : CandidateIndices)
				{
					if (Idx < 0 || Idx >= Particles.Num()) continue;

					const FFluidParticle& Particle = Particles[Idx];

					// 1. 이미 붙어있으면 충돌 중
					if (Particle.bIsAttached && Particle.AttachedActor.Get() == Owner)
					{
						++Count;
						continue;
					}

					// 2. 정밀 체크 (후보만)
					if (AutoCollider->IsPointInside(Particle.Position))
					{
						++Count;
					}
				}
			}
			else
			{
				// SpatialHash 없으면 기존 방식 (폴백)
				for (const FFluidParticle& Particle : Particles)
				{
					if (Particle.bIsAttached && Particle.AttachedActor.Get() == Owner)
					{
						++Count;
						continue;
					}

					if (AutoCollider->IsPointInside(Particle.Position))
					{
						++Count;
					}
				}
			}
		}
	}

	CollidingParticleCount = Count;
}

FBox UFluidInteractionComponent::GetPerPolygonFilterAABB() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FBox(ForceInit);
	}

	FBox ActorBounds(ForceInit);

	// SkeletalMeshComponent가 있으면 그 바운딩 박스 사용 (더 정확함)
	if (USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		ActorBounds = SkelMesh->Bounds.GetBox();
	}
	else
	{
		// 없으면 Actor 전체 바운딩 박스 사용
		ActorBounds = Owner->GetComponentsBoundingBox(true);
	}

	// 패딩 적용
	if (PerPolygonAABBPadding > 0.0f && ActorBounds.IsValid)
	{
		ActorBounds = ActorBounds.ExpandBy(PerPolygonAABBPadding);
	}

	return ActorBounds;
}

//=============================================================================
// GPU Collision Feedback Implementation (Particle -> Player Interaction)
//=============================================================================

void UFluidInteractionComponent::ProcessCollisionFeedback(float DeltaTime)
{
	AActor* Owner = GetOwner();
	const int32 MyOwnerID = Owner ? Owner->GetUniqueID() : 0;

	if (!TargetSubsystem)
	{
		// 접촉 없음 → 힘 감쇠
		SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed);
		CurrentFluidForce = SmoothedForce;
		CurrentContactCount = 0;
		CurrentAveragePressure = 0.0f;
		return;
	}

	// GPUSimulator에서 피드백 가져오기 (첫 번째 GPU 모드 모듈에서)
	FGPUFluidSimulator* GPUSimulator = nullptr;
	for (auto* Module : TargetSubsystem->GetAllModules())
	{
		if (Module && Module->GetGPUSimulator())
		{
			GPUSimulator = Module->GetGPUSimulator();
			break;
		}
	}

	if (!GPUSimulator)
	{
		// GPUSimulator 없음 → 힘 감쇠
		SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed);
		CurrentFluidForce = SmoothedForce;
		CurrentContactCount = 0;
		CurrentAveragePressure = 0.0f;
		return;
	}

	// =====================================================
	// 새로운 접근법: 콜라이더별 카운트 버퍼 사용
	// GPU에서 콜라이더 인덱스별로 충돌 카운트를 집계하고,
	// OwnerID로 필터링하여 이 액터의 콜라이더와 충돌한 입자 수를 얻음
	// =====================================================
	const int32 OwnerContactCount = GPUSimulator->GetContactCountForOwner(MyOwnerID);

	// Debug: 콜라이더 카운트 로그
	static int32 DebugLogCounter = 0;
	if (++DebugLogCounter % 60 == 0)  // 60프레임마다 한 번 로그
	{
		UE_LOG(LogTemp, Warning, TEXT("FluidInteraction: OwnerID=%d, ContactCount=%d, TotalColliders=%d"),
			MyOwnerID, OwnerContactCount, GPUSimulator->GetTotalColliderCount());
	}

	// 유체 태그별 카운트 초기화 및 설정
	CurrentFluidTagCounts.Empty();
	if (OwnerContactCount > 0)
	{
		// 현재는 기본 태그로 처리 (향후 태그 시스템 확장 가능)
		CurrentFluidTagCounts.FindOrAdd(NAME_None) = OwnerContactCount;
	}

	// 콜라이더 접촉 카운트로 이벤트 트리거
	CurrentContactCount = OwnerContactCount;

	// =====================================================
	// 힘 계산: 상세 피드백이 필요한 경우에만 처리
	// (피드백이 비활성화되어도 카운트 기반 이벤트는 동작)
	// =====================================================
	if (GPUSimulator->IsCollisionFeedbackEnabled())
	{
		TArray<FGPUCollisionFeedback> AllFeedback;
		int32 FeedbackCount = 0;
		GPUSimulator->GetAllCollisionFeedback(AllFeedback, FeedbackCount);

		// 본별 힘 처리 (Per-Bone Force)
		if (bEnablePerBoneForce)
		{
			ProcessPerBoneForces(DeltaTime, AllFeedback, FeedbackCount);
		}

		if (FeedbackCount > 0)
		{
			// 항력 계산 파라미터
			const float ParticleRadius = 3.0f;  // cm (기본값)
			const float ParticleArea = PI * ParticleRadius * ParticleRadius;  // cm²
			const float AreaInM2 = ParticleArea * 0.0001f;  // m² (cm² → m²)

			// 캐릭터/오브젝트 속도 가져오기
			FVector BodyVelocity = FVector::ZeroVector;
			if (Owner)
			{
				if (UCharacterMovementComponent* MovementComp = Owner->FindComponentByClass<UCharacterMovementComponent>())
				{
					BodyVelocity = MovementComp->Velocity;
				}
				else if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
				{
					BodyVelocity = RootPrimitive->GetPhysicsLinearVelocity();
				}
			}
			const FVector BodyVelocityInMS = BodyVelocity * 0.01f;

			FVector ForceAccum = FVector::ZeroVector;
			float DensitySum = 0.0f;
			int32 ForceContactCount = 0;

			for (int32 i = 0; i < FeedbackCount; ++i)
			{
				const FGPUCollisionFeedback& Feedback = AllFeedback[i];

				// ColliderOwnerID 필터링: 이 액터의 콜라이더와 충돌한 피드백만 힘 계산에 사용
				if (Feedback.ColliderOwnerID != 0 && Feedback.ColliderOwnerID != MyOwnerID)
				{
					continue;
				}

				// 파티클 속도 (cm/s → m/s)
				FVector ParticleVelocity(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z);
				FVector ParticleVelocityInMS = ParticleVelocity * 0.01f;

				// 상대 속도: v_rel = u_fluid - v_body
				FVector RelativeVelocity = ParticleVelocityInMS - BodyVelocityInMS;
				float RelativeSpeed = RelativeVelocity.Size();

				DensitySum += Feedback.Density;
				ForceContactCount++;

				if (RelativeSpeed < SMALL_NUMBER)
				{
					continue;
				}

				// 항력 공식: F = ½ρCdA|v|²
				float DragMagnitude = 0.5f * Feedback.Density * DragCoefficient * AreaInM2 * RelativeSpeed * RelativeSpeed;
				FVector DragDirection = RelativeVelocity / RelativeSpeed;
				ForceAccum += DragDirection * DragMagnitude;
			}

			// 힘을 cm 단위로 변환
			ForceAccum *= 100.0f;

			// 스무딩 적용
			FVector TargetForce = ForceAccum * DragForceMultiplier;
			SmoothedForce = FMath::VInterpTo(SmoothedForce, TargetForce, DeltaTime, ForceSmoothingSpeed);
			CurrentFluidForce = SmoothedForce;
			CurrentAveragePressure = (ForceContactCount > 0) ? (DensitySum / ForceContactCount) : 0.0f;
		}
		else
		{
			// 피드백 없음 → 힘 감쇠
			SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed);
			CurrentFluidForce = SmoothedForce;
			CurrentAveragePressure = 0.0f;
		}
	}
	else
	{
		// 상세 피드백 비활성화 → 힘 없음, 카운트만 사용
		SmoothedForce = FMath::VInterpTo(SmoothedForce, FVector::ZeroVector, DeltaTime, ForceSmoothingSpeed);
		CurrentFluidForce = SmoothedForce;
		CurrentAveragePressure = 0.0f;
	}

	// 이벤트 브로드캐스트
	if (OnFluidForceUpdate.IsBound())
	{
		OnFluidForceUpdate.Broadcast(CurrentFluidForce, CurrentAveragePressure, CurrentContactCount);
	}

	// 유체 태그 이벤트 업데이트 (OnFluidEnter/OnFluidExit)
	UpdateFluidTagEvents();
}

void UFluidInteractionComponent::UpdateFluidTagEvents()
{
	// 현재 프레임에서 충분한 파티클과 충돌 중인 태그 확인
	TSet<FName> CurrentlyColliding;
	for (const auto& Pair : CurrentFluidTagCounts)
	{
		if (Pair.Value >= MinParticleCountForFluidEvent)
		{
			CurrentlyColliding.Add(Pair.Key);
		}
	}

	// Exit 이벤트: 이전에 충돌 중이었지만 지금은 아닌 경우
	for (auto& Pair : PreviousFluidTagStates)
	{
		if (Pair.Value && !CurrentlyColliding.Contains(Pair.Key))
		{
			if (OnFluidExit.IsBound())
			{
				OnFluidExit.Broadcast(Pair.Key);
			}
			Pair.Value = false;
		}
	}

	// Enter 이벤트: 이전에 충돌 중이 아니었지만 지금은 충돌 중인 경우
	for (const FName& Tag : CurrentlyColliding)
	{
		bool* bWasCollidingWithTag = PreviousFluidTagStates.Find(Tag);
		if (!bWasCollidingWithTag || !(*bWasCollidingWithTag))
		{
			int32* CountPtr = CurrentFluidTagCounts.Find(Tag);
			int32 Count = CountPtr ? *CountPtr : 0;

			if (OnFluidEnter.IsBound())
			{
				OnFluidEnter.Broadcast(Tag, Count);
			}
			PreviousFluidTagStates.FindOrAdd(Tag) = true;
		}
	}
}

void UFluidInteractionComponent::ApplyFluidForceToCharacterMovement(float ForceScale)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = Owner->FindComponentByClass<UCharacterMovementComponent>();
	if (!MovementComp)
	{
		return;
	}

	// 힘 적용 (AddForce는 가속도로 변환됨)
	FVector ScaledForce = CurrentFluidForce * ForceScale;
	if (!ScaledForce.IsNearlyZero())
	{
		MovementComp->AddForce(ScaledForce);
	}
}

bool UFluidInteractionComponent::IsCollidingWithFluidTag(FName FluidTag) const
{
	const bool* bIsColliding = PreviousFluidTagStates.Find(FluidTag);
	return bIsColliding && *bIsColliding;
}

void UFluidInteractionComponent::EnableGPUCollisionFeedbackIfNeeded()
{
	// 이미 활성화되었으면 스킵
	if (bGPUFeedbackEnabled)
	{
		return;
	}

	if (!TargetSubsystem)
	{
		return;
	}

	// 모든 GPU 모듈에서 피드백 활성화
	for (auto* Module : TargetSubsystem->GetAllModules())
	{
		if (Module)
		{
			FGPUFluidSimulator* GPUSimulator = Module->GetGPUSimulator();
			if (GPUSimulator)
			{
				GPUSimulator->SetCollisionFeedbackEnabled(true);
				bGPUFeedbackEnabled = true;
				UE_LOG(LogTemp, Log, TEXT("FluidInteractionComponent: GPU Collision Feedback Enabled"));
			}
		}
	}
}

//=============================================================================
// Per-Bone Force Implementation
//=============================================================================

void UFluidInteractionComponent::InitializeBoneNameCache()
{
	if (bBoneNameCacheInitialized)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh || !SkelMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 BoneCount = RefSkeleton.GetNum();

	BoneIndexToNameCache.Empty(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		BoneIndexToNameCache.Add(i, RefSkeleton.GetBoneName(i));
	}

	bBoneNameCacheInitialized = true;
}

void UFluidInteractionComponent::ProcessPerBoneForces(float DeltaTime, const TArray<FGPUCollisionFeedback>& AllFeedback, int32 FeedbackCount)
{
	AActor* Owner = GetOwner();
	const int32 MyOwnerID = Owner ? Owner->GetUniqueID() : 0;

	// 본 이름 캐시 초기화 (첫 호출 시)
	if (!bBoneNameCacheInitialized)
	{
		InitializeBoneNameCache();
	}

	// 본별 원시 힘 집계 (이번 프레임)
	TMap<int32, FVector> RawBoneForces;
	TMap<int32, int32> BoneContactCounts;

	// 캐릭터/오브젝트 속도 가져오기
	FVector BodyVelocity = FVector::ZeroVector;
	if (Owner)
	{
		if (UCharacterMovementComponent* MovementComp = Owner->FindComponentByClass<UCharacterMovementComponent>())
		{
			BodyVelocity = MovementComp->Velocity;
		}
		else if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			BodyVelocity = RootPrimitive->GetPhysicsLinearVelocity();
		}
	}
	const FVector BodyVelocityInMS = BodyVelocity * 0.01f;  // cm/s → m/s

	// 항력 계산 파라미터
	const float ParticleRadius = 3.0f;  // cm (기본값)
	const float ParticleArea = PI * ParticleRadius * ParticleRadius;  // cm²
	const float AreaInM2 = ParticleArea * 0.0001f;  // m² (cm² → m²)

	for (int32 i = 0; i < FeedbackCount; ++i)
	{
		const FGPUCollisionFeedback& Feedback = AllFeedback[i];

		// OwnerID 필터링
		if (Feedback.ColliderOwnerID != 0 && Feedback.ColliderOwnerID != MyOwnerID)
		{
			continue;
		}

		// BoneIndex가 유효한 경우에만 처리
		if (Feedback.BoneIndex < 0)
		{
			continue;
		}

		// 파티클 속도 (cm/s → m/s)
		FVector ParticleVelocity(Feedback.ParticleVelocity.X, Feedback.ParticleVelocity.Y, Feedback.ParticleVelocity.Z);
		FVector ParticleVelocityInMS = ParticleVelocity * 0.01f;

		// 상대 속도
		FVector RelativeVelocity = ParticleVelocityInMS - BodyVelocityInMS;
		float RelativeSpeed = RelativeVelocity.Size();

		if (RelativeSpeed < SMALL_NUMBER)
		{
			continue;
		}

		// 항력 공식: F = ½ρCdA|v|²
		float DragMagnitude = 0.5f * Feedback.Density * DragCoefficient * AreaInM2 * RelativeSpeed * RelativeSpeed;
		FVector DragDirection = RelativeVelocity / RelativeSpeed;
		FVector DragForce = DragDirection * DragMagnitude;

		// cm 단위로 변환 후 배율 적용
		DragForce *= 100.0f * PerBoneForceMultiplier;

		// 본별 힘 누적
		FVector& BoneForce = RawBoneForces.FindOrAdd(Feedback.BoneIndex, FVector::ZeroVector);
		BoneForce += DragForce;

		int32& ContactCount = BoneContactCounts.FindOrAdd(Feedback.BoneIndex, 0);
		ContactCount++;
	}

	// 스무딩 적용 및 결과 저장
	// 먼저 기존 본들 감쇠
	TArray<int32> BonesToRemove;
	for (auto& Pair : SmoothedPerBoneForces)
	{
		const int32 BoneIdx = Pair.Key;
		FVector& SmoothedForceRef = Pair.Value;

		FVector* RawForce = RawBoneForces.Find(BoneIdx);
		FVector TargetForce = RawForce ? *RawForce : FVector::ZeroVector;

		SmoothedForceRef = FMath::VInterpTo(SmoothedForceRef, TargetForce, DeltaTime, PerBoneForceSmoothingSpeed);
		CurrentPerBoneForces.FindOrAdd(BoneIdx) = SmoothedForceRef;

		// 힘이 거의 0이면 제거 대상
		if (SmoothedForceRef.SizeSquared() < 0.01f && !RawForce)
		{
			BonesToRemove.Add(BoneIdx);
		}
	}

	// 새로운 본들 추가
	for (const auto& Pair : RawBoneForces)
	{
		const int32 BoneIdx = Pair.Key;
		if (!SmoothedPerBoneForces.Contains(BoneIdx))
		{
			FVector& SmoothedForceRef = SmoothedPerBoneForces.Add(BoneIdx, FVector::ZeroVector);
			SmoothedForceRef = FMath::VInterpTo(SmoothedForceRef, Pair.Value, DeltaTime, PerBoneForceSmoothingSpeed);
			CurrentPerBoneForces.FindOrAdd(BoneIdx) = SmoothedForceRef;
		}
	}

	// 0에 가까운 본 제거
	for (int32 BoneIdx : BonesToRemove)
	{
		SmoothedPerBoneForces.Remove(BoneIdx);
		CurrentPerBoneForces.Remove(BoneIdx);
	}

	// =====================================================
	// 디버그 로그: 3초마다 본별 항력 출력
	// =====================================================
	PerBoneForceDebugTimer += DeltaTime;
	if (PerBoneForceDebugTimer >= 3.0f)
	{
		PerBoneForceDebugTimer = 0.0f;

		// 피드백 데이터 분석 로그
		int32 TotalFeedback = FeedbackCount;
		int32 MatchedOwner = 0;
		int32 ValidBoneIndex = 0;
		int32 InvalidBoneIndex = 0;

		for (int32 i = 0; i < FeedbackCount; ++i)
		{
			const FGPUCollisionFeedback& Feedback = AllFeedback[i];

			if (Feedback.ColliderOwnerID == 0 || Feedback.ColliderOwnerID == MyOwnerID)
			{
				MatchedOwner++;
				if (Feedback.BoneIndex >= 0)
				{
					ValidBoneIndex++;
				}
				else
				{
					InvalidBoneIndex++;
				}
			}
		}

		// 피드백 OwnerID 샘플 수집
		TSet<int32> UniqueOwnerIDs;
		for (int32 i = 0; i < FMath::Min(FeedbackCount, 100); ++i)
		{
			UniqueOwnerIDs.Add(AllFeedback[i].ColliderOwnerID);
		}
		FString OwnerIDSamples;
		for (int32 ID : UniqueOwnerIDs)
		{
			OwnerIDSamples += FString::Printf(TEXT("%d, "), ID);
		}

		UE_LOG(LogTemp, Warning, TEXT("========== Per-Bone Force Debug (Owner: %s, ID: %d) =========="),
			Owner ? *Owner->GetName() : TEXT("None"), MyOwnerID);
		UE_LOG(LogTemp, Warning, TEXT("  Feedback: Total=%d, MatchedOwner=%d, ValidBone=%d, InvalidBone(=-1)=%d"),
			TotalFeedback, MatchedOwner, ValidBoneIndex, InvalidBoneIndex);
		UE_LOG(LogTemp, Warning, TEXT("  Feedback OwnerIDs 샘플: [%s]"), *OwnerIDSamples);

		if (CurrentPerBoneForces.Num() > 0)
		{
			for (const auto& Pair : CurrentPerBoneForces)
			{
				const int32 BoneIdx = Pair.Key;
				const FVector& Force = Pair.Value;
				const float ForceMagnitude = Force.Size();

				// 본 이름 가져오기
				FName BoneName = NAME_None;
				if (const FName* CachedName = BoneIndexToNameCache.Find(BoneIdx))
				{
					BoneName = *CachedName;
				}

				UE_LOG(LogTemp, Warning, TEXT("  [%d] %s: Force=(%.2f, %.2f, %.2f) Magnitude=%.2f"),
					BoneIdx,
					*BoneName.ToString(),
					Force.X, Force.Y, Force.Z,
					ForceMagnitude);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  -> No active bone forces"));
		}

		UE_LOG(LogTemp, Warning, TEXT("================================================================"));
	}
}

FVector UFluidInteractionComponent::GetFluidForceForBone(int32 BoneIndex) const
{
	const FVector* Force = CurrentPerBoneForces.Find(BoneIndex);
	return Force ? *Force : FVector::ZeroVector;
}

FVector UFluidInteractionComponent::GetFluidForceForBoneByName(FName BoneName) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		return FVector::ZeroVector;
	}

	int32 BoneIndex = SkelMesh->GetBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return FVector::ZeroVector;
	}

	return GetFluidForceForBone(BoneIndex);
}

void UFluidInteractionComponent::GetActiveBoneIndices(TArray<int32>& OutBoneIndices) const
{
	OutBoneIndices.Empty(CurrentPerBoneForces.Num());
	for (const auto& Pair : CurrentPerBoneForces)
	{
		if (Pair.Value.SizeSquared() > 0.01f)
		{
			OutBoneIndices.Add(Pair.Key);
		}
	}
}

bool UFluidInteractionComponent::GetStrongestBoneForce(int32& OutBoneIndex, FVector& OutForce) const
{
	OutBoneIndex = -1;
	OutForce = FVector::ZeroVector;
	float MaxForceSq = 0.0f;

	for (const auto& Pair : CurrentPerBoneForces)
	{
		float ForceSq = Pair.Value.SizeSquared();
		if (ForceSq > MaxForceSq)
		{
			MaxForceSq = ForceSq;
			OutBoneIndex = Pair.Key;
			OutForce = Pair.Value;
		}
	}

	return OutBoneIndex >= 0;
}

//=============================================================================
// Boundary Particles Implementation (Flex-style Adhesion)
//=============================================================================

void UFluidInteractionComponent::SampleTriangleSurface(const FVector& V0, const FVector& V1, const FVector& V2,
                                                        float Spacing, TArray<FVector>& OutPoints)
{
	// 삼각형 변 길이
	FVector Edge1 = V1 - V0;
	FVector Edge2 = V2 - V0;

	float Len1 = Edge1.Size();
	float Len2 = Edge2.Size();

	if (Len1 < SMALL_NUMBER || Len2 < SMALL_NUMBER)
	{
		return;
	}

	// 삼각형 면적 기반으로 샘플링 개수 결정
	FVector Cross = FVector::CrossProduct(Edge1, Edge2);
	float Area = Cross.Size() * 0.5f;

	if (Area < Spacing * Spacing * 0.1f)
	{
		// 너무 작은 삼각형은 중심점만
		OutPoints.Add((V0 + V1 + V2) / 3.0f);
		return;
	}

	// Barycentric 좌표로 균일 샘플링
	int32 NumSamplesU = FMath::Max(1, FMath::CeilToInt(Len1 / Spacing));
	int32 NumSamplesV = FMath::Max(1, FMath::CeilToInt(Len2 / Spacing));

	for (int32 i = 0; i <= NumSamplesU; ++i)
	{
		float u = (float)i / (float)NumSamplesU;

		for (int32 j = 0; j <= NumSamplesV; ++j)
		{
			float v = (float)j / (float)NumSamplesV;

			// u + v <= 1 조건 (삼각형 내부)
			if (u + v > 1.0f)
			{
				continue;
			}

			// Barycentric 좌표로 점 계산
			FVector Point = V0 + Edge1 * u + Edge2 * v;
			OutPoints.Add(Point);
		}
	}
}

void UFluidInteractionComponent::GenerateBoundaryParticles()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	BoundaryParticleLocalPositions.Empty();
	BoundaryParticleBoneIndices.Empty();
	BoundaryParticlePositions.Empty();
	BoundaryParticleVertexIndices.Empty();
	bIsSkeletalMesh = false;

	// 스켈레탈 메시 확인
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (SkelMesh && SkelMesh->GetSkeletalMeshAsset())
	{
		bIsSkeletalMesh = true;

		// 스켈레탈 메시 표면 샘플링
		USkeletalMesh* SkeletalMesh = SkelMesh->GetSkeletalMeshAsset();
		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();

		if (RenderData && RenderData->LODRenderData.Num() > 0)
		{
			// LOD 0 사용
			FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[0];

			// 정점 버퍼와 스킨 가중치 버퍼
			const FPositionVertexBuffer& PositionBuffer = LODData.StaticVertexBuffers.PositionVertexBuffer;
			FSkinWeightVertexBuffer& SkinWeightBuffer = LODData.SkinWeightVertexBuffer;
			const int32 NumVertices = PositionBuffer.GetNumVertices();

			// 바인드 포즈에서 본들의 컴포넌트 스페이스 트랜스폼 계산
			const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
			const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();

			// 각 본의 컴포넌트 스페이스 트랜스폼 미리 계산
			TArray<FTransform> BoneCSTransforms;
			BoneCSTransforms.SetNum(RefBonePose.Num());
			for (int32 BoneIdx = 0; BoneIdx < RefBonePose.Num(); ++BoneIdx)
			{
				int32 ParentIdx = RefSkeleton.GetParentIndex(BoneIdx);
				if (ParentIdx == INDEX_NONE)
				{
					BoneCSTransforms[BoneIdx] = RefBonePose[BoneIdx];
				}
				else
				{
					BoneCSTransforms[BoneIdx] = RefBonePose[BoneIdx] * BoneCSTransforms[ParentIdx];
				}
			}

			// 렌더 섹션 정보 (섹션별 BoneMap 사용)
			const TArray<FSkelMeshRenderSection>& RenderSections = LODData.RenderSections;

			// 인덱스 버퍼 가져오기 (삼각형 면 샘플링용)
			FMultiSizeIndexContainerData IndexData;
			LODData.MultiSizeIndexContainer.GetIndexBufferData(IndexData);

			if (NumVertices > 0 && IndexData.Indices.Num() > 0)
			{
				// 삼각형 면 샘플링
				int32 NumIndices = IndexData.Indices.Num();
				int32 NumTriangles = NumIndices / 3;

				// 정점별 본 정보 미리 계산 (각 정점의 주요 본 인덱스)
				TArray<int32> VertexBoneIndices;
				VertexBoneIndices.SetNum(NumVertices);

				for (int32 VertIdx = 0; VertIdx < NumVertices; ++VertIdx)
				{
					// 이 정점이 속한 섹션 찾기
					const FSkelMeshRenderSection* VertexSection = nullptr;
					for (const FSkelMeshRenderSection& Section : RenderSections)
					{
						if (VertIdx >= (int32)Section.BaseVertexIndex &&
						    VertIdx < (int32)(Section.BaseVertexIndex + Section.NumVertices))
						{
							VertexSection = &Section;
							break;
						}
					}

					// 가장 가중치가 높은 본 찾기
					int32 MaxLocalBoneIndex = 0;
					float MaxWeight = 0.0f;

					int32 InfluenceCount = SkinWeightBuffer.GetMaxBoneInfluences();
					for (int32 InfluenceIdx = 0; InfluenceIdx < InfluenceCount; ++InfluenceIdx)
					{
						int32 LocalBoneIdx = SkinWeightBuffer.GetBoneIndex(VertIdx, InfluenceIdx);
						uint8 WeightByte = SkinWeightBuffer.GetBoneWeight(VertIdx, InfluenceIdx);
						float Weight = WeightByte / 255.0f;

						if (Weight > MaxWeight)
						{
							MaxWeight = Weight;
							MaxLocalBoneIndex = LocalBoneIdx;
						}
					}

					// 섹션의 BoneMap으로 변환
					int32 SkeletonBoneIndex = MaxLocalBoneIndex;
					if (VertexSection && MaxLocalBoneIndex < VertexSection->BoneMap.Num())
					{
						SkeletonBoneIndex = VertexSection->BoneMap[MaxLocalBoneIndex];
					}
					VertexBoneIndices[VertIdx] = SkeletonBoneIndex;
				}

				// 삼각형별 샘플링
				float SpacingSq = BoundaryParticleSpacing * BoundaryParticleSpacing;

				for (int32 TriIdx = 0; TriIdx < NumTriangles; ++TriIdx)
				{
					// 삼각형의 세 정점 인덱스
					uint32 Idx0 = IndexData.Indices[TriIdx * 3 + 0];
					uint32 Idx1 = IndexData.Indices[TriIdx * 3 + 1];
					uint32 Idx2 = IndexData.Indices[TriIdx * 3 + 2];

					if (Idx0 >= (uint32)NumVertices || Idx1 >= (uint32)NumVertices || Idx2 >= (uint32)NumVertices)
						continue;

					// 세 정점의 바인드 포즈 위치
					FVector V0 = FVector(PositionBuffer.VertexPosition(Idx0));
					FVector V1 = FVector(PositionBuffer.VertexPosition(Idx1));
					FVector V2 = FVector(PositionBuffer.VertexPosition(Idx2));

					// 삼각형 변
					FVector Edge1 = V1 - V0;
					FVector Edge2 = V2 - V0;

					// 삼각형 면적
					FVector Cross = FVector::CrossProduct(Edge1, Edge2);
					float Area = Cross.Size() * 0.5f;

					// 면적 기반 샘플 수 결정
					int32 NumSamples = FMath::Max(1, FMath::CeilToInt(Area / SpacingSq));
					NumSamples = FMath::Min(NumSamples, 50);  // 삼각형당 최대 샘플 수 제한

					// 삼각형 내부 샘플링 (Barycentric 좌표)
					for (int32 SampleIdx = 0; SampleIdx < NumSamples; ++SampleIdx)
					{
						// 랜덤 Barycentric 좌표 (균일 분포)
						float u = FMath::FRand();
						float v = FMath::FRand();
						if (u + v > 1.0f)
						{
							u = 1.0f - u;
							v = 1.0f - v;
						}
						float w = 1.0f - u - v;

						// 샘플 위치
						FVector SamplePos = V0 * w + V1 * u + V2 * v;

						// 가장 가까운 정점의 본 사용 (Barycentric 가중치 기반)
						int32 ClosestVertIdx;
						if (w >= u && w >= v)
							ClosestVertIdx = Idx0;
						else if (u >= w && u >= v)
							ClosestVertIdx = Idx1;
						else
							ClosestVertIdx = Idx2;

						int32 BoneIndex = VertexBoneIndices[ClosestVertIdx];

						// 본 로컬 좌표 계산
						FVector BoneLocalPos = SamplePos;
						if (BoneIndex < BoneCSTransforms.Num())
						{
							BoneLocalPos = BoneCSTransforms[BoneIndex].InverseTransformPosition(SamplePos);
						}

						// 삼각형 노멀 계산 (외부 방향)
						FVector TriangleNormal = Cross.GetSafeNormal();
						// 로컬 노멀을 본 로컬 공간으로 변환
						FVector BoneLocalNormal = TriangleNormal;
						if (BoneIndex < BoneCSTransforms.Num())
						{
							BoneLocalNormal = BoneCSTransforms[BoneIndex].InverseTransformVector(TriangleNormal);
						}

						BoundaryParticleVertexIndices.Add(ClosestVertIdx);
						BoundaryParticleLocalPositions.Add(BoneLocalPos);
						BoundaryParticleBoneIndices.Add(BoneIndex);
						BoundaryParticleLocalNormals.Add(BoneLocalNormal);
					}
				}

				UE_LOG(LogTemp, Log, TEXT("FluidInteraction: Generated %d boundary particles from SkeletalMesh (Triangles: %d)"),
					BoundaryParticleLocalPositions.Num(), NumTriangles);
			}
		}
	}
	else
	{
		// 스태틱 메시 확인
		UStaticMeshComponent* StaticMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
		if (StaticMesh && StaticMesh->GetStaticMesh())
		{
			UStaticMesh* Mesh = StaticMesh->GetStaticMesh();

			if (Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0)
			{
				// LOD 0 사용
				FStaticMeshLODResources& LODResource = Mesh->GetRenderData()->LODResources[0];

				const FPositionVertexBuffer& PositionBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
				const int32 NumVertices = PositionBuffer.GetNumVertices();

				if (NumVertices > 0)
				{
					int32 Step = FMath::Max(1, FMath::CeilToInt(NumVertices / (5000.0f / BoundaryParticleSpacing)));

					for (int32 i = 0; i < NumVertices; i += Step)
					{
						FVector3f LocalPos = PositionBuffer.VertexPosition(i);
						BoundaryParticleLocalPositions.Add(FVector(LocalPos));
						BoundaryParticleBoneIndices.Add(-1);  // 스태틱 메시는 본 없음
						// StaticMesh의 경우 외부 방향 노멀 (원점에서 외부로)
						FVector Normal = FVector(LocalPos).GetSafeNormal();
						if (Normal.IsNearlyZero())
						{
							Normal = FVector::UpVector;
						}
						BoundaryParticleLocalNormals.Add(Normal);
					}

					UE_LOG(LogTemp, Log, TEXT("FluidInteraction: Generated %d boundary particles from StaticMesh (LOD0 vertices: %d)"),
						BoundaryParticleLocalPositions.Num(), NumVertices);
				}
			}
		}
		else
		{
			// 캡슐 컴포넌트 폴백
			UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
			if (Capsule)
			{
				float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
				float Radius = Capsule->GetScaledCapsuleRadius();

				// 캡슐 표면에 점 샘플링
				int32 NumRings = FMath::Max(3, FMath::CeilToInt(HalfHeight * 2.0f / BoundaryParticleSpacing));
				int32 NumSegments = FMath::Max(6, FMath::CeilToInt(2.0f * PI * Radius / BoundaryParticleSpacing));

				for (int32 Ring = 0; Ring <= NumRings; ++Ring)
				{
					float Z = -HalfHeight + (2.0f * HalfHeight * Ring / NumRings);
					float CurrentRadius = Radius;

					// 상하단 반구 처리
					if (FMath::Abs(Z) > HalfHeight - Radius)
					{
						float SphereZ = FMath::Abs(Z) - (HalfHeight - Radius);
						CurrentRadius = FMath::Sqrt(FMath::Max(0.0f, Radius * Radius - SphereZ * SphereZ));
					}

					for (int32 Seg = 0; Seg < NumSegments; ++Seg)
					{
						float Angle = 2.0f * PI * Seg / NumSegments;
						FVector LocalPos(
							CurrentRadius * FMath::Cos(Angle),
							CurrentRadius * FMath::Sin(Angle),
							Z
						);
						BoundaryParticleLocalPositions.Add(LocalPos);
						BoundaryParticleBoneIndices.Add(-1);
						// 캡슐 표면 노멀 (외부 방향)
						FVector Normal(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
						if (FMath::Abs(Z) > HalfHeight - Radius)
						{
							// 반구 부분은 중심에서 외부 방향
							FVector SphereCenter(0.0f, 0.0f, (Z > 0.0f) ? (HalfHeight - Radius) : -(HalfHeight - Radius));
							Normal = (LocalPos - SphereCenter).GetSafeNormal();
						}
						BoundaryParticleLocalNormals.Add(Normal);
					}
				}

				UE_LOG(LogTemp, Log, TEXT("FluidInteraction: Generated %d boundary particles from Capsule"),
					BoundaryParticleLocalPositions.Num());
			}
		}
	}

	// 월드 위치/노멀 배열 초기화
	BoundaryParticlePositions.SetNum(BoundaryParticleLocalPositions.Num());
	BoundaryParticleNormals.SetNum(BoundaryParticleLocalNormals.Num());
	bBoundaryParticlesInitialized = BoundaryParticleLocalPositions.Num() > 0;

	// 첫 프레임 위치 업데이트
	if (bBoundaryParticlesInitialized)
	{
		UpdateBoundaryParticlePositions();
	}
}

void UFluidInteractionComponent::UpdateBoundaryParticlePositions()
{
	AActor* Owner = GetOwner();
	if (!Owner || BoundaryParticlePositions.Num() == 0)
	{
		return;
	}

	const int32 NumParticles = BoundaryParticlePositions.Num();
	const bool bHasNormals = (BoundaryParticleLocalNormals.Num() == NumParticles);

	// 스켈레탈 메시인 경우 본 트랜스폼 적용
	if (bIsSkeletalMesh)
	{
		USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (SkelMesh && BoundaryParticleLocalPositions.Num() == NumParticles)
		{
			for (int32 i = 0; i < NumParticles; ++i)
			{
				int32 BoneIdx = BoundaryParticleBoneIndices[i];
				if (BoneIdx >= 0)
				{
					// 현재 본의 월드 트랜스폼으로 본 로컬 좌표를 월드로 변환
					FTransform BoneWorldTransform = SkelMesh->GetBoneTransform(BoneIdx);
					BoundaryParticlePositions[i] = BoneWorldTransform.TransformPosition(BoundaryParticleLocalPositions[i]);
					// 노멀도 변환
					if (bHasNormals)
					{
						BoundaryParticleNormals[i] = BoneWorldTransform.TransformVectorNoScale(BoundaryParticleLocalNormals[i]);
					}
				}
				else
				{
					FTransform ComponentTransform = SkelMesh->GetComponentTransform();
					BoundaryParticlePositions[i] = ComponentTransform.TransformPosition(BoundaryParticleLocalPositions[i]);
					if (bHasNormals)
					{
						BoundaryParticleNormals[i] = ComponentTransform.TransformVectorNoScale(BoundaryParticleLocalNormals[i]);
					}
				}
			}
		}
	}
	else
	{
		// 스태틱 메시 또는 다른 컴포넌트 - 로컬 → 월드 변환
		USceneComponent* RootComp = Owner->GetRootComponent();
		if (RootComp && BoundaryParticleLocalPositions.Num() == NumParticles)
		{
			FTransform ComponentTransform = RootComp->GetComponentTransform();

			for (int32 i = 0; i < NumParticles; ++i)
			{
				BoundaryParticlePositions[i] = ComponentTransform.TransformPosition(BoundaryParticleLocalPositions[i]);
				if (bHasNormals)
				{
					BoundaryParticleNormals[i] = ComponentTransform.TransformVectorNoScale(BoundaryParticleLocalNormals[i]);
				}
			}
		}
	}
}

void UFluidInteractionComponent::DrawDebugBoundaryParticles()
{
	UWorld* World = GetWorld();
	if (!World || BoundaryParticlePositions.Num() == 0)
	{
		return;
	}

	for (const FVector& Pos : BoundaryParticlePositions)
	{
		DrawDebugPoint(
			World,
			Pos,
			BoundaryParticleDebugSize,
			BoundaryParticleDebugColor,
			false,  // bPersistentLines
			-1.0f,  // LifeTime
			SDPG_Foreground  // DepthPriority - 메쉬 앞에 항상 표시
		);
	}
}

void UFluidInteractionComponent::RegenerateBoundaryParticles()
{
	bBoundaryParticlesInitialized = false;
	bIsSkeletalMesh = false;
	BoundaryParticleLocalPositions.Empty();
	BoundaryParticleBoneIndices.Empty();
	BoundaryParticlePositions.Empty();
	BoundaryParticleVertexIndices.Empty();
	BoundaryParticleNormals.Empty();
	BoundaryParticleLocalNormals.Empty();

	if (bEnableBoundaryParticles)
	{
		GenerateBoundaryParticles();
	}
}

void UFluidInteractionComponent::CollectGPUBoundaryParticles(FGPUBoundaryParticles& OutBoundaryParticles) const
{
	if (!bBoundaryParticlesInitialized || BoundaryParticlePositions.Num() == 0)
	{
		return;
	}

	const int32 NumParticles = BoundaryParticlePositions.Num();
	OutBoundaryParticles.Particles.Reserve(OutBoundaryParticles.Particles.Num() + NumParticles);

	// Component의 고유 ID 생성
	const int32 OwnerID = GetUniqueID();

	for (int32 i = 0; i < NumParticles; ++i)
	{
		FVector3f Position = FVector3f(BoundaryParticlePositions[i]);
		FVector3f Normal = (i < BoundaryParticleNormals.Num())
			? FVector3f(BoundaryParticleNormals[i])
			: FVector3f(0.0f, 0.0f, 1.0f);

		// Psi는 경계 입자의 볼륨 기여도
		// 낮을수록 밀려나는 힘 감소, 높을수록 강하게 밀려남
		float Psi = 0.1f;

		OutBoundaryParticles.Add(Position, Normal, OwnerID, Psi);
	}
}

void UFluidInteractionComponent::CollectLocalBoundaryParticles(TArray<FGPUBoundaryParticleLocal>& OutLocalParticles) const
{
	if (!bBoundaryParticlesInitialized || BoundaryParticleLocalPositions.Num() == 0)
	{
		return;
	}

	const int32 NumParticles = BoundaryParticleLocalPositions.Num();
	OutLocalParticles.Reserve(OutLocalParticles.Num() + NumParticles);

	for (int32 i = 0; i < NumParticles; ++i)
	{
		FVector3f LocalPosition = FVector3f(BoundaryParticleLocalPositions[i]);
		FVector3f LocalNormal = (i < BoundaryParticleLocalNormals.Num())
			? FVector3f(BoundaryParticleLocalNormals[i])
			: FVector3f(0.0f, 0.0f, 1.0f);
		int32 BoneIndex = (i < BoundaryParticleBoneIndices.Num()) ? BoundaryParticleBoneIndices[i] : -1;

		// Psi는 경계 입자의 볼륨 기여도
		float Psi = 0.1f;

		OutLocalParticles.Add(FGPUBoundaryParticleLocal(LocalPosition, BoneIndex, LocalNormal, Psi));
	}
}

void UFluidInteractionComponent::CollectBoneTransformsForBoundary(TArray<FMatrix>& OutBoneTransforms, FMatrix& OutComponentTransform) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutComponentTransform = FMatrix::Identity;
		return;
	}

	if (bIsSkeletalMesh)
	{
		USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (SkelMesh)
		{
			// 모든 본의 월드 트랜스폼 수집
			const int32 NumBones = SkelMesh->GetNumBones();
			OutBoneTransforms.SetNum(NumBones);

			for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
			{
				FTransform BoneWorldTransform = SkelMesh->GetBoneTransform(BoneIdx);
				OutBoneTransforms[BoneIdx] = BoneWorldTransform.ToMatrixWithScale();
			}

			OutComponentTransform = SkelMesh->GetComponentTransform().ToMatrixWithScale();
		}
		else
		{
			OutComponentTransform = FMatrix::Identity;
		}
	}
	else
	{
		// 스태틱 메시 또는 다른 컴포넌트
		USceneComponent* RootComp = Owner->GetRootComponent();
		if (RootComp)
		{
			OutComponentTransform = RootComp->GetComponentTransform().ToMatrixWithScale();
		}
		else
		{
			OutComponentTransform = FMatrix::Identity;
		}
		OutBoneTransforms.Empty();
	}
}
