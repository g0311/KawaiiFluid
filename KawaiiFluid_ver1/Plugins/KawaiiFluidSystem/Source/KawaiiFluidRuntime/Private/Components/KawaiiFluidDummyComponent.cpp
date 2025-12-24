// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/KawaiiFluidDummyComponent.h"
#include "Rendering/KawaiiFluidRenderResource.h"
#include "Rendering/FluidRendererSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

UKawaiiFluidDummyComponent::UKawaiiFluidDummyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

UKawaiiFluidDummyComponent::~UKawaiiFluidDummyComponent() = default;

void UKawaiiFluidDummyComponent::BeginPlay()
{
	Super::BeginPlay();

	// Render Resource 초기화
	InitializeRenderResource();

	// Debug Mesh 초기화
	if (bEnableRendering && ShouldUseDebugMesh())
	{
		InitializeDebugMesh();
	}

	// Niagara 초기화
	if (bEnableRendering && ShouldUseNiagara())
	{
		InitializeNiagara();
	}

	// 테스트 데이터 생성
	GenerateTestParticles();

	// ✅ FluidRendererSubsystem에 Component로 등록
	if (bEnableRendering)
	{
		if (UWorld* World = GetWorld())
		{
			if (UFluidRendererSubsystem* Subsystem = World->GetSubsystem<UFluidRendererSubsystem>())
			{
				Subsystem->RegisterRenderableComponent(this);

				AActor* Owner = GetOwner();
				const TCHAR* ModeStr = (RenderingMode == EKawaiiFluidRenderingMode::SSFR) ? TEXT("SSFR") :
				                       (RenderingMode == EKawaiiFluidRenderingMode::DebugMesh) ? TEXT("DebugMesh") : TEXT("Both");

				UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent registered: %s (Owner: %s, Mode: %s)"),
					*GetName(),
					Owner ? *Owner->GetName() : TEXT("None"),
					ModeStr);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent: Generated %d test particles"),
		TestParticles.Num());
}

void UKawaiiFluidDummyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ✅ FluidRendererSubsystem에서 Component 해제
	if (UWorld* World = GetWorld())
	{
		if (UFluidRendererSubsystem* Subsystem = World->GetSubsystem<UFluidRendererSubsystem>())
		{
			Subsystem->UnregisterRenderableComponent(this);
			UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent: Unregistered from renderer subsystem"));
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UKawaiiFluidDummyComponent::BeginDestroy()
{
	// Render Resource 정리
	if (RenderResource.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(ReleaseFluidDummyComponentRenderResource)(
			[RenderResource = MoveTemp(RenderResource)](FRHICommandListImmediate& RHICmdList) mutable
			{
				if (RenderResource.IsValid())
				{
					RenderResource->ReleaseResource();
					RenderResource.Reset();
				}
			}
		);
	}

	Super::BeginDestroy();
}

void UKawaiiFluidDummyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableRendering || TestParticles.Num() == 0 || bAnimationPaused)
	{
		return;
	}

	// 애니메이션 모드에서만 업데이트
	if (DataMode == EKawaiiFluidDummyGenMode::Animated || DataMode == EKawaiiFluidDummyGenMode::Wave)
	{
		UpdateAnimatedParticles(DeltaTime);

		// GPU 버퍼 업데이트
		if (RenderResource.IsValid())
		{
			RenderResource->UpdateParticleData(TestParticles);
		}
	}

	// Debug Mesh 업데이트
	if (ShouldUseDebugMesh())
	{
		UpdateDebugMeshInstances();
	}

	// Niagara 업데이트
	if (ShouldUseNiagara() && NiagaraComponent)
	{
		// Niagara Data Interface가 자동으로 데이터 가져감
		// 추가 업데이트 필요 없음
	}
}

void UKawaiiFluidDummyComponent::InitializeRenderResource()
{
	RenderResource = MakeShared<FKawaiiFluidRenderResource>();

	ENQUEUE_RENDER_COMMAND(InitFluidDummyComponentRenderResource)(
		[RenderResourcePtr = RenderResource.Get()](FRHICommandListImmediate& RHICmdList)
		{
			RenderResourcePtr->InitResource(RHICmdList);
		}
	);
}

void UKawaiiFluidDummyComponent::InitializeDebugMesh()
{
	// ✅ 이미 생성되었으면 스킵
	if (DebugMeshComponent && DebugMeshComponent->IsValidLowLevel())
	{
		DebugMeshComponent->SetVisibility(true);
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// ✅ this를 Outer로 사용 (Component 생명주기 동기화)
	DebugMeshComponent = NewObject<UInstancedStaticMeshComponent>(
		this,  // ← Owner 대신 this! (생명주기 동기화)
		UInstancedStaticMeshComponent::StaticClass(),
		TEXT("DebugMesh")
	);

	if (DebugMeshComponent)
	{
		// ✅ Owner의 RootComponent에 Attach
		DebugMeshComponent->SetupAttachment(Owner->GetRootComponent());
		DebugMeshComponent->RegisterComponent();
		DebugMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// ✅ LoadObject 사용 (런타임 안전)
		UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereMesh)
		{
			DebugMeshComponent->SetStaticMesh(SphereMesh);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidDummyComponent: Failed to load Sphere mesh"));
		}

		// ✅ LoadObject 사용 (런타임 안전)
		UMaterial* DefaultMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (DefaultMaterial)
		{
			DebugMeshComponent->SetMaterial(0, DefaultMaterial);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidDummyComponent: Failed to load default material"));
		}

		UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent: Debug Mesh created"));
	}
}

void UKawaiiFluidDummyComponent::InitializeNiagara()
{
	// 이미 생성되었으면 스킵
	if (NiagaraComponent && NiagaraComponent->IsValidLowLevel())
	{
		NiagaraComponent->SetVisibility(true);
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidDummyComponent: No owner for Niagara component"));
		return;
	}

	if (!NiagaraSystemTemplate)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidDummyComponent: No Niagara System Template assigned"));
		return;
	}

	// Niagara Component 생성
	NiagaraComponent = NewObject<UNiagaraComponent>(
		this,
		UNiagaraComponent::StaticClass(),
		TEXT("NiagaraFluid")
	);

	if (NiagaraComponent)
	{
		// Owner의 RootComponent에 Attach
		NiagaraComponent->SetupAttachment(Owner->GetRootComponent());
		NiagaraComponent->RegisterComponent();
		
		// Niagara System 설정
		NiagaraComponent->SetAsset(NiagaraSystemTemplate);
		
		// Auto Activate
		NiagaraComponent->Activate(true);

		UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent: Niagara Component created and activated"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidDummyComponent: Failed to create Niagara Component"));
	}
}

