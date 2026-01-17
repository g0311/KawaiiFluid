// Copyright KawaiiFluid Team. All Rights Reserved.
// FGPUSpawnManager - Thread-safe particle spawn queue manager

#include "GPU/Managers/GPUSpawnManager.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"

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
// ID-Based Despawn API
//=============================================================================

void FGPUSpawnManager::AddDespawnByIDRequest(int32 ParticleID)
{
	FScopeLock Lock(&DespawnByIDLock);
	PendingDespawnByIDs.Add(ParticleID);
	bHasPendingDespawnByIDRequests.store(true);
}

void FGPUSpawnManager::AddDespawnByIDRequests(const TArray<int32>& ParticleIDs)
{
	if (ParticleIDs.Num() == 0)
	{
		return;
	}

	FScopeLock Lock(&DespawnByIDLock);
	PendingDespawnByIDs.Append(ParticleIDs);
	bHasPendingDespawnByIDRequests.store(true);

	UE_LOG(LogGPUSpawnManager, Verbose, TEXT("AddDespawnByIDRequests: Added %d IDs (total pending: %d)"),
		ParticleIDs.Num(), PendingDespawnByIDs.Num());
}

void FGPUSpawnManager::SwapDespawnByIDBuffers()
{
	FScopeLock Lock(&DespawnByIDLock);
	ActiveDespawnByIDs = MoveTemp(PendingDespawnByIDs);
	PendingDespawnByIDs.Empty();
	bHasPendingDespawnByIDRequests.store(false);

	// Sort for binary search optimization in shader
	if (ActiveDespawnByIDs.Num() > 0)
	{
		ActiveDespawnByIDs.Sort();
		UE_LOG(LogGPUSpawnManager, Verbose, TEXT("SwapDespawnByIDBuffers: %d IDs ready for processing"),
			ActiveDespawnByIDs.Num());
	}
}

