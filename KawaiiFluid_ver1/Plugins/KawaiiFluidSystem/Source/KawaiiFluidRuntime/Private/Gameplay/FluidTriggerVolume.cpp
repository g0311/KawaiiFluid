// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Gameplay/FluidTriggerVolume.h"
#include "Core/FluidSimulator.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

AFluidTriggerVolume::AFluidTriggerVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// 트리거 박스 생성
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // Query로만 사용
	TriggerBox->SetHiddenInGame(false);
	RootComponent = TriggerBox;
}

void AFluidTriggerVolume::BeginPlay()
{
	Super::BeginPlay();
	
	// 초기화
	ParticlesInVolume.Empty();
	PreviousParticleCount = 0;
	bIsTriggered = false;
	SimulatorTriggerStates.Empty();
}

void AFluidTriggerVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bEnableDetection)
	{
		DetectParticles();
		
		if (bShowDebugInfo)
		{
			DrawDebugInfo();
		}
	}
}

void AFluidTriggerVolume::DetectParticles()
{
	ParticlesInVolume.Empty();
	
	// 타겟 Simulator 찾기
	TArray<AFluidSimulator*> Simulators;
	
	if (TargetSimulator)
	{
		// 특정 Simulator만
		Simulators.Add(TargetSimulator);
	}
	else
	{
		// 모든 FluidSimulator 찾기
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		
		for (TActorIterator<AFluidSimulator> It(World); It; ++It)
		{
			Simulators.Add(*It);
		}
	}
	
	// 각 Simulator의 파티클 체크
	for (AFluidSimulator* Sim : Simulators)
	{
		DetectParticlesFromSimulator(Sim);
	}
	
	// 이벤트 발생 조건 체크
	int32 CurrentCount = ParticlesInVolume.Num();
	
	// Enter 이벤트
	if (CurrentCount >= MinParticleCountForTrigger && !bIsTriggered)
	{
		bIsTriggered = true;
		
		// Delegate 호출 (각 Simulator별로)
		for (AFluidSimulator* Sim : Simulators)
		{
			if (OnFluidEnter.IsBound() && IsSimulatorInVolume(Sim))
			{
				OnFluidEnter.Broadcast(Sim, ParticlesInVolume, CurrentCount);
			}
		}
	}
	// Exit 이벤트
	else if (CurrentCount < MinParticleCountForTrigger && bIsTriggered)
	{
		bIsTriggered = false;
		
		// Delegate 호출
		for (AFluidSimulator* Sim : Simulators)
		{
			if (OnFluidExit.IsBound())
			{
				OnFluidExit.Broadcast(Sim);
			}
		}
	}
	
	PreviousParticleCount = CurrentCount;
}

void AFluidTriggerVolume::DetectParticlesFromSimulator(AFluidSimulator* Simulator)
{
	if (!Simulator || !TriggerBox)
	{
		return;
	}
	
	const TArray<FFluidParticle>& Particles = Simulator->GetParticles();
	
	// 박스 월드 변환
	FVector BoxCenter = TriggerBox->GetComponentLocation();
	FVector BoxExtent = TriggerBox->GetScaledBoxExtent();
	FQuat BoxRotation = TriggerBox->GetComponentQuat();
	
	// 각 파티클 체크
	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		const FVector& ParticlePos = Particles[i].Position;
		
		// 월드 좌표 → 로컬 좌표 변환
		FVector LocalPos = BoxRotation.UnrotateVector(ParticlePos - BoxCenter);
		
		// 박스 안에 있는지 체크
		if (FMath::Abs(LocalPos.X) <= BoxExtent.X &&
		    FMath::Abs(LocalPos.Y) <= BoxExtent.Y &&
		    FMath::Abs(LocalPos.Z) <= BoxExtent.Z)
		{
			ParticlesInVolume.Add(i);
		}
	}
}

float AFluidTriggerVolume::GetFillRatio() const
{
	if (!TargetSimulator)
	{
		return 0.0f;
	}
	
	int32 TotalParticles = TargetSimulator->GetParticleCount();
	if (TotalParticles == 0)
	{
		return 0.0f;
	}
	
	return static_cast<float>(ParticlesInVolume.Num()) / static_cast<float>(TotalParticles);
}

bool AFluidTriggerVolume::IsSimulatorInVolume(AFluidSimulator* Simulator) const
{
	if (!Simulator)
	{
		return false;
	}
	
	const TArray<FFluidParticle>& Particles = Simulator->GetParticles();
	
	// 볼륨 안에 이 Simulator의 파티클이 하나라도 있는지 체크
	// (간단히 ParticlesInVolume이 비어있지 않으면 true)
	return ParticlesInVolume.Num() > 0;
}

void AFluidTriggerVolume::DrawDebugInfo()
{
	if (!TriggerBox)
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	// 박스 그리기
	FVector BoxCenter = TriggerBox->GetComponentLocation();
	FVector BoxExtent = TriggerBox->GetScaledBoxExtent();
	FQuat BoxRotation = TriggerBox->GetComponentQuat();
	
	FColor BoxColor = bIsTriggered ? FColor::Green : FColor::Yellow;
	
	DrawDebugBox(
		World,
		BoxCenter,
		BoxExtent,
		BoxRotation,
		BoxColor,
		false,  // bPersistentLines
		-1.0f,  // LifeTime (한 프레임)
		0,      // DepthPriority
		2.0f    // Thickness
	);
	
	// 텍스트 정보
	FString InfoText = FString::Printf(
		TEXT("Particles: %d/%d\nFill: %.1f%%\nTriggered: %s"),
		ParticlesInVolume.Num(),
		MinParticleCountForTrigger,
		GetFillRatio() * 100.0f,
		bIsTriggered ? TEXT("YES") : TEXT("NO")
	);
	
	DrawDebugString(
		World,
		BoxCenter + FVector(0, 0, BoxExtent.Z + 50.0f),
		InfoText,
		nullptr,
		FColor::White,
		0.0f,  // Duration (한 프레임)
		true   // bDrawShadow
	);
}
