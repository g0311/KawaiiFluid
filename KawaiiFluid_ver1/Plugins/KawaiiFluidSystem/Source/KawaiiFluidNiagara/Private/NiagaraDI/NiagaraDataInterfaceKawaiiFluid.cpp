// Copyright KawaiiFluid Team. All Rights Reserved.

#include "NiagaraDI/NiagaraDataInterfaceKawaiiFluid.h"
#include "Core/KawaiiRenderParticle.h"
#include "Components/KawaiiFluidSimulationComponent.h"
#include "NiagaraShader.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraTypes.h"
#include "ShaderParameterUtils.h"
#include "RHICommandList.h"

//========================================
// 함수 이름 정의
//========================================

const FName UNiagaraDataInterfaceKawaiiFluid::GetParticleCountName(TEXT("GetParticleCount"));
const FName UNiagaraDataInterfaceKawaiiFluid::GetParticlePositionName(TEXT("GetParticlePosition"));
const FName UNiagaraDataInterfaceKawaiiFluid::GetParticleVelocityName(TEXT("GetParticleVelocity"));
const FName UNiagaraDataInterfaceKawaiiFluid::GetParticleRadiusName(TEXT("GetParticleRadius"));

//========================================
// 생성자
//========================================

UNiagaraDataInterfaceKawaiiFluid::UNiagaraDataInterfaceKawaiiFluid(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAutoUpdate(true)
	, UpdateInterval(0.0f)
{
}

//========================================
// Niagara Type Registry 등록 (필수!)
//========================================

void UNiagaraDataInterfaceKawaiiFluid::PostInitProperties()
{
	Super::PostInitProperties();

	// CDO (Class Default Object)일 때만 Type Registry에 등록
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// AllowAnyVariable: 변수 타입으로 사용 가능
		// AllowParameter: User Parameter로 추가 가능
		ENiagaraTypeRegistryFlags Flags = 
			ENiagaraTypeRegistryFlags::AllowAnyVariable | 
			ENiagaraTypeRegistryFlags::AllowParameter;
		
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
		
		UE_LOG(LogTemp, Warning, TEXT("✅ UNiagaraDataInterfaceKawaiiFluid registered with Niagara Type Registry"));
	}
}

//========================================
// UPROPERTY 동기화
//========================================

bool UNiagaraDataInterfaceKawaiiFluid::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceKawaiiFluid* DestTyped = CastChecked<UNiagaraDataInterfaceKawaiiFluid>(Destination);
	DestTyped->SourceFluidActor = SourceFluidActor;
	DestTyped->bAutoUpdate = bAutoUpdate;
	DestTyped->UpdateInterval = UpdateInterval;

	return true;
}

//========================================
// 함수 시그니처 등록
//========================================

#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceKawaiiFluid::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	// 1. GetParticleCount
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetParticleCountName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("KawaiiFluid")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
		Sig.SetDescription(NSLOCTEXT("Niagara", "KawaiiFluid_GetParticleCount", "Returns the total number of fluid particles"));
		OutFunctions.Add(Sig);
	}

	// 2. GetParticlePosition
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetParticlePositionName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("KawaiiFluid")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.SetDescription(NSLOCTEXT("Niagara", "KawaiiFluid_GetParticlePosition", "Returns position of particle at given index"));
		OutFunctions.Add(Sig);
	}

	// 3. GetParticleVelocity
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetParticleVelocityName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("KawaiiFluid")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.SetDescription(NSLOCTEXT("Niagara", "KawaiiFluid_GetParticleVelocity", "Returns velocity of particle at given index"));
		OutFunctions.Add(Sig);
	}

	// 4. GetParticleRadius
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetParticleRadiusName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("KawaiiFluid")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Radius")));
		Sig.SetDescription(NSLOCTEXT("Niagara", "KawaiiFluid_GetParticleRadius", "Returns rendering radius for fluid particles"));
		OutFunctions.Add(Sig);
	}
}
#endif

//========================================
// 함수 유효성 검사 (컴파일 타임)
//========================================