void UKawaiiFluidDummyComponent::UpdateDebugMeshInstances()
{
	if (!DebugMeshComponent || TestParticles.Num() == 0)
	{
		return;
	}

	const int32 NumParticles = TestParticles.Num();
	const int32 InstanceCount = DebugMeshComponent->GetInstanceCount();

	// 인스턴스 수 조정
	if (InstanceCount < NumParticles)
	{
		for (int32 i = InstanceCount; i < NumParticles; ++i)
		{
			DebugMeshComponent->AddInstance(FTransform::Identity);
		}
	}
	else if (InstanceCount > NumParticles)
	{
		for (int32 i = InstanceCount - 1; i >= NumParticles; --i)
		{
			DebugMeshComponent->RemoveInstance(i);
		}
	}

	// 스케일 계산 (기본 Sphere는 지름 100cm = 반지름 50cm)
	const float Scale = ParticleRadius / 50.0f;
	const FVector ScaleVec(Scale, Scale, Scale);

	// 각 파티클 위치로 인스턴스 업데이트
	for (int32 i = 0; i < NumParticles; ++i)
	{
		FTransform InstanceTransform;
		InstanceTransform.SetLocation(FVector(TestParticles[i].Position));
		InstanceTransform.SetScale3D(ScaleVec);

		DebugMeshComponent->UpdateInstanceTransform(i, InstanceTransform, true, false, false);
	}

	// 일괄 업데이트
	DebugMeshComponent->MarkRenderStateDirty();
}

