// Copyright KawaiiFluid Team. All Rights Reserved.
// FGPUSpawnManager - Thread-safe particle spawn queue manager

#include "GPU/Managers/GPUSpawnManager.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPUSpawnManager, Log, All);
DEFINE_LOG_CATEGORY(LogGPUSpawnManager);

//=============================================================================
// Constructor / Destructor
//=============================================================================

FGPUSpawnManager::FGPUSpawnManager()
	: bIsInitialized(false)
	, MaxParticleCapacity(0)
{
}

FGPUSpawnManager::~FGPUSpawnManager()
{
	Release();
}

//=============================================================================
// Lifecycle
//=============================================================================

void FGPUSpawnManager::Initialize(int32 InMaxParticleCount)
{
	if (InMaxParticleCount <= 0)
	{
		UE_LOG(LogGPUSpawnManager, Warning, TEXT("Initialize called with invalid particle count: %d"), InMaxParticleCount);
		return;
	}

	MaxParticleCapacity = InMaxParticleCount;
	bIsInitialized = true;

	UE_LOG(LogGPUSpawnManager, Log, TEXT("GPUSpawnManager initialized with capacity: %d"), MaxParticleCapacity);
}

void FGPUSpawnManager::Release()
{
	FScopeLock Lock(&SpawnLock);

	PendingSpawnRequests.Empty();
	ActiveSpawnRequests.Empty();
	bHasPendingSpawnRequests.store(false);
	NextParticleID.store(0);
	bIsInitialized = false;
	MaxParticleCapacity = 0;

	UE_LOG(LogGPUSpawnManager, Log, TEXT("GPUSpawnManager released"));
}

//=============================================================================
// Thread-Safe Public API
//=============================================================================

void FGPUSpawnManager::AddSpawnRequest(const FVector3f& Position, const FVector3f& Velocity, float Mass)
{
	FScopeLock Lock(&SpawnLock);

	FGPUSpawnRequest Request;
	Request.Position = Position;
	Request.Velocity = Velocity;
	Request.Mass = Mass;
	Request.Radius = DefaultSpawnRadius;

	PendingSpawnRequests.Add(Request);
	bHasPendingSpawnRequests.store(true);

	UE_LOG(LogGPUSpawnManager, Verbose, TEXT("AddSpawnRequest: Pos=(%.2f, %.2f, %.2f), Vel=(%.2f, %.2f, %.2f)"),
		Position.X, Position.Y, Position.Z, Velocity.X, Velocity.Y, Velocity.Z);
}

void FGPUSpawnManager::AddSpawnRequests(const TArray<FGPUSpawnRequest>& Requests)
{
	if (Requests.Num() == 0)
	{
		return;
	}

	FScopeLock Lock(&SpawnLock);

	PendingSpawnRequests.Append(Requests);
	bHasPendingSpawnRequests.store(true);

	UE_LOG(LogGPUSpawnManager, Verbose, TEXT("AddSpawnRequests: Added %d requests (total pending: %d)"),
		Requests.Num(), PendingSpawnRequests.Num());
}

void FGPUSpawnManager::ClearSpawnRequests()
{
	FScopeLock Lock(&SpawnLock);
	PendingSpawnRequests.Empty();
	bHasPendingSpawnRequests.store(false);
}

int32 FGPUSpawnManager::GetPendingSpawnCount() const
{
	FScopeLock Lock(&SpawnLock);
	return PendingSpawnRequests.Num();
}

//=============================================================================
// Render Thread API
//=============================================================================

void FGPUSpawnManager::SwapBuffers()
{
	FScopeLock Lock(&SpawnLock);

	// Move pending requests to active buffer
	ActiveSpawnRequests = MoveTemp(PendingSpawnRequests);
	PendingSpawnRequests.Empty();
	bHasPendingSpawnRequests.store(false);
}

void FGPUSpawnManager::AddSpawnParticlesPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferUAVRef ParticlesUAV,
	FRDGBufferUAVRef ParticleCounterUAV,
	int32 MaxParticleCount)
{
	if (ActiveSpawnRequests.Num() == 0)
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
		ActiveSpawnRequests.Num(),
		ActiveSpawnRequests.GetData(),
		ActiveSpawnRequests.Num() * sizeof(FGPUSpawnRequest),
		ERDGInitialDataFlags::None
	);

	FSpawnParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSpawnParticlesCS::FParameters>();
	PassParameters->SpawnRequests = GraphBuilder.CreateSRV(SpawnRequestBuffer);
	PassParameters->Particles = ParticlesUAV;
	PassParameters->ParticleCounter = ParticleCounterUAV;
	PassParameters->SpawnRequestCount = ActiveSpawnRequests.Num();
	PassParameters->MaxParticleCount = MaxParticleCount;
	PassParameters->NextParticleID = NextParticleID.load();
	PassParameters->DefaultRadius = DefaultSpawnRadius;
	PassParameters->DefaultMass = DefaultSpawnMass;

	const uint32 NumGroups = FMath::DivideAndRoundUp(ActiveSpawnRequests.Num(), FSpawnParticlesCS::ThreadGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::SpawnParticles(%d)", ActiveSpawnRequests.Num()),
		ComputeShader,
		PassParameters,
		FIntVector(NumGroups, 1, 1)
	);

	UE_LOG(LogGPUSpawnManager, Verbose, TEXT("SpawnParticlesPass: Spawning %d particles (NextID: %d)"),
		ActiveSpawnRequests.Num(), NextParticleID.load());
}

void FGPUSpawnManager::OnSpawnComplete(int32 SpawnedCount)
{
	if (SpawnedCount > 0)
	{
		NextParticleID.fetch_add(SpawnedCount);
	}
}
