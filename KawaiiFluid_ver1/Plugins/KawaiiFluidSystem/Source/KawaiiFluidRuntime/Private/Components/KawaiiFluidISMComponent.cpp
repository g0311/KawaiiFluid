// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/KawaiiFluidISMComponent.h"
#include "Interfaces/IKawaiiFluidDataProvider.h"
#include "Core/FluidParticle.h"

UKawaiiFluidISMComponent::UKawaiiFluidISMComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UKawaiiFluidISMComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeISM();

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidISMComponent: Initialized (Mesh: %s, MaxParticles: %d)"),
		ISMComponent && ISMComponent->GetStaticMesh() ? *ISMComponent->GetStaticMesh()->GetName() : TEXT("None"),
		MaxRenderParticles);
}

void UKawaiiFluidISMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ISMComponent)
	{
		ISMComponent->ClearInstances();
	}

	Super::EndPlay(EndPlayReason);
}

void UKawaiiFluidISMComponent::UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime)
{
	if (!bEnableRendering || !ISMComponent || !DataProvider)
	{
		return;
	}

	// DataProvider에서 시뮬레이션 데이터 가져오기
	const TArray<FFluidParticle>& SimParticles = DataProvider->GetParticles();
	
	if (SimParticles.Num() == 0)
	{
		ISMComponent->ClearInstances();
		return;
	}

	// 렌더링할 파티클 수 제한
	int32 NumInstances = FMath::Min(SimParticles.Num(), MaxRenderParticles);

	// 기존 인스턴스 클리어 및 메모리 미리 할당
	ISMComponent->ClearInstances();
	ISMComponent->PreAllocateInstancesMemory(NumInstances);

	// 파티클 반경
	float ParticleRadius = DataProvider->GetParticleRenderRadius();
	float ScaleFactor = (ParticleRadius / 50.0f) * ParticleScale; // 기본 Sphere는 반지름 50cm
	FVector ScaleVec(ScaleFactor, ScaleFactor, ScaleFactor);

	// 각 파티클을 인스턴스로 추가
	for (int32 i = 0; i < NumInstances; ++i)
	{
		const FFluidParticle& Particle = SimParticles[i];

		// Transform 생성
		FTransform InstanceTransform;
		InstanceTransform.SetLocation(Particle.Position);
		InstanceTransform.SetScale3D(ScaleVec);

		// 속도 기반 회전 (선택적)
		if (bRotateByVelocity && !Particle.Velocity.IsNearlyZero())
		{
			FRotator Rotation = Particle.Velocity.ToOrientationRotator();
			InstanceTransform.SetRotation(Rotation.Quaternion());
		}

		// 인스턴스 추가
		ISMComponent->AddInstance(InstanceTransform, false);

		// 속도 기반 컬러 (선택적)
		if (bColorByVelocity)
		{
			float VelocityMagnitude = Particle.Velocity.Size();
			float T = FMath::Clamp(VelocityMagnitude / MaxVelocityForColor, 0.0f, 1.0f);
			FLinearColor Color = FMath::Lerp(MinVelocityColor, MaxVelocityColor, T);
			
			// Custom Data로 색상 전달 (머티리얼에서 사용 가능)
			ISMComponent->SetCustomDataValue(i, 0, Color.R, false);
			ISMComponent->SetCustomDataValue(i, 1, Color.G, false);
			ISMComponent->SetCustomDataValue(i, 2, Color.B, false);
			ISMComponent->SetCustomDataValue(i, 3, Color.A, false);
		}
	}

	// 렌더 상태 업데이트
	ISMComponent->MarkRenderStateDirty();
}

void UKawaiiFluidISMComponent::InitializeISM()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidISMComponent: No owner actor"));
		return;
	}

	// ISM 컴포넌트 생성
	ISMComponent = NewObject<UInstancedStaticMeshComponent>(
		Owner,
		UInstancedStaticMeshComponent::StaticClass(),
		TEXT("FluidISM")
	);

	if (!ISMComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidISMComponent: Failed to create ISM component"));
		return;
	}

	// 컴포넌트 설정
	ISMComponent->SetupAttachment(GetOwner()->GetRootComponent());
	
	// ✨ 절대 좌표 사용 (DummyComponent와 동일)
	ISMComponent->SetAbsolute(true, true, true);
	
	ISMComponent->RegisterComponent();
	ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMComponent->SetCastShadow(bCastShadow);
	ISMComponent->SetCullDistances(0, CullDistance);

	// 메시 설정
	if (!ParticleMesh)
	{
		ParticleMesh = GetDefaultParticleMesh();
	}

	if (ParticleMesh)
	{
		ISMComponent->SetStaticMesh(ParticleMesh);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMComponent: No particle mesh available"));
	}

	// 머티리얼 설정
	if (ParticleMaterial)
	{
		ISMComponent->SetMaterial(0, ParticleMaterial);
	}
	else if (!ParticleMesh)
	{
		// 메시가 없으면 기본 머티리얼 로드
		UMaterialInterface* DefaultMaterial = GetDefaultParticleMaterial();
		if (DefaultMaterial)
		{
			ISMComponent->SetMaterial(0, DefaultMaterial);
		}
	}

	// Custom Data 설정 (색상 변화용)
	if (bColorByVelocity)
	{
		ISMComponent->NumCustomDataFloats = 4; // RGBA
	}

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidISMComponent: ISM component initialized"));
}

UStaticMesh* UKawaiiFluidISMComponent::GetDefaultParticleMesh()
{
	// 엔진 기본 Sphere 메시 로드
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere")
	);

	if (!SphereMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMComponent: Failed to load default sphere mesh"));
	}

	return SphereMesh;
}

UMaterialInterface* UKawaiiFluidISMComponent::GetDefaultParticleMaterial()
{
	// 엔진 기본 머티리얼 로드
	UMaterialInterface* DefaultMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
	);

	if (!DefaultMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMComponent: Failed to load default material"));
	}

	return DefaultMaterial;
}

