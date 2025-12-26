// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/KawaiiFluidComponent.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Core/KawaiiFluidSimulationTypes.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Data/KawaiiFluidPresetDataAsset.h"

UKawaiiFluidComponent::UKawaiiFluidComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// 시뮬레이션 모듈 생성
	SimulationModule = CreateDefaultSubobject<UKawaiiFluidSimulationModule>(TEXT("SimulationModule"));
}

void UKawaiiFluidComponent::BeginPlay()
{
	Super::BeginPlay();

	// 시뮬레이션 모듈 초기화
	if (SimulationModule)
	{
		// Preset 없으면 기본 생성
		if (!SimulationModule->Preset)
		{
			SimulationModule->Preset = NewObject<UKawaiiFluidPresetDataAsset>(this, NAME_None, RF_Transient);
			UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidComponent [%s]: No Preset assigned, using default values"), *GetName());
		}
		SimulationModule->Initialize(SimulationModule->Preset);
	}

	// Subsystem에 등록
	RegisterToSubsystem();

	// 자동 스폰
	if (bSpawnOnBeginPlay && AutoSpawnCount > 0 && SimulationModule)
	{
		SimulationModule->SpawnParticles(GetOwner()->GetActorLocation(), AutoSpawnCount, AutoSpawnRadius);
	}

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidComponent BeginPlay: %s"), *GetName());
}

void UKawaiiFluidComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Subsystem에서 등록 해제
	UnregisterFromSubsystem();

	// 이벤트 클리어
	OnParticleHit.Clear();

	// 시뮬레이션 모듈 정리
	if (SimulationModule)
	{
		SimulationModule->Shutdown();
	}

	Super::EndPlay(EndPlayReason);

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidComponent EndPlay: %s"), *GetName());
}

void UKawaiiFluidComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 이벤트 카운터 리셋
	EventCountThisFrame = 0;

	// 연속 스폰 처리
	if (bContinuousSpawn)
	{
		ProcessContinuousSpawn(DeltaTime);
	}
}

#if WITH_EDITOR
void UKawaiiFluidComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (SimulationModule)
	{
		SimulationModule->MarkRuntimePresetDirty();
	}
}
#endif

//========================================
// IKawaiiFluidRenderable Interface
//========================================

FKawaiiFluidRenderResource* UKawaiiFluidComponent::GetFluidRenderResource() const
{
	return nullptr;
}

bool UKawaiiFluidComponent::IsFluidRenderResourceValid() const
{
	return false;
}

float UKawaiiFluidComponent::GetParticleRenderRadius() const
{
	return ParticleRenderRadius;
}

FString UKawaiiFluidComponent::GetDebugName() const
{
	return FString::Printf(TEXT("KawaiiFluid_%s"), *GetName());
}

bool UKawaiiFluidComponent::ShouldUseSSFR() const
{
	return false;
}

bool UKawaiiFluidComponent::ShouldUseDebugMesh() const
{
	return true;
}

UInstancedStaticMeshComponent* UKawaiiFluidComponent::GetDebugMeshComponent() const
{
	return nullptr;
}

int32 UKawaiiFluidComponent::GetParticleCount() const
{
	return SimulationModule ? SimulationModule->GetParticleCount() : 0;
}

//========================================
// Component-Level API
//========================================

FKawaiiFluidSimulationParams UKawaiiFluidComponent::BuildSimulationParams()
{
	FKawaiiFluidSimulationParams Params;

	if (SimulationModule)
	{
		Params = SimulationModule->BuildSimulationParams();
	}

	// 모듈에서 접근 불가능한 값들 설정
	Params.World = GetWorld();
	Params.IgnoreActor = GetOwner();
	Params.bUseWorldCollision = bUseWorldCollision;

	// 이벤트 시스템 설정
	Params.bEnableCollisionEvents = bEnableParticleHitEvents;
	Params.MinVelocityForEvent = MinVelocityForEvent;
	Params.MaxEventsPerFrame = MaxEventsPerFrame;
	Params.EventCooldownPerParticle = EventCooldownPerParticle;

	if (bEnableParticleHitEvents && SimulationModule)
	{
		// 쿨다운 추적용 맵 연결
		Params.ParticleLastEventTimePtr = &SimulationModule->GetParticleLastEventTimeMap();

		// 현재 게임 시간
		if (UWorld* World = GetWorld())
		{
			Params.CurrentGameTime = World->GetTimeSeconds();
		}

		// 콜백 바인딩
		Params.OnCollisionEvent.BindUObject(this, &UKawaiiFluidComponent::HandleCollisionEvent);
	}

	return Params;
}

bool UKawaiiFluidComponent::ShouldSimulateIndependently() const
{
	return SimulationModule ? SimulationModule->IsIndependentSimulation() : false;
}

//========================================
// Continuous Spawn
//========================================

void UKawaiiFluidComponent::ProcessContinuousSpawn(float DeltaTime)
{
	if (!SimulationModule || ParticlesPerSecond <= 0.0f)
	{
		return;
	}

	// 최대 파티클 수 체크
	if (MaxParticleCount > 0 && SimulationModule->GetParticleCount() >= MaxParticleCount)
	{
		return;
	}

	SpawnAccumulatedTime += DeltaTime;
	const float SpawnInterval = 1.0f / ParticlesPerSecond;

	while (SpawnAccumulatedTime >= SpawnInterval)
	{
		// 스폰 위치 계산
		FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnOffset;

		// 반경 내 랜덤 위치
		if (ContinuousSpawnRadius > 0.0f)
		{
			FVector RandomOffset = FMath::VRand() * FMath::FRandRange(0.0f, ContinuousSpawnRadius);
			SpawnLocation += RandomOffset;
		}

		// 파티클 스폰
		SimulationModule->SpawnParticle(SpawnLocation, SpawnVelocity);

		SpawnAccumulatedTime -= SpawnInterval;

		// 최대 파티클 수 체크
		if (MaxParticleCount > 0 && SimulationModule->GetParticleCount() >= MaxParticleCount)
		{
			SpawnAccumulatedTime = 0.0f;
			break;
		}
	}
}

//========================================
// Event System
//========================================

void UKawaiiFluidComponent::HandleCollisionEvent(const FKawaiiFluidCollisionEvent& Event)
{
	// 이벤트 횟수 제한 체크
	if (MaxEventsPerFrame > 0 && EventCountThisFrame >= MaxEventsPerFrame)
	{
		return;
	}

	// 쿨다운 체크
	if (SimulationModule && EventCooldownPerParticle > 0.0f)
	{
		TMap<int32, float>& LastEventTimeMap = SimulationModule->GetParticleLastEventTimeMap();
		float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		if (float* LastTime = LastEventTimeMap.Find(Event.ParticleIndex))
		{
			if (CurrentTime - *LastTime < EventCooldownPerParticle)
			{
				return; // 쿨다운 중
			}
		}

		// 마지막 이벤트 시간 갱신
		LastEventTimeMap.Add(Event.ParticleIndex, CurrentTime);
	}

	// 이벤트 카운터 증가
	EventCountThisFrame++;

	// 델리게이트 브로드캐스트
	if (OnParticleHit.IsBound())
	{
		OnParticleHit.Broadcast(
			Event.ParticleIndex,
			Event.HitActor.Get(),
			Event.HitLocation,
			Event.HitNormal,
			Event.HitSpeed
		);
	}
}

//========================================
// Subsystem Registration
//========================================

void UKawaiiFluidComponent::RegisterToSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->RegisterComponent(this);
		}
	}
}

void UKawaiiFluidComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->UnregisterComponent(this);
		}
	}
}