void UNiagaraDataInterfaceKawaiiFluid::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	// 부모 클래스 검증 먼저 수행
	Super::ValidateFunction(Function, OutValidationErrors);

	// ⚠️ ValidateFunction은 컴파일 타임에 실행되므로
	// Runtime 값 (SourceDummyActor)을 확인할 수 없음!
	// 
	// 대신 런타임 에러는 InitPerInstanceData()에서 처리됨
	// 여기서는 함수 시그니처만 검증

	// 추가 검증이 필요하다면 함수 파라미터 타입 등만 확인
	// 예: GetParticlePosition의 Index가 Int인지 확인
}

//========================================
// VM 함수 바인딩
//========================================

void UNiagaraDataInterfaceKawaiiFluid::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, 
                                                               void* InstanceData, 
                                                               FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == GetParticleCountName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceKawaiiFluid::VMGetParticleCount);
	}
	else if (BindingInfo.Name == GetParticlePositionName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceKawaiiFluid::VMGetParticlePosition);
	}
	else if (BindingInfo.Name == GetParticleVelocityName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceKawaiiFluid::VMGetParticleVelocity);
	}
	else if (BindingInfo.Name == GetParticleRadiusName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceKawaiiFluid::VMGetParticleRadius);
	}
}

//========================================
// Per-Instance 데이터 관리
//========================================

bool UNiagaraDataInterfaceKawaiiFluid::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIKawaiiFluid_InstanceData* InstanceData = new (PerInstanceData) FNDIKawaiiFluid_InstanceData();

	// ✅ Runtime 검증: User Parameter 연결 확인
	if (SourceFluidActor.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("UNiagaraDataInterfaceKawaiiFluid: SourceFluidActor is not set! Please assign an Actor in User Parameters."));
		return true; // 초기화는 성공하지만 데이터 없음
	}

	// Actor에서 UKawaiiFluidSimulationComponent 찾기
	if (SourceFluidActor.IsValid())
	{
		AActor* Actor = SourceFluidActor.Get();
		if (Actor)
		{
			UKawaiiFluidSimulationComponent* SimComp = Actor->FindComponentByClass<UKawaiiFluidSimulationComponent>();
			if (SimComp)
			{
				InstanceData->SourceComponent = SimComp;

				// ✅ 초기 CachedParticleCount 설정 (Tick 전에!)
				const TArray<FFluidParticle>& Particles = SimComp->GetParticles();
				InstanceData->CachedParticleCount = Particles.Num();

				UE_LOG(LogTemp, Log, TEXT("Niagara DI: Found SimulationComponent on %s (Particles: %d)"),
					*Actor->GetName(), InstanceData->CachedParticleCount);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Niagara DI: Actor '%s' does not have UKawaiiFluidSimulationComponent!"),
					*Actor->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Niagara DI: SourceFluidActor is invalid (Actor deleted or not loaded)"));
		}
	}

	return true;
}

void UNiagaraDataInterfaceKawaiiFluid::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIKawaiiFluid_InstanceData* InstanceData = static_cast<FNDIKawaiiFluid_InstanceData*>(PerInstanceData);
	InstanceData->~FNDIKawaiiFluid_InstanceData();
}

//========================================
// 매 프레임 업데이트
//========================================

bool UNiagaraDataInterfaceKawaiiFluid::PerInstanceTick(void* PerInstanceData, 
                                                         FNiagaraSystemInstance* SystemInstance, 
                                                         float DeltaSeconds)
{
	FNDIKawaiiFluid_InstanceData* InstanceData = static_cast<FNDIKawaiiFluid_InstanceData*>(PerInstanceData);
	
	if (!bAutoUpdate)
	{
		return false;
	}

	// 업데이트 간격 체크
	InstanceData->LastUpdateTime += DeltaSeconds;
	if (UpdateInterval > 0.0f && InstanceData->LastUpdateTime < UpdateInterval)
	{
		return false;
	}
	InstanceData->LastUpdateTime = 0.0f;

	// Component 유효성 체크
	UKawaiiFluidSimulationComponent* SimComp = InstanceData->SourceComponent.Get();
	if (!SimComp)
	{
		return false;
	}

	// 파티클 데이터 가져오기
	const TArray<FFluidParticle>& Particles = SimComp->GetParticles();
	InstanceData->CachedParticleCount = Particles.Num();

	// 🔴 BREAKPOINT: PIE 실행 중에만 로그 출력
	#if !UE_BUILD_SHIPPING
	static bool bFirstTick = true;
	if (bFirstTick && Particles.Num() > 0)
	{
		// ✅ World가 Game World인지 확인 (PIE, Standalone 등)
		if (UWorld* World = SimComp->GetWorld())
		{
			if (World->IsGameWorld())
			{
				UE_LOG(LogTemp, Error, TEXT("🔴 BREAKPOINT: PerInstanceTick - CachedParticleCount=%d (PIE)"),
					InstanceData->CachedParticleCount);
				bFirstTick = false;
			}
		}
	}
	#endif

	if (Particles.Num() == 0)
	{
		return false;
	}

	return true;
}