void UKawaiiFluidDummyComponent::GenerateTestParticles()
{
	switch (DataMode)
	{
	case EKawaiiFluidDummyGenMode::Static:
		GenerateStaticData();
		break;

	case EKawaiiFluidDummyGenMode::Animated:
		GenerateStaticData();
		break;

	case EKawaiiFluidDummyGenMode::GridPattern:
		GenerateGridPattern();
		break;

	case EKawaiiFluidDummyGenMode::Sphere:
		GenerateSpherePattern();
		break;

	case EKawaiiFluidDummyGenMode::Wave:
		GenerateGridPattern();
		break;
	}

	// GPU 버퍼로 전송
	if (RenderResource.IsValid())
	{
		RenderResource->UpdateParticleData(TestParticles);
	}
}

void UKawaiiFluidDummyComponent::GenerateStaticData()
{
	TestParticles.Empty();
	TestParticles.Reserve(ParticleCount);

	AActor* Owner = GetOwner();
	const FVector BaseLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	for (int32 i = 0; i < ParticleCount; ++i)
	{
		FKawaiiRenderParticle Particle;

		FVector RandomOffset = FVector(
			FMath::FRandRange(-SpawnExtent.X, SpawnExtent.X),
			FMath::FRandRange(-SpawnExtent.Y, SpawnExtent.Y),
			FMath::FRandRange(-SpawnExtent.Z, SpawnExtent.Z)
		);

		Particle.Position = FVector3f(BaseLocation + RandomOffset);
		Particle.Velocity = FVector3f::ZeroVector;
		Particle.Radius = ParticleRadius;
		Particle.Padding = 0.0f;

		TestParticles.Add(Particle);
	}
}

void UKawaiiFluidDummyComponent::GenerateGridPattern()
{
	TestParticles.Empty();

	const int32 GridSize = FMath::CeilToInt(FMath::Pow(ParticleCount, 1.0f / 3.0f));
	AActor* Owner = GetOwner();
	const FVector BaseLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	const float Spacing = ParticleRadius * 2.5f;

	int32 Count = 0;
	for (int32 x = 0; x < GridSize && Count < ParticleCount; ++x)
	{
		for (int32 y = 0; y < GridSize && Count < ParticleCount; ++y)
		{
			for (int32 z = 0; z < GridSize && Count < ParticleCount; ++z)
			{
				FKawaiiRenderParticle Particle;

				FVector GridPos = FVector(
					(x - GridSize / 2) * Spacing,
					(y - GridSize / 2) * Spacing,
					(z - GridSize / 2) * Spacing
				);

				Particle.Position = FVector3f(BaseLocation + GridPos);
				Particle.Velocity = FVector3f::ZeroVector;
				Particle.Radius = ParticleRadius;
				Particle.Padding = 0.0f;

				TestParticles.Add(Particle);
				Count++;
			}
		}
	}
}

void UKawaiiFluidDummyComponent::GenerateSpherePattern()
{
	TestParticles.Empty();
	TestParticles.Reserve(ParticleCount);

	AActor* Owner = GetOwner();
	const FVector BaseLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	const float SphereRadius = SpawnExtent.X;

	for (int32 i = 0; i < ParticleCount; ++i)
	{
		FKawaiiRenderParticle Particle;

		// Fibonacci Sphere
		float Phi = FMath::Acos(1.0f - 2.0f * (i + 0.5f) / ParticleCount);
		float Theta = PI * (1.0f + FMath::Sqrt(5.0f)) * i;

		FVector SpherePos = FVector(
			FMath::Cos(Theta) * FMath::Sin(Phi),
			FMath::Sin(Theta) * FMath::Sin(Phi),
			FMath::Cos(Phi)
		) * SphereRadius;

		Particle.Position = FVector3f(BaseLocation + SpherePos);
		Particle.Velocity = FVector3f::ZeroVector;
		Particle.Radius = ParticleRadius;
		Particle.Padding = 0.0f;

		TestParticles.Add(Particle);
	}
}

