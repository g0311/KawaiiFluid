// Copyright KawaiiFluid Team. All Rights Reserved.
// GPUFluidSimulator - Particle Spawn System Functions
// Public API delegates to FGPUSpawnManager for thread-safe spawn queue management

#include "GPU/GPUFluidSimulator.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPUFluidSimulator, Log, All);

//=============================================================================
// GPU Particle Spawning (Thread-Safe)
// Public API delegates to FGPUSpawnManager
// CPU sends spawn requests, GPU creates particles via atomic counter
// This eliminates race conditions between game thread and render thread
//=============================================================================

void FGPUFluidSimulator::AddSpawnRequest(const FVector3f& Position, const FVector3f& Velocity, float Mass)
{
	if (SpawnManager.IsValid())
	{
		SpawnManager->AddSpawnRequest(Position, Velocity, Mass);
	}
}

void FGPUFluidSimulator::AddSpawnRequests(const TArray<FGPUSpawnRequest>& Requests)
{
	if (SpawnManager.IsValid())
	{
		SpawnManager->AddSpawnRequests(Requests);
	}
}

void FGPUFluidSimulator::ClearSpawnRequests()
{
	if (SpawnManager.IsValid())
	{
		SpawnManager->ClearSpawnRequests();
	}
}

int32 FGPUFluidSimulator::GetPendingSpawnCount() const
{
	if (SpawnManager.IsValid())
	{
		return SpawnManager->GetPendingSpawnCount();
	}
	return 0;
}

void FGPUFluidSimulator::AddSpawnParticlesPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferUAVRef ParticlesUAV,
	FRDGBufferUAVRef ParticleCounterUAV,
	const TArray<FGPUSpawnRequest>& SpawnRequests)
{
	if (SpawnRequests.Num() == 0 || !SpawnManager.IsValid())
	{
		return;
	}

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FSpawnParticlesCS> ComputeShader(ShaderMap);

	// Create spawn request buffer
	// IMPORTANT: Do NOT use NoCopy - SpawnRequests is temporary data that may be
	// invalidated before RDG pass executes. RDG must copy the data.
	FRDGBufferRef SpawnRequestBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("GPUFluidSpawnRequests"),
		sizeof(FGPUSpawnRequest),
		SpawnRequests.Num(),
		SpawnRequests.GetData(),
		SpawnRequests.Num() * sizeof(FGPUSpawnRequest),
		ERDGInitialDataFlags::None
	);

	FSpawnParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSpawnParticlesCS::FParameters>();
	PassParameters->SpawnRequests = GraphBuilder.CreateSRV(SpawnRequestBuffer);
	PassParameters->Particles = ParticlesUAV;
	PassParameters->ParticleCounter = ParticleCounterUAV;
	PassParameters->SpawnRequestCount = SpawnRequests.Num();
	PassParameters->MaxParticleCount = MaxParticleCount;
	PassParameters->NextParticleID = SpawnManager->GetNextParticleID();
	PassParameters->DefaultRadius = SpawnManager->GetDefaultRadius();
	PassParameters->DefaultMass = SpawnManager->GetDefaultMass();

	const uint32 NumGroups = FMath::DivideAndRoundUp(SpawnRequests.Num(), FSpawnParticlesCS::ThreadGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::SpawnParticles(%d)", SpawnRequests.Num()),
		ComputeShader,
		PassParameters,
		FIntVector(NumGroups, 1, 1)
	);

	// Update next particle ID via SpawnManager
	SpawnManager->OnSpawnComplete(SpawnRequests.Num());

	//UE_LOG(LogGPUFluidSimulator, Log, TEXT("SpawnParticlesPass: Spawned %d particles (NextID: %d)"), SpawnRequests.Num(), SpawnManager->GetNextParticleID());
}

//=============================================================================
// FGPUFluidSimulationTask Implementation
//=============================================================================

void FGPUFluidSimulationTask::Execute(
	FGPUFluidSimulator* Simulator,
	const FGPUFluidSimulationParams& Params,
	int32 NumSubsteps)
{
	if (!Simulator || !Simulator->IsReady())
	{
		return;
	}

	FGPUFluidSimulationParams SubstepParams = Params;
	SubstepParams.DeltaTime = Params.DeltaTime / FMath::Max(1, NumSubsteps);
	SubstepParams.DeltaTimeSq = SubstepParams.DeltaTime * SubstepParams.DeltaTime;
	SubstepParams.TotalSubsteps = NumSubsteps;

	for (int32 i = 0; i < NumSubsteps; ++i)
	{
		SubstepParams.SubstepIndex = i;
		Simulator->SimulateSubstep(SubstepParams);
	}
}