//========================================
// GPU 버퍼 업데이트 (렌더 스레드)
//========================================

void UNiagaraDataInterfaceKawaiiFluid::UpdateGPUBuffers_RenderThread(FNDIKawaiiFluid_InstanceData* InstanceData,
                                                                       const TArray<FFluidParticle>& Particles,
                                                                       float Radius)
{
	int32 ParticleCount = Particles.Num();

	// FFluidParticle을 FKawaiiRenderParticle로 변환
	TArray<FKawaiiRenderParticle> RenderParticles;
	RenderParticles.Reserve(ParticleCount);

	for (const FFluidParticle& Particle : Particles)
	{
		FKawaiiRenderParticle RenderParticle;
		RenderParticle.Position = (FVector3f)Particle.Position;
		RenderParticle.Velocity = (FVector3f)Particle.Velocity;
		RenderParticle.Radius = Radius;
		RenderParticle.Padding = 0.0f;
		RenderParticles.Add(RenderParticle);
	}

	// 렌더 스레드로 전송
	ENQUEUE_RENDER_COMMAND(UpdateKawaiiFluidBuffers)(
		[InstanceData, RenderParticles, ParticleCount](FRHICommandListImmediate& RHICmdList)
		{
			// 버퍼 재할당이 필요한지 체크
			if (InstanceData->BufferCapacity < ParticleCount)
			{
				int32 NewCapacity = FMath::Max(ParticleCount, 1024);
				InstanceData->BufferCapacity = NewCapacity;

				// Particle Buffer 생성 (FKawaiiRenderParticle 크기 = 32 bytes)
				// UE 5.7 API: FRHIBufferCreateDesc 사용 (builder 패턴)
				FRHIBufferCreateDesc BufferDesc;
				BufferDesc.Size = NewCapacity * sizeof(FKawaiiRenderParticle);
				BufferDesc.Usage = BUF_ShaderResource | BUF_Dynamic;
				BufferDesc.DebugName = TEXT("KawaiiFluid_Particles");

				InstanceData->ParticleBuffer = RHICmdList.CreateBuffer(BufferDesc);

				// SRV 생성 (UE 5.7 API: FRHIViewDesc 사용)
				InstanceData->ParticleSRV = RHICmdList.CreateShaderResourceView(
					InstanceData->ParticleBuffer,
					FRHIViewDesc::CreateBufferSRV()
						.SetType(FRHIViewDesc::EBufferType::Typed)
						.SetFormat(PF_R32_FLOAT)
				);
			}

			// FKawaiiRenderParticle 직접 복사
			void* Data = RHICmdList.LockBuffer(InstanceData->ParticleBuffer, 0,
			                                    ParticleCount * sizeof(FKawaiiRenderParticle),
			                                    RLM_WriteOnly);
			FMemory::Memcpy(Data, RenderParticles.GetData(),
			                 ParticleCount * sizeof(FKawaiiRenderParticle));
			RHICmdList.UnlockBuffer(InstanceData->ParticleBuffer);
		}
	);
}

//========================================
// VM 함수 구현 (CPU 시뮬레이션용)
//========================================