void UKawaiiFluidDummyComponent::UpdateAnimatedParticles(float DeltaTime)
{
	AnimationTime += DeltaTime * AnimationSpeed;

	AActor* Owner = GetOwner();
	const FVector BaseLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	if (DataMode == EKawaiiFluidDummyGenMode::Animated)
	{
		// 회전 애니메이션
		for (int32 i = 0; i < TestParticles.Num(); ++i)
		{
			FKawaiiRenderParticle& Particle = TestParticles[i];

			FVector LocalPos = FVector(Particle.Position) - BaseLocation;

			// Y축 회전
			FRotator Rotation(0.0f, AnimationTime * 50.0f, 0.0f);
			FVector RotatedPos = Rotation.RotateVector(LocalPos);

			Particle.Position = FVector3f(BaseLocation + RotatedPos);
		}
	}
	else if (DataMode == EKawaiiFluidDummyGenMode::Wave)
	{
		// Z축 파동
		for (int32 i = 0; i < TestParticles.Num(); ++i)
		{
			FKawaiiRenderParticle& Particle = TestParticles[i];

			FVector LocalPos = FVector(Particle.Position) - BaseLocation;

			// 원래 위치 저장
			static TArray<FVector> OriginalLocalPositions;
			if (OriginalLocalPositions.Num() != TestParticles.Num())
			{
				OriginalLocalPositions.SetNum(TestParticles.Num());
				for (int32 j = 0; j < TestParticles.Num(); ++j)
				{
					OriginalLocalPositions[j] = FVector(TestParticles[j].Position) - BaseLocation;
				}
			}

			float Wave = FMath::Sin(OriginalLocalPositions[i].X * WaveFrequency * 0.01f + AnimationTime) * WaveAmplitude;
			LocalPos.Z = OriginalLocalPositions[i].Z + Wave;

			Particle.Position = FVector3f(BaseLocation + LocalPos);
		}
	}
}

void UKawaiiFluidDummyComponent::RegenerateTestData()
{
	GenerateTestParticles();
	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent: Regenerated %d particles"), TestParticles.Num());
}

void UKawaiiFluidDummyComponent::ForceUpdateGPUBuffer()
{
	if (RenderResource.IsValid() && TestParticles.Num() > 0)
	{
		RenderResource->UpdateParticleData(TestParticles);
		UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent: GPU buffer updated (%d particles)"), TestParticles.Num());
	}
}

void UKawaiiFluidDummyComponent::SetTestPattern(EKawaiiFluidDummyGenMode NewMode)
{
	if (DataMode != NewMode)
	{
		DataMode = NewMode;
		RegenerateTestData();
	}
}

void UKawaiiFluidDummyComponent::SetParticleCount(int32 NewCount)
{
	NewCount = FMath::Clamp(NewCount, 1, 10000);
	if (ParticleCount != NewCount)
	{
		ParticleCount = NewCount;
		RegenerateTestData();
	}
}

void UKawaiiFluidDummyComponent::ToggleAnimation()
{
	bAnimationPaused = !bAnimationPaused;
	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidDummyComponent: Animation %s"), 
		bAnimationPaused ? TEXT("Paused") : TEXT("Resumed"));
}

bool UKawaiiFluidDummyComponent::IsFluidRenderResourceValid() const
{
	return RenderResource.IsValid() && RenderResource->IsValid();
}