int32 FGPUSpawnManager::GetPendingDespawnByIDCount() const
{
	FScopeLock Lock(&DespawnByIDLock);
	return PendingDespawnByIDs.Num();
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

void FGPUSpawnManager::AddDespawnByIDPass(FRDGBuilder& GraphBuilder, FRDGBufferRef& InOutParticleBuffer,
	int32& InOutParticleCount)
{
	if (ActiveDespawnByIDs.Num() == 0)
	{
		return;
	}

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FMarkDespawnByIDCS> MarkDespawnByIDCS(ShaderMap);

	// Create sorted ID buffer for GPU binary search
	FRDGBufferRef DespawnIDsBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("GPUFluidDespawnByIDs"),
		sizeof(int32),
		ActiveDespawnByIDs.Num(),
		ActiveDespawnByIDs.GetData(),
		ActiveDespawnByIDs.Num() * sizeof(int32),
		ERDGInitialDataFlags::None
	);

	FRDGBufferRef AliveMaskBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("GPUFluidOutAliveMaskByID"),
		sizeof(uint32),
		InOutParticleCount,
		nullptr,
		0,
		ERDGInitialDataFlags::None
	);

	// Mark particles for removal by ID matching (binary search)
	FMarkDespawnByIDCS::FParameters* MarkPassParameters = GraphBuilder.AllocParameters<FMarkDespawnByIDCS::FParameters>();
	MarkPassParameters->DespawnIDs = GraphBuilder.CreateSRV(DespawnIDsBuffer);
	MarkPassParameters->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
	MarkPassParameters->OutAliveMask = GraphBuilder.CreateUAV(AliveMaskBuffer);
	MarkPassParameters->DespawnIDCount = ActiveDespawnByIDs.Num();
	MarkPassParameters->ParticleCount = InOutParticleCount;

	const uint32 MarkPassNumGroups = FMath::DivideAndRoundUp(InOutParticleCount, FMarkDespawnByIDCS::ThreadGroupSize);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::DespawnByID_Mark(%d IDs)", ActiveDespawnByIDs.Num()),
		MarkDespawnByIDCS,
		MarkPassParameters,
		FIntVector(MarkPassNumGroups, 1, 1)
	);

	// === Stream Compaction Pipeline (reuse existing shaders) ===

	FRDGBufferRef PrefixSumsBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("PrefixSumsByID"),
		sizeof(uint32),
		InOutParticleCount,
		nullptr,
		0,
		ERDGInitialDataFlags::None
	);

	FRDGBufferRef BlockSumsBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("BlockSumsByID"),
		sizeof(uint32),
		FMath::DivideAndRoundUp(InOutParticleCount, FPrefixSumBlockCS_RDG::ThreadGroupSize),
		nullptr,
		0,
		ERDGInitialDataFlags::None
	);

	// Block-wise prefix sum
	TShaderMapRef<FPrefixSumBlockCS_RDG> PrefixSumBlock(ShaderMap);
	FPrefixSumBlockCS_RDG::FParameters* PrefixSumBlockParameters = GraphBuilder.AllocParameters<FPrefixSumBlockCS_RDG::FParameters>();
	PrefixSumBlockParameters->MarkedFlags = GraphBuilder.CreateSRV(AliveMaskBuffer);
	PrefixSumBlockParameters->PrefixSums = GraphBuilder.CreateUAV(PrefixSumsBuffer);
	PrefixSumBlockParameters->BlockSums = GraphBuilder.CreateUAV(BlockSumsBuffer);
	PrefixSumBlockParameters->ElementCount = InOutParticleCount;

	const int32 PrefixSumBlockNumGroups = FMath::DivideAndRoundUp(InOutParticleCount, FPrefixSumBlockCS_RDG::ThreadGroupSize);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::DespawnByID_PrefixSumBlock"),
		PrefixSumBlock,
		PrefixSumBlockParameters,
		FIntVector(PrefixSumBlockNumGroups, 1, 1)
	);

	// Scan block sums
	TShaderMapRef<FScanBlockSumsCS_RDG> ScanBlockSums(ShaderMap);
	FScanBlockSumsCS_RDG::FParameters* ScanBlockSumsParameters = GraphBuilder.AllocParameters<FScanBlockSumsCS_RDG::FParameters>();
	ScanBlockSumsParameters->BlockSums = GraphBuilder.CreateUAV(BlockSumsBuffer);
	ScanBlockSumsParameters->BlockCount = PrefixSumBlockNumGroups;

	const int32 ScanBlockSumsNumGroups = FMath::DivideAndRoundUp(PrefixSumBlockNumGroups, FScanBlockSumsCS_RDG::ThreadGroupSize);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::DespawnByID_ScanBlockSums"),
		ScanBlockSums,
		ScanBlockSumsParameters,
		FIntVector(ScanBlockSumsNumGroups, 1, 1)
	);

	// Add block offsets
	TShaderMapRef<FAddBlockOffsetsCS_RDG> AddBlockOffsets(ShaderMap);
	FAddBlockOffsetsCS_RDG::FParameters* AddBlockOffsetsParameters = GraphBuilder.AllocParameters<FAddBlockOffsetsCS_RDG::FParameters>();
	AddBlockOffsetsParameters->PrefixSums = GraphBuilder.CreateUAV(PrefixSumsBuffer);
	AddBlockOffsetsParameters->BlockSums = GraphBuilder.CreateUAV(BlockSumsBuffer);
	AddBlockOffsetsParameters->ElementCount = InOutParticleCount;

	const int32 AddBlockOffsetsNumGroups = FMath::DivideAndRoundUp(InOutParticleCount, FPrefixSumBlockCS_RDG::ThreadGroupSize);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::DespawnByID_AddBlockOffsets"),
		AddBlockOffsets,
		AddBlockOffsetsParameters,
		FIntVector(AddBlockOffsetsNumGroups, 1, 1)
	);

	FRDGBufferRef CompactedParticlesBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("CompactedParticlesByID"),
		sizeof(FGPUFluidParticle),
		InOutParticleCount,
		nullptr,
		0,
		ERDGInitialDataFlags::None
	);

	// Compact particles
	TShaderMapRef<FCompactParticlesCS_RDG> Compact(ShaderMap);
	FCompactParticlesCS_RDG::FParameters* CompactParameters = GraphBuilder.AllocParameters<FCompactParticlesCS_RDG::FParameters>();
	CompactParameters->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
	CompactParameters->MarkedFlags = GraphBuilder.CreateSRV(AliveMaskBuffer);
	CompactParameters->PrefixSums = GraphBuilder.CreateSRV(PrefixSumsBuffer);
	CompactParameters->CompactedParticles = GraphBuilder.CreateUAV(CompactedParticlesBuffer);
	CompactParameters->ParticleCount = InOutParticleCount;

	const int32 CompactCSNumGroups = FMath::DivideAndRoundUp(InOutParticleCount, FPrefixSumBlockCS_RDG::ThreadGroupSize);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::DespawnByID_Compact"),
		Compact,
		CompactParameters,
		FIntVector(CompactCSNumGroups, 1, 1)
	);

	// Update buffer reference
	InOutParticleBuffer = CompactedParticlesBuffer;

	FRDGBufferRef TotalCountBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("DespawnByID.TotalCount")
	);

	// Write total count for async readback
	TShaderMapRef<FWriteTotalCountCS_RDG> WriteTotalCount(ShaderMap);
	FWriteTotalCountCS_RDG::FParameters* WriteTotalCountParameters = GraphBuilder.AllocParameters<FWriteTotalCountCS_RDG::FParameters>();
	WriteTotalCountParameters->PrefixSums = GraphBuilder.CreateSRV(PrefixSumsBuffer);
	WriteTotalCountParameters->MarkedFlags = GraphBuilder.CreateSRV(AliveMaskBuffer);
	WriteTotalCountParameters->OutTotalCount = GraphBuilder.CreateUAV(TotalCountBuffer);
	WriteTotalCountParameters->ParticleCount = InOutParticleCount;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GPUFluid::DespawnByID_WriteTotalCount"),
		WriteTotalCount,
		WriteTotalCountParameters,
		FIntVector(1, 1, 1)
	);

	if (!ParticleCountReadback)
	{
		ParticleCountReadback = new FRHIGPUBufferReadback(TEXT("FluidParticleCountReadback"));
	}
	AddEnqueueCopyPass(GraphBuilder, ParticleCountReadback, TotalCountBuffer, 0);
	bDespawnPassExecuted = true;

	UE_LOG(LogGPUSpawnManager, Log, TEXT("AddDespawnByIDPass: Processing %d IDs, %d particles"),
		ActiveDespawnByIDs.Num(), InOutParticleCount);

	// Clear active IDs after processing
	ActiveDespawnByIDs.Empty();
}

int32 FGPUSpawnManager::ProcessAsyncReadback()
{
	if (ParticleCountReadback && ParticleCountReadback->IsReady())
	{
		uint32* BufferData = static_cast<uint32*>(ParticleCountReadback->Lock(sizeof(uint32)));
		int32 DeadCount = static_cast<int32>(BufferData[0]);

		ParticleCountReadback->Unlock();

		return DeadCount;
	}

	return -1;
}

void FGPUSpawnManager::OnSpawnComplete(int32 SpawnedCount)
{
	if (SpawnedCount > 0)
	{
		NextParticleID.fetch_add(SpawnedCount);
	}
}