void UNiagaraDataInterfaceKawaiiFluid::VMGetParticleCount(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIKawaiiFluid_InstanceData> InstanceData(Context);
	FNDIOutputParam<int32> OutCount(Context);

	int32 Count = InstanceData->CachedParticleCount;

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutCount.SetAndAdvance(Count);
	}

	// ✅ PIE 실행 중에만 로그 출력 (첫 호출 시)
	#if !UE_BUILD_SHIPPING
	static bool bFirstCall = true;
	if (bFirstCall && Count > 0)
	{
		// World 상태 확인 (InstanceData의 SourceComponent 사용)
		if (InstanceData->SourceComponent.IsValid())
		{
			if (UWorld* World = InstanceData->SourceComponent->GetWorld())
			{
				if (World->IsGameWorld())
				{
					UE_LOG(LogTemp, Warning, TEXT("🎯 VMGetParticleCount called: %d particles (PIE)"), Count);
					bFirstCall = false;
				}
			}
		}
	}
	#endif
}

void UNiagaraDataInterfaceKawaiiFluid::VMGetParticlePosition(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIKawaiiFluid_InstanceData> InstanceData(Context);
	FNDIInputParam<int32> InIndex(Context);
	FNDIOutputParam<FVector3f> OutPosition(Context);

	// SimulationComponent에서 파티클 데이터 가져오기
	UKawaiiFluidSimulationComponent* SimComp = InstanceData->SourceComponent.Get();
	if (!SimComp)
	{
		// Component 없으면 제로 반환
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			InIndex.GetAndAdvance();
			OutPosition.SetAndAdvance(FVector3f::ZeroVector);
		}
		return;
	}

	const TArray<FFluidParticle>& Particles = SimComp->GetParticles();

	// ✅ PIE 실행 중에만 로그 출력 (첫 호출 시)
	#if !UE_BUILD_SHIPPING
	static bool bFirstCall = true;
	if (bFirstCall && Particles.Num() > 0)
	{
		if (UWorld* World = SimComp->GetWorld())
		{
			if (World->IsGameWorld())
			{
				UE_LOG(LogTemp, Warning, TEXT("🎯 VMGetParticlePosition called: %d instances (PIE)"), Context.GetNumInstances());
				UE_LOG(LogTemp, Warning, TEXT("  → First Particle Position: (%f, %f, %f)"),
					Particles[0].Position.X, Particles[0].Position.Y, Particles[0].Position.Z);
				bFirstCall = false;
			}
		}
	}
	#endif

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 Index = InIndex.GetAndAdvance();
		if (Particles.IsValidIndex(Index))
		{
			OutPosition.SetAndAdvance((FVector3f)Particles[Index].Position);
		}
		else
		{
			OutPosition.SetAndAdvance(FVector3f::ZeroVector);
		}
	}
}

void UNiagaraDataInterfaceKawaiiFluid::VMGetParticleVelocity(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIKawaiiFluid_InstanceData> InstanceData(Context);
	FNDIInputParam<int32> InIndex(Context);
	FNDIOutputParam<FVector3f> OutVelocity(Context);

	// SimulationComponent에서 파티클 데이터 가져오기
	UKawaiiFluidSimulationComponent* SimComp = InstanceData->SourceComponent.Get();
	if (!SimComp)
	{
		// Component 없으면 제로 반환
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			InIndex.GetAndAdvance();
			OutVelocity.SetAndAdvance(FVector3f::ZeroVector);
		}
		return;
	}

	const TArray<FFluidParticle>& Particles = SimComp->GetParticles();

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		int32 Index = InIndex.GetAndAdvance();
		if (Particles.IsValidIndex(Index))
		{
			OutVelocity.SetAndAdvance((FVector3f)Particles[Index].Velocity);
		}
		else
		{
			OutVelocity.SetAndAdvance(FVector3f::ZeroVector);
		}
	}
}

void UNiagaraDataInterfaceKawaiiFluid::VMGetParticleRadius(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIKawaiiFluid_InstanceData> InstanceData(Context);
	FNDIOutputParam<float> OutRadius(Context);

	UKawaiiFluidSimulationComponent* SimComp = InstanceData->SourceComponent.Get();
	float Radius = SimComp ? SimComp->GetParticleRadius() : 5.0f;

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutRadius.SetAndAdvance(Radius);
	}
}

//========================================
// 기타 오버라이드
//========================================

bool UNiagaraDataInterfaceKawaiiFluid::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UNiagaraDataInterfaceKawaiiFluid* OtherTyped = CastChecked<const UNiagaraDataInterfaceKawaiiFluid>(Other);
	return SourceFluidActor == OtherTyped->SourceFluidActor &&
	       bAutoUpdate == OtherTyped->bAutoUpdate &&
	       FMath::IsNearlyEqual(UpdateInterval, OtherTyped->UpdateInterval);
}

void UNiagaraDataInterfaceKawaiiFluid::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, 
                                                                              void* PerInstanceData, 
                                                                              const FNiagaraSystemInstanceID& SystemInstance)
{
	// 렌더 스레드로 인스턴스 데이터 복사
	FNDIKawaiiFluid_InstanceData* SourceData = static_cast<FNDIKawaiiFluid_InstanceData*>(PerInstanceData);
	FNDIKawaiiFluid_InstanceData* DestData = new (DataForRenderThread) FNDIKawaiiFluid_InstanceData();
	
	*DestData = *SourceData;
}

//========================================
// GPU 함수 HLSL 생성 (에디터 전용)
//========================================

#if WITH_EDITORONLY_DATA

void UNiagaraDataInterfaceKawaiiFluid::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, 
                                                                    FString& OutHLSL)
{
	OutHLSL += TEXT("Buffer<float4> {ParameterName}_ParticleBuffer;\n");
	OutHLSL += TEXT("int {ParameterName}_ParticleCount;\n");
}

bool UNiagaraDataInterfaceKawaiiFluid::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, 
                                                         const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, 
                                                         int FunctionInstanceIndex, 
                                                         FString& OutHLSL)
{
	if (FunctionInfo.DefinitionName == GetParticleCountName)
	{
		OutHLSL += FString::Printf(TEXT("void %s(out int Count) { Count = {ParameterName}_ParticleCount; }\n"), 
		                            *FunctionInfo.InstanceName);
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetParticlePositionName)
	{
		// FKawaiiRenderParticle = 32 bytes = float4 × 2
		// float4[0] = Position.xyz + Velocity.x
		// float4[1] = Velocity.yz + Radius + Padding
		OutHLSL += FString::Printf(TEXT("void %s(int Index, out float3 Position) {\n"), 
		                            *FunctionInfo.InstanceName);
		OutHLSL += TEXT("    float4 Data0 = {ParameterName}_ParticleBuffer[Index * 2 + 0];\n");
		OutHLSL += TEXT("    Position = Data0.xyz;\n");
		OutHLSL += TEXT("}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetParticleVelocityName)
	{
		// Velocity는 Data0.w + Data1.xy
		OutHLSL += FString::Printf(TEXT("void %s(int Index, out float3 Velocity) {\n"), 
		                            *FunctionInfo.InstanceName);
		OutHLSL += TEXT("    float4 Data0 = {ParameterName}_ParticleBuffer[Index * 2 + 0];\n");
		OutHLSL += TEXT("    float4 Data1 = {ParameterName}_ParticleBuffer[Index * 2 + 1];\n");
		OutHLSL += TEXT("    Velocity = float3(Data0.w, Data1.xy);\n");
		OutHLSL += TEXT("}\n");
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetParticleRadiusName)
	{
		// Radius는 Data1.z (인덱스 0 기준)
		OutHLSL += FString::Printf(TEXT("void %s(out float Radius) {\n"), 
		                            *FunctionInfo.InstanceName);
		OutHLSL += TEXT("    float4 Data1 = {ParameterName}_ParticleBuffer[0 * 2 + 1];\n");
		OutHLSL += TEXT("    Radius = Data1.z;\n");
		OutHLSL += TEXT("}\n");
		return true;
	}

	return false;
}

bool UNiagaraDataInterfaceKawaiiFluid::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
	{
		return false;
	}

	// 버전 업데이트 (구조 변경)
	InVisitor->UpdatePOD(TEXT("KawaiiFluidNiagaraDI"), (int32)2);
	
	return true;
}

#endif // WITH_EDITORONLY_DATA
