// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUSpawnManager - Thread-safe particle spawn queue manager

#include "GPU/Managers/GPUSpawnManager.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "GPU/GPUIndirectDispatchUtils.h"
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

/**
 * @brief Initialize the spawn manager with max particle capacity.
 * @param InMaxParticleCount Maximum particle capacity.
 */
void FGPUSpawnManager::Initialize(int32 InMaxParticleCount)
{
	if (InMaxParticleCount <= 0)
	{
		UE_LOG(LogGPUSpawnManager, Warning, TEXT("Initialize called with invalid particle count: %d"), InMaxParticleCount);
		return;
	}

	MaxParticleCapacity = InMaxParticleCount;
	bIsInitialized = true;

	// Initialize source counter cache
	CachedSourceCounts.SetNumZeroed(EGPUParticleSource::MaxSourceCount);

	// Initialize per-source emitter max counts
	EmitterMaxCountsCPU.SetNumZeroed(EGPUParticleSource::MaxSourceCount);
	ActiveEmitterMaxCount = 0;
	bEmitterMaxCountsDirty = false;

	// Create source counter readback ring buffer
	SourceCounterReadbacks.SetNum(SourceCounterRingBufferSize);
	for (int32 i = 0; i < SourceCounterRingBufferSize; ++i)
	{
		SourceCounterReadbacks[i] = new FRHIGPUBufferReadback(*FString::Printf(TEXT("SourceCounterReadback_%d"), i));
	}
	SourceCounterWriteIndex = 0;
	SourceCounterReadIndex = 0;
	SourceCounterPendingCount = 0;

	UE_LOG(LogGPUSpawnManager, Log, TEXT("GPUSpawnManager initialized with capacity: %d, MaxSourceCount: %d"),
		MaxParticleCapacity, EGPUParticleSource::MaxSourceCount);
}

/**
 * @brief Release all resources.
 */
void FGPUSpawnManager::Release()
{
	{
		FScopeLock Lock(&SpawnLock);
		PendingSpawnRequests.Empty();
		ActiveSpawnRequests.Empty();
		bHasPendingSpawnRequests.store(false);
	}

	{
		FScopeLock Lock(&GPUDespawnLock);
		PendingGPUBrushDespawns.Empty();
		ActiveGPUBrushDespawns.Empty();
		PendingGPUSourceDespawns.Empty();
		ActiveGPUSourceDespawns.Empty();
		bHasPendingGPUDespawnRequests.store(false);
	}

	// Release source counter resources
	{
		FScopeLock Lock(&SourceCountLock);
		SourceCounterBuffer.SafeRelease();
		for (FRHIGPUBufferReadback* Readback : SourceCounterReadbacks)
		{
			if (Readback)
			{
				delete Readback;
			}
		}
		SourceCounterReadbacks.Empty();
		SourceCounterWriteIndex = 0;
		SourceCounterReadIndex = 0;
		SourceCounterPendingCount = 0;
		CachedSourceCounts.Empty();
	}

	// Release stream compaction buffers
	PersistentAliveMaskBuffer.SafeRelease();
	PersistentPrefixSumsBuffer.SafeRelease();
	PersistentBlockSumsBuffer.SafeRelease();
	PersistentCompactedBuffer[0].SafeRelease();
	PersistentCompactedBuffer[1].SafeRelease();
	CompactedBufferIndex = 0;
	StreamCompactionCapacity = 0;

	// Release oldest despawn histogram buffers
	PersistentIDHistogramBuffer.SafeRelease();
	PersistentOldestThresholdBuffer.SafeRelease();
	PersistentBoundaryCounterBuffer.SafeRelease();

	// Release per-source recycle buffers
	PersistentEmitterMaxCountsBuffer.SafeRelease();
	PersistentPerSourceExcessBuffer.SafeRelease();
	EmitterMaxCountsCPU.Empty();
	ActiveEmitterMaxCount = 0;
	bEmitterMaxCountsDirty = false;

	NextParticleID.store(0);
	bIsInitialized = false;
	MaxParticleCapacity = 0;

	UE_LOG(LogGPUSpawnManager, Log, TEXT("GPUSpawnManager released (all despawn state cleared)"));
}

//=============================================================================
// Thread-Safe Public API
//=============================================================================

/**
 * @brief Add a spawn request (thread-safe).
 * @param Position World position to spawn at.
 * @param Velocity Initial velocity.
 * @param Mass Particle mass (0 = use default).
 */
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

/**
 * @brief Add multiple spawn requests at once (thread-safe, more efficient).
 * @param Requests Array of spawn requests.
 */
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

/**
 * @brief Clear all pending spawn requests.
 */
void FGPUSpawnManager::ClearSpawnRequests()
{
	FScopeLock Lock(&SpawnLock);
	PendingSpawnRequests.Empty();
	bHasPendingSpawnRequests.store(false);
}

/**
 * @brief Get number of pending spawn requests (thread-safe).
 * @return Number of requests.
 */
int32 FGPUSpawnManager::GetPendingSpawnCount() const
{
	FScopeLock Lock(&SpawnLock);
	return PendingSpawnRequests.Num();
}

/**
 * @brief Cancel pending spawn requests for a specific SourceID (thread-safe).
 * @param SourceID Source ID to cancel spawns for.
 * @return Number of cancelled spawn requests.
 */
int32 FGPUSpawnManager::CancelPendingSpawnsForSource(int32 SourceID)
{
	FScopeLock Lock(&SpawnLock);

	const int32 OriginalCount = PendingSpawnRequests.Num();

	// Remove all pending spawn requests with matching SourceID
	PendingSpawnRequests.RemoveAll([SourceID](const FGPUSpawnRequest& Request)
	{
		return Request.SourceID == SourceID;
	});

	const int32 RemovedCount = OriginalCount - PendingSpawnRequests.Num();

	if (PendingSpawnRequests.Num() == 0)
	{
		bHasPendingSpawnRequests.store(false);
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogGPUSpawnManager, Log, TEXT("CancelPendingSpawnsForSource: Cancelled %d pending spawns for SourceID=%d"),
			RemovedCount, SourceID);
	}

	return RemovedCount;
}

//=============================================================================
// GPU-Driven Despawn API
//=============================================================================

/**
 * @brief Add a brush despawn request - removes particles within radius (thread-safe).
 * @param Center World position of brush center.
 * @param Radius Brush radius.
 */
void FGPUSpawnManager::AddGPUDespawnBrushRequest(const FVector3f& Center, float Radius)
{
	FScopeLock Lock(&GPUDespawnLock);
	PendingGPUBrushDespawns.Emplace(Center, Radius);
	bHasPendingGPUDespawnRequests.store(true);
}

/**
 * @brief Add a source despawn request - removes all particles with matching SourceID (thread-safe).
 * @param SourceID Source component ID to despawn.
 */
void FGPUSpawnManager::AddGPUDespawnSourceRequest(int32 SourceID)
{
	if (SourceID < 0 || SourceID >= EGPUParticleSource::MaxSourceCount)
	{
		return;
	}

	FScopeLock Lock(&GPUDespawnLock);

	// Deduplicate SourceIDs within same frame
	if (!PendingGPUSourceDespawns.Contains(SourceID))
	{
		PendingGPUSourceDespawns.Add(SourceID);
	}
	bHasPendingGPUDespawnRequests.store(true);
}

/**
 * @brief Set per-source emitter max particle count for GPU-driven recycling (thread-safe).
 * @param SourceID Source component ID (0 to MaxSourceCount-1).
 * @param MaxCount Max particles for this source (0 = no limit / disable).
 */
void FGPUSpawnManager::SetSourceEmitterMax(int32 SourceID, int32 MaxCount)
{
	if (SourceID < 0 || SourceID >= EGPUParticleSource::MaxSourceCount)
	{
		return;
	}

	FScopeLock Lock(&GPUDespawnLock);

	if (EmitterMaxCountsCPU.Num() == 0)
	{
		EmitterMaxCountsCPU.SetNumZeroed(EGPUParticleSource::MaxSourceCount);
	}

	const int32 OldValue = EmitterMaxCountsCPU[SourceID];
	if (OldValue == MaxCount)
	{
		return;
	}

	EmitterMaxCountsCPU[SourceID] = MaxCount;
	bEmitterMaxCountsDirty = true;

	// Track active count
	if (OldValue == 0 && MaxCount > 0)
	{
		++ActiveEmitterMaxCount;
	}
	else if (OldValue > 0 && MaxCount == 0)
	{
		--ActiveEmitterMaxCount;
	}

	UE_LOG(LogGPUSpawnManager, Verbose, TEXT("SetSourceEmitterMax: SourceID=%d, Max=%d (active=%d)"),
		SourceID, MaxCount, ActiveEmitterMaxCount);
}

/**
 * @brief Swap pending GPU despawn requests to active buffers.
 * @return true if any despawn requests were swapped.
 */
bool FGPUSpawnManager::SwapGPUDespawnBuffers()
{
	FScopeLock Lock(&GPUDespawnLock);

	ActiveGPUBrushDespawns = MoveTemp(PendingGPUBrushDespawns);
	PendingGPUBrushDespawns.Empty();

	ActiveGPUSourceDespawns = MoveTemp(PendingGPUSourceDespawns);
	PendingGPUSourceDespawns.Empty();

	bHasPendingGPUDespawnRequests.store(false);

	const bool bHasAny = (ActiveGPUBrushDespawns.Num() > 0 ||
		ActiveGPUSourceDespawns.Num() > 0 ||
		HasPerSourceRecycle());

	if (bHasAny)
	{
		UE_LOG(LogGPUSpawnManager, Verbose, TEXT("SwapGPUDespawnBuffers: Brush=%d, Source=%d, PerSourceRecycle=%s"),
			ActiveGPUBrushDespawns.Num(), ActiveGPUSourceDespawns.Num(), HasPerSourceRecycle() ? TEXT("Yes") : TEXT("No"));
	}

	return bHasAny;
}

/**
 * @brief Add GPU-driven despawn RDG passes.
 * @param GraphBuilder RDG builder.
 * @param InOutParticleBuffer Particle buffer.
 * @param InOutParticleCount Particle count.
 * @param NextParticleIDHint Hint for IDShiftBits computation.
 * @param ParticleCountBuffer GPU particle count buffer.
 */
void FGPUSpawnManager::AddGPUDespawnPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferRef& InOutParticleBuffer,
	int32& InOutParticleCount,
	int32 NextParticleIDHint,
	FRDGBufferRef ParticleCountBuffer)
{
	// Safety: no particles to process
	if (InOutParticleCount <= 0)
	{
		ActiveGPUBrushDespawns.Empty();
		ActiveGPUSourceDespawns.Empty();
		return;
	}

	const bool bHasBrush = ActiveGPUBrushDespawns.Num() > 0;
	const bool bHasSource = ActiveGPUSourceDespawns.Num() > 0;
	const bool bHasPerSourceRecycle = HasPerSourceRecycle();
	const bool bHasOldest = bHasPerSourceRecycle;

	if (!bHasBrush && !bHasSource && !bHasOldest)
	{
		return;
	}

	// Mark passes use indirect dispatch (GPU-accurate particle count from ParticleCountBuffer).
	// PrefixSum/Compact still use MaxParticleCapacity (single-pass, correct with ClearUAV(0)).
	const int32 CompactionElementCount = MaxParticleCapacity;

	FRDGBufferRef AliveMaskBuffer;
	FRDGBufferRef PrefixSumsBuffer;
	FRDGBufferRef BlockSumsBuffer;
	FRDGBufferUAVRef SourceCounterUAV;
	FGlobalShaderMap* ShaderMap;
	FRDGBufferSRVRef ParticleCountSRV;

	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_Setup");

		EnsureStreamCompactionBuffers(GraphBuilder, CompactionElementCount);
		ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		AliveMaskBuffer = GraphBuilder.RegisterExternalBuffer(PersistentAliveMaskBuffer, TEXT("AliveMask"));
		PrefixSumsBuffer = GraphBuilder.RegisterExternalBuffer(PersistentPrefixSumsBuffer, TEXT("PrefixSums"));
		BlockSumsBuffer = GraphBuilder.RegisterExternalBuffer(PersistentBlockSumsBuffer, TEXT("BlockSums"));
		SourceCounterUAV = RegisterSourceCounterUAV(GraphBuilder);

		// ParticleCountBuffer SRV for bounds checking in all mark shaders
		ParticleCountSRV = GraphBuilder.CreateSRV(ParticleCountBuffer);

		// Step 1: Clear AliveMask to 0 (all dead), then InitAliveMask sets valid particles to 1
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AliveMaskBuffer), 0u);

		TShaderMapRef<FInitAliveMaskCS> InitCS(ShaderMap);
		FInitAliveMaskCS::FParameters* InitParams = GraphBuilder.AllocParameters<FInitAliveMaskCS::FParameters>();
		InitParams->OutAliveMask = GraphBuilder.CreateUAV(AliveMaskBuffer);
		InitParams->ParticleCountBuffer = ParticleCountSRV;

		GPUIndirectDispatch::AddIndirectComputePass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::InitAliveMask"),
			InitCS, InitParams, ParticleCountBuffer,
			GPUIndirectDispatch::IndirectArgsOffset_TG256);
	}

	// Step 2: Brush mark pass
	if (bHasBrush)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_MarkBrush");

		FRDGBufferRef BrushRequestBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("GPUDespawnBrushRequests"),
			sizeof(FGPUDespawnBrushRequest),
			ActiveGPUBrushDespawns.Num(),
			ActiveGPUBrushDespawns.GetData(),
			ActiveGPUBrushDespawns.Num() * sizeof(FGPUDespawnBrushRequest),
			ERDGInitialDataFlags::None
		);

		TShaderMapRef<FMarkDespawnByBrushCS> BrushCS(ShaderMap);
		FMarkDespawnByBrushCS::FParameters* BrushParams = GraphBuilder.AllocParameters<FMarkDespawnByBrushCS::FParameters>();
		BrushParams->BrushRequests = GraphBuilder.CreateSRV(BrushRequestBuffer);
		BrushParams->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
		BrushParams->OutAliveMask = GraphBuilder.CreateUAV(AliveMaskBuffer);
		BrushParams->ParticleCountBuffer = ParticleCountSRV;
		BrushParams->BrushRequestCount = ActiveGPUBrushDespawns.Num();

		GPUIndirectDispatch::AddIndirectComputePass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::DespawnBrush(%d brushes)", ActiveGPUBrushDespawns.Num()),
			BrushCS, BrushParams, ParticleCountBuffer,
			GPUIndirectDispatch::IndirectArgsOffset_TG256);
	}

	// Step 3: Source mark pass
	if (bHasSource)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_MarkSource");

		FRDGBufferRef SourceIDBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("GPUDespawnSourceIDs"),
			sizeof(int32),
			ActiveGPUSourceDespawns.Num(),
			ActiveGPUSourceDespawns.GetData(),
			ActiveGPUSourceDespawns.Num() * sizeof(int32),
			ERDGInitialDataFlags::None
		);

		TShaderMapRef<FMarkDespawnBySourceCS> SourceCS(ShaderMap);
		FMarkDespawnBySourceCS::FParameters* SourceParams = GraphBuilder.AllocParameters<FMarkDespawnBySourceCS::FParameters>();
		SourceParams->DespawnSourceIDs = GraphBuilder.CreateSRV(SourceIDBuffer);
		SourceParams->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
		SourceParams->OutAliveMask = GraphBuilder.CreateUAV(AliveMaskBuffer);
		SourceParams->ParticleCountBuffer = ParticleCountSRV;
		SourceParams->DespawnSourceIDCount = ActiveGPUSourceDespawns.Num();

		GPUIndirectDispatch::AddIndirectComputePass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::DespawnSource(%d sources)", ActiveGPUSourceDespawns.Num()),
			SourceCS, SourceParams, ParticleCountBuffer,
			GPUIndirectDispatch::IndirectArgsOffset_TG256);
	}

	// Shared: Compute IDShiftBits (used by both per-source and global oldest)
	const int32 MaxID = FMath::Max(1, NextParticleIDHint);
	const int32 Log2MaxID = FMath::FloorLog2(MaxID);
	const int32 IDShiftBits = FMath::Max(0, Log2MaxID - 7);

	// Shared: Ensure histogram buffers exist (reused by per-source loop and global oldest)
	FRDGBufferRef IDHistogramBuffer = nullptr;
	FRDGBufferRef OldestThresholdBuffer = nullptr;
	FRDGBufferRef BoundaryCounterBuffer = nullptr;

	if (bHasOldest)
	{
		if (!PersistentIDHistogramBuffer.IsValid())
		{
			FRDGBufferDesc HistDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 256);
			IDHistogramBuffer = GraphBuilder.CreateBuffer(HistDesc, TEXT("IDHistogram"));
			PersistentIDHistogramBuffer = GraphBuilder.ConvertToExternalBuffer(IDHistogramBuffer);
		}
		else
		{
			IDHistogramBuffer = GraphBuilder.RegisterExternalBuffer(PersistentIDHistogramBuffer, TEXT("IDHistogram"));
		}

		if (!PersistentOldestThresholdBuffer.IsValid())
		{
			FRDGBufferDesc ThreshDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2);
			OldestThresholdBuffer = GraphBuilder.CreateBuffer(ThreshDesc, TEXT("OldestThreshold"));
			PersistentOldestThresholdBuffer = GraphBuilder.ConvertToExternalBuffer(OldestThresholdBuffer);
		}
		else
		{
			OldestThresholdBuffer = GraphBuilder.RegisterExternalBuffer(PersistentOldestThresholdBuffer, TEXT("OldestThreshold"));
		}

		if (!PersistentBoundaryCounterBuffer.IsValid())
		{
			FRDGBufferDesc CounterDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1);
			BoundaryCounterBuffer = GraphBuilder.CreateBuffer(CounterDesc, TEXT("BoundaryCounter"));
			PersistentBoundaryCounterBuffer = GraphBuilder.ConvertToExternalBuffer(BoundaryCounterBuffer);
		}
		else
		{
			BoundaryCounterBuffer = GraphBuilder.RegisterExternalBuffer(PersistentBoundaryCounterBuffer, TEXT("BoundaryCounter"));
		}
	}

	// Step 3.5: Per-source recycle — compute PerSourceExcess[64] and run oldest 3-pass per source
	if (bHasPerSourceRecycle)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_PerSourceRecycle");

		// Ensure PerSourceExcess buffer exists
		FRDGBufferRef PerSourceExcessBuffer;
		if (!PersistentPerSourceExcessBuffer.IsValid())
		{
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), EGPUParticleSource::MaxSourceCount);
			PerSourceExcessBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("PerSourceExcess"));
			PersistentPerSourceExcessBuffer = GraphBuilder.ConvertToExternalBuffer(PerSourceExcessBuffer);
		}
		else
		{
			PerSourceExcessBuffer = GraphBuilder.RegisterExternalBuffer(PersistentPerSourceExcessBuffer, TEXT("PerSourceExcess"));
		}

		// Ensure EmitterMaxCounts buffer exists and upload if dirty
		FRDGBufferRef EmitterMaxCountsBuffer;
		if (!PersistentEmitterMaxCountsBuffer.IsValid() || bEmitterMaxCountsDirty)
		{
			TArray<uint32> MaxCountsUint32;
			MaxCountsUint32.SetNumUninitialized(EGPUParticleSource::MaxSourceCount);
			for (int32 i = 0; i < EGPUParticleSource::MaxSourceCount; ++i)
			{
				MaxCountsUint32[i] = static_cast<uint32>(FMath::Max(0, EmitterMaxCountsCPU.IsValidIndex(i) ? EmitterMaxCountsCPU[i] : 0));
			}

			EmitterMaxCountsBuffer = CreateStructuredBuffer(
				GraphBuilder,
				TEXT("EmitterMaxCounts"),
				sizeof(uint32),
				EGPUParticleSource::MaxSourceCount,
				MaxCountsUint32.GetData(),
				MaxCountsUint32.Num() * sizeof(uint32),
				ERDGInitialDataFlags::None
			);
			PersistentEmitterMaxCountsBuffer = GraphBuilder.ConvertToExternalBuffer(EmitterMaxCountsBuffer);
			bEmitterMaxCountsDirty = false;
		}
		else
		{
			EmitterMaxCountsBuffer = GraphBuilder.RegisterExternalBuffer(PersistentEmitterMaxCountsBuffer, TEXT("EmitterMaxCounts"));
		}

		// Build per-source incoming spawn counts from ActiveSpawnRequests
		TArray<uint32> IncomingCounts;
		IncomingCounts.SetNumZeroed(EGPUParticleSource::MaxSourceCount);
		for (const FGPUSpawnRequest& Req : ActiveSpawnRequests)
		{
			if (Req.SourceID >= 0 && Req.SourceID < EGPUParticleSource::MaxSourceCount)
			{
				IncomingCounts[Req.SourceID]++;
			}
		}

		FRDGBufferRef IncomingSpawnCountsBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("IncomingSpawnCounts"),
			sizeof(uint32),
			EGPUParticleSource::MaxSourceCount,
			IncomingCounts.GetData(),
			IncomingCounts.Num() * sizeof(uint32),
			ERDGInitialDataFlags::None
		);

		// Register SourceCounters as SRV for reading
		FRDGBufferRef SourceCounterRDG = GraphBuilder.RegisterExternalBuffer(SourceCounterBuffer, TEXT("SourceCountersForRecycle"));

		// Clear PerSourceExcess before compute
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PerSourceExcessBuffer), 0u);

		// Dispatch ComputePerSourceRecycleCS → PerSourceExcess[64]
		{
			TShaderMapRef<FComputePerSourceRecycleCS> RecycleCS(ShaderMap);
			FComputePerSourceRecycleCS::FParameters* RecycleParams = GraphBuilder.AllocParameters<FComputePerSourceRecycleCS::FParameters>();
			RecycleParams->SourceCounters = GraphBuilder.CreateSRV(SourceCounterRDG);
			RecycleParams->EmitterMaxCounts = GraphBuilder.CreateSRV(EmitterMaxCountsBuffer);
			RecycleParams->IncomingSpawnCounts = GraphBuilder.CreateSRV(IncomingSpawnCountsBuffer);
			RecycleParams->PerSourceExcess = GraphBuilder.CreateUAV(PerSourceExcessBuffer);
			RecycleParams->ActiveSourceCount = EGPUParticleSource::MaxSourceCount;

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("GPUFluid::ComputePerSourceExcess"),
				RecycleCS, RecycleParams, FIntVector(1, 1, 1));
		}

		FRDGBufferSRVRef PerSourceExcessSRV = GraphBuilder.CreateSRV(PerSourceExcessBuffer);

		// Step 3.6: Per-source oldest 3-pass — loop over each source with EmitterMax set
		for (int32 s = 0; s < EGPUParticleSource::MaxSourceCount; ++s)
		{
			if (!EmitterMaxCountsCPU.IsValidIndex(s) || EmitterMaxCountsCPU[s] <= 0)
			{
				continue;
			}

			// Clear histogram and boundary counter for this source
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(IDHistogramBuffer), 0u);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BoundaryCounterBuffer), 0u);

			// Pass 1: BuildIDHistogramCS (filtered to SourceID == s)
			{
				TShaderMapRef<FBuildIDHistogramCS> HistCS(ShaderMap);
				FBuildIDHistogramCS::FParameters* HistParams = GraphBuilder.AllocParameters<FBuildIDHistogramCS::FParameters>();
				HistParams->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
				HistParams->IDHistogram = GraphBuilder.CreateUAV(IDHistogramBuffer);
				HistParams->ParticleCountBuffer = ParticleCountSRV;
				HistParams->PerSourceExcess = PerSourceExcessSRV;
				HistParams->FilterSourceID = s;
				HistParams->IDShiftBits = IDShiftBits;

				GPUIndirectDispatch::AddIndirectComputePass(GraphBuilder,
					RDG_EVENT_NAME("GPUFluid::PerSourceOldest_Histogram(src=%d)", s),
					HistCS, HistParams, ParticleCountBuffer,
					GPUIndirectDispatch::IndirectArgsOffset_TG256);
			}

			// Pass 2: FindOldestThresholdCS (removeCount = PerSourceExcess[s], read on GPU)
			{
				TShaderMapRef<FFindOldestThresholdCS> ThreshCS(ShaderMap);
				FFindOldestThresholdCS::FParameters* ThreshParams = GraphBuilder.AllocParameters<FFindOldestThresholdCS::FParameters>();
				ThreshParams->IDHistogram = GraphBuilder.CreateUAV(IDHistogramBuffer);
				ThreshParams->OldestThreshold = GraphBuilder.CreateUAV(OldestThresholdBuffer);
				ThreshParams->ParticleCountBuffer = ParticleCountSRV;
				ThreshParams->PerSourceExcess = PerSourceExcessSRV;
				ThreshParams->FilterSourceID = s;

				FComputeShaderUtils::AddPass(GraphBuilder,
					RDG_EVENT_NAME("GPUFluid::PerSourceOldest_Threshold(src=%d)", s),
					ThreshCS, ThreshParams, FIntVector(1, 1, 1));
			}

			// Pass 3: MarkOldestParticlesCS (filtered to SourceID == s)
			{
				TShaderMapRef<FMarkOldestParticlesCS> MarkCS(ShaderMap);
				FMarkOldestParticlesCS::FParameters* MarkParams = GraphBuilder.AllocParameters<FMarkOldestParticlesCS::FParameters>();
				MarkParams->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
				MarkParams->OldestThreshold = GraphBuilder.CreateUAV(OldestThresholdBuffer);
				MarkParams->OutAliveMask = GraphBuilder.CreateUAV(AliveMaskBuffer);
				MarkParams->BoundaryCounter = GraphBuilder.CreateUAV(BoundaryCounterBuffer);
				MarkParams->ParticleCountBuffer = ParticleCountSRV;
				MarkParams->PerSourceExcess = PerSourceExcessSRV;
				MarkParams->FilterSourceID = s;
				MarkParams->IDShiftBits = IDShiftBits;

				GPUIndirectDispatch::AddIndirectComputePass(GraphBuilder,
					RDG_EVENT_NAME("GPUFluid::PerSourceOldest_Mark(src=%d)", s),
					MarkCS, MarkParams, ParticleCountBuffer,
					GPUIndirectDispatch::IndirectArgsOffset_TG256);
			}
		}
	}

	// Step 4: UpdateSourceCountersDespawnCS
	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_UpdateSourceCounters");

		TShaderMapRef<FUpdateSourceCountersDespawnCS> UpdateCS(ShaderMap);
		FUpdateSourceCountersDespawnCS::FParameters* UpdateParams = GraphBuilder.AllocParameters<FUpdateSourceCountersDespawnCS::FParameters>();
		UpdateParams->AliveMask = GraphBuilder.CreateSRV(AliveMaskBuffer);
		UpdateParams->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
		UpdateParams->SourceCounters = SourceCounterUAV;
		UpdateParams->ParticleCountBuffer = ParticleCountSRV;
		UpdateParams->MaxSourceCount = EGPUParticleSource::MaxSourceCount;

		GPUIndirectDispatch::AddIndirectComputePass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::DespawnUpdateSourceCounters"),
			UpdateCS, UpdateParams, ParticleCountBuffer,
			GPUIndirectDispatch::IndirectArgsOffset_TG256);
	}

	// Step 5-6: PrefixSum + Compact (reuse existing stream compaction pipeline)
	const int32 PrefixSumBlockNumGroups = FMath::DivideAndRoundUp(CompactionElementCount, FPrefixSumBlockCS_RDG::ThreadGroupSize);

	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_PrefixSum");

		TShaderMapRef<FPrefixSumBlockCS_RDG> PrefixSumBlock(ShaderMap);
		FPrefixSumBlockCS_RDG::FParameters* PrefixSumBlockParameters = GraphBuilder.AllocParameters<FPrefixSumBlockCS_RDG::FParameters>();
		PrefixSumBlockParameters->MarkedFlags = GraphBuilder.CreateSRV(AliveMaskBuffer);
		PrefixSumBlockParameters->PrefixSums = GraphBuilder.CreateUAV(PrefixSumsBuffer);
		PrefixSumBlockParameters->BlockSums = GraphBuilder.CreateUAV(BlockSumsBuffer);
		PrefixSumBlockParameters->ElementCount = CompactionElementCount;

		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::GPUDespawn_PrefixSumBlock"),
			PrefixSumBlock, PrefixSumBlockParameters,
			FIntVector(PrefixSumBlockNumGroups, 1, 1));

		TShaderMapRef<FScanBlockSumsCS_RDG> ScanBlockSums(ShaderMap);
		FScanBlockSumsCS_RDG::FParameters* ScanBlockSumsParameters = GraphBuilder.AllocParameters<FScanBlockSumsCS_RDG::FParameters>();
		ScanBlockSumsParameters->BlockSums = GraphBuilder.CreateUAV(BlockSumsBuffer);
		ScanBlockSumsParameters->BlockCount = PrefixSumBlockNumGroups;

		const int32 ScanBlockSumsNumGroups = FMath::DivideAndRoundUp(PrefixSumBlockNumGroups, FScanBlockSumsCS_RDG::ThreadGroupSize);
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::GPUDespawn_ScanBlockSums"),
			ScanBlockSums, ScanBlockSumsParameters,
			FIntVector(ScanBlockSumsNumGroups, 1, 1));

		TShaderMapRef<FAddBlockOffsetsCS_RDG> AddBlockOffsets(ShaderMap);
		FAddBlockOffsetsCS_RDG::FParameters* AddBlockOffsetsParameters = GraphBuilder.AllocParameters<FAddBlockOffsetsCS_RDG::FParameters>();
		AddBlockOffsetsParameters->PrefixSums = GraphBuilder.CreateUAV(PrefixSumsBuffer);
		AddBlockOffsetsParameters->BlockSums = GraphBuilder.CreateUAV(BlockSumsBuffer);
		AddBlockOffsetsParameters->ElementCount = CompactionElementCount;

		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::GPUDespawn_AddBlockOffsets"),
			AddBlockOffsets, AddBlockOffsetsParameters,
			FIntVector(PrefixSumBlockNumGroups, 1, 1));
	}

	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_Compact");

		FRDGBufferRef CompactedParticlesBuffer = GraphBuilder.RegisterExternalBuffer(
			PersistentCompactedBuffer[CompactedBufferIndex], TEXT("CompactedParticlesGPU"));
		CompactedBufferIndex = 1 - CompactedBufferIndex;

		TShaderMapRef<FCompactParticlesCS_RDG> Compact(ShaderMap);
		FCompactParticlesCS_RDG::FParameters* CompactParameters = GraphBuilder.AllocParameters<FCompactParticlesCS_RDG::FParameters>();
		CompactParameters->Particles = GraphBuilder.CreateSRV(InOutParticleBuffer);
		CompactParameters->MarkedFlags = GraphBuilder.CreateSRV(AliveMaskBuffer);
		CompactParameters->PrefixSums = GraphBuilder.CreateSRV(PrefixSumsBuffer);
		CompactParameters->CompactedParticles = GraphBuilder.CreateUAV(CompactedParticlesBuffer);
		CompactParameters->ParticleCount = CompactionElementCount;

		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::GPUDespawn_Compact"),
			Compact, CompactParameters,
			FIntVector(PrefixSumBlockNumGroups, 1, 1));

		InOutParticleBuffer = CompactedParticlesBuffer;
	}

	// Step 7: Write exact alive count to ParticleCountBuffer (dispatch args + raw count + DrawIndirect)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "GPUDespawn_WriteAliveCount");

		TShaderMapRef<FWriteAliveCountAfterCompactionCS> WriteCountShader(ShaderMap);
		FWriteAliveCountAfterCompactionCS::FParameters* WriteCountParams =
			GraphBuilder.AllocParameters<FWriteAliveCountAfterCompactionCS::FParameters>();
		WriteCountParams->PrefixSums = GraphBuilder.CreateSRV(PrefixSumsBuffer);
		WriteCountParams->AliveMask = GraphBuilder.CreateSRV(AliveMaskBuffer);
		WriteCountParams->ParticleCountBuffer = GraphBuilder.CreateUAV(ParticleCountBuffer);
		WriteCountParams->OldParticleCount = CompactionElementCount;

		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::WriteAliveCountAfterCompaction"),
			WriteCountShader, WriteCountParams, FIntVector(1, 1, 1));
	}

	// Clear active requests
	ActiveGPUBrushDespawns.Empty();
	ActiveGPUSourceDespawns.Empty();
}

//=============================================================================
// Render Thread API
//=============================================================================

/**
 * @brief Swap pending requests to active buffer.
 */
void FGPUSpawnManager::SwapBuffers()
{
	FScopeLock Lock(&SpawnLock);

	// Move pending requests to active buffer
	ActiveSpawnRequests = MoveTemp(PendingSpawnRequests);
	PendingSpawnRequests.Empty();
	bHasPendingSpawnRequests.store(false);
}

/**
 * @brief Add spawn particles RDG pass.
 * @param GraphBuilder RDG builder.
 * @param ParticlesUAV Particle buffer UAV.
 * @param ParticleCounterUAV Atomic counter UAV.
 * @param MaxParticleCount Maximum particle capacity.
 */
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

	// Get or create source counter buffer UAV
	FRDGBufferUAVRef SourceCounterUAV = RegisterSourceCounterUAV(GraphBuilder);

	FSpawnParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSpawnParticlesCS::FParameters>();
	PassParameters->SpawnRequests = GraphBuilder.CreateSRV(SpawnRequestBuffer);
	PassParameters->Particles = ParticlesUAV;
	PassParameters->ParticleCounter = ParticleCounterUAV;
	PassParameters->SourceCounters = SourceCounterUAV;
	PassParameters->SpawnRequestCount = ActiveSpawnRequests.Num();
	PassParameters->MaxParticleCount = MaxParticleCount;
	PassParameters->NextParticleID = NextParticleID.load();
	PassParameters->MaxSourceCount = EGPUParticleSource::MaxSourceCount;
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

	// UE_LOG(LogGPUSpawnManager, Verbose, TEXT("SpawnParticlesPass: Spawning %d particles (NextID: %d)"),
	// 	ActiveSpawnRequests.Num(), NextParticleID.load());
}


/**
 * @brief Update next particle ID after spawning.
 * @param SpawnedCount Number of particles successfully spawned.
 */
void FGPUSpawnManager::OnSpawnComplete(int32 SpawnedCount)
{
	if (SpawnedCount > 0)
	{
		NextParticleID.fetch_add(SpawnedCount);
	}
}

//=============================================================================
// Source Counter API (Per-Component Particle Count Tracking)
//=============================================================================

/**
 * @brief Register source counter buffer for RDG and get UAV.
 * @param GraphBuilder RDG builder.
 * @return Source counter UAV.
 */
FRDGBufferUAVRef FGPUSpawnManager::RegisterSourceCounterUAV(FRDGBuilder& GraphBuilder)
{
	// Create persistent buffer if not exists
	if (!SourceCounterBuffer.IsValid())
	{
		// Allocate on render thread via RDG
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), EGPUParticleSource::MaxSourceCount);
		FRDGBufferRef TempBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("GPUSourceCounters"));

		// Initialize to zero
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TempBuffer), 0u);

		// Extract to persistent buffer
		GraphBuilder.QueueBufferExtraction(TempBuffer, &SourceCounterBuffer);

		UE_LOG(LogGPUSpawnManager, Log, TEXT("Created SourceCounterBuffer with %d slots"), EGPUParticleSource::MaxSourceCount);

		return GraphBuilder.CreateUAV(TempBuffer);
	}

	// Register existing buffer
	FRDGBufferRef RegisteredBuffer = GraphBuilder.RegisterExternalBuffer(SourceCounterBuffer, TEXT("GPUSourceCounters"));
	return GraphBuilder.CreateUAV(RegisteredBuffer);
}

/**
 * @brief Get particle count for a specific source (component).
 * @param SourceID Source component ID.
 * @return Particle count or -1 if invalid.
 */
int32 FGPUSpawnManager::GetParticleCountForSource(int32 SourceID) const
{
	// Check SourceID range
	if (SourceID < 0 || SourceID >= EGPUParticleSource::MaxSourceCount)
	{
		return -1;  // Invalid SourceID
	}

	// If GPU buffer not yet created, no data available
	if (!SourceCounterBuffer.IsValid())
	{
		return -1;  // Data not ready
	}

	FScopeLock Lock(&SourceCountLock);
	if (SourceID < CachedSourceCounts.Num())
	{
		return CachedSourceCounts[SourceID];
	}
	return -1;
}

/**
 * @brief Get all source counts.
 * @return Array of particle counts per source.
 */
TArray<int32> FGPUSpawnManager::GetAllSourceCounts() const
{
	FScopeLock Lock(&SourceCountLock);
	return CachedSourceCounts;
}

/**
 * @brief Enqueue source counter readback.
 * @param RHICmdList Command list.
 */
void FGPUSpawnManager::EnqueueSourceCounterReadback(FRHICommandListImmediate& RHICmdList)
{
	if (!SourceCounterBuffer.IsValid())
	{
		return;
	}

	FRHIBuffer* SourceBuffer = SourceCounterBuffer->GetRHI();
	if (!SourceBuffer)
	{
		return;
	}

	// Ring buffer full - skip this frame
	if (SourceCounterPendingCount >= SourceCounterRingBufferSize)
	{
		UE_LOG(LogGPUSpawnManager, Verbose, TEXT("SourceCounter ring buffer full, skipping enqueue"));
		return;
	}

	// Enqueue to write slot
	if (SourceCounterReadbacks.IsValidIndex(SourceCounterWriteIndex) && SourceCounterReadbacks[SourceCounterWriteIndex])
	{
		const uint32 CopySize = EGPUParticleSource::MaxSourceCount * sizeof(uint32);

		// State transition for readback
		RHICmdList.Transition(FRHITransitionInfo(SourceBuffer, ERHIAccess::UAVCompute, ERHIAccess::CopySrc));
		SourceCounterReadbacks[SourceCounterWriteIndex]->EnqueueCopy(RHICmdList, SourceBuffer, CopySize);
		RHICmdList.Transition(FRHITransitionInfo(SourceBuffer, ERHIAccess::CopySrc, ERHIAccess::UAVCompute));

		// Advance write index
		SourceCounterWriteIndex = (SourceCounterWriteIndex + 1) % SourceCounterRingBufferSize;
		++SourceCounterPendingCount;
	}
}

/**
 * @brief Process source counter readback (check completion, copy to cache).
 */
void FGPUSpawnManager::ProcessSourceCounterReadback()
{
	// Nothing pending
	if (SourceCounterPendingCount <= 0)
	{
		return;
	}

	// Check if oldest readback is ready
	if (!SourceCounterReadbacks.IsValidIndex(SourceCounterReadIndex) ||
		!SourceCounterReadbacks[SourceCounterReadIndex] ||
		!SourceCounterReadbacks[SourceCounterReadIndex]->IsReady())
	{
		return;
	}

	const uint32 CopySize = EGPUParticleSource::MaxSourceCount * sizeof(uint32);
	const uint32* Data = static_cast<const uint32*>(SourceCounterReadbacks[SourceCounterReadIndex]->Lock(CopySize));

	if (Data)
	{
		FScopeLock Lock(&SourceCountLock);
		CachedSourceCounts.SetNum(EGPUParticleSource::MaxSourceCount);
		for (int32 i = 0; i < EGPUParticleSource::MaxSourceCount; ++i)
		{
			CachedSourceCounts[i] = static_cast<int32>(Data[i]);
		}
	}

	SourceCounterReadbacks[SourceCounterReadIndex]->Unlock();

	// Advance read index
	SourceCounterReadIndex = (SourceCounterReadIndex + 1) % SourceCounterRingBufferSize;
	--SourceCounterPendingCount;
}

/**
 * @brief Clear all source counters.
 * @param GraphBuilder RDG builder.
 */
void FGPUSpawnManager::ClearSourceCounters(FRDGBuilder& GraphBuilder)
{
	if (!SourceCounterBuffer.IsValid())
	{
		return;
	}

	FRDGBufferRef RegisteredBuffer = GraphBuilder.RegisterExternalBuffer(SourceCounterBuffer, TEXT("GPUSourceCounters"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RegisteredBuffer), 0u);

	// Also clear cached counts
	{
		FScopeLock Lock(&SourceCountLock);
		for (int32& Count : CachedSourceCounts)
		{
			Count = 0;
		}
	}

	UE_LOG(LogGPUSpawnManager, Log, TEXT("Cleared all source counters"));
}

/**
 * @brief Initialize source counters from uploaded particles.
 * @param Particles Array of particles to count by SourceID.
 */
void FGPUSpawnManager::InitializeSourceCountersFromParticles(const TArray<FGPUFluidParticle>& Particles)
{
	if (Particles.Num() == 0)
	{
		return;
	}

	// Count particles by SourceID
	TArray<int32> SourceCounts;
	SourceCounts.SetNumZeroed(EGPUParticleSource::MaxSourceCount);

	for (const FGPUFluidParticle& Particle : Particles)
	{
		const int32 SourceID = Particle.SourceID;
		if (SourceID >= 0 && SourceID < EGPUParticleSource::MaxSourceCount)
		{
			SourceCounts[SourceID]++;
		}
	}

	// Update CPU cache immediately
	{
		FScopeLock Lock(&SourceCountLock);
		CachedSourceCounts = SourceCounts;
	}

	// Create or update GPU buffer
	TArray<uint32> CountsUint32;
	CountsUint32.SetNumUninitialized(EGPUParticleSource::MaxSourceCount);
	for (int32 i = 0; i < EGPUParticleSource::MaxSourceCount; ++i)
	{
		CountsUint32[i] = static_cast<uint32>(SourceCounts[i]);
	}

	FGPUSpawnManager* Self = this;
	TArray<uint32> CountsCopy = CountsUint32;

	ENQUEUE_RENDER_COMMAND(InitializeSourceCounters)(
		[Self, CountsCopy](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("InitializeSourceCounters"));

			FRDGBufferRef CounterBuffer;
			if (!Self->SourceCounterBuffer.IsValid())
			{
				// Create buffer with initial data if not exists
				CounterBuffer = CreateStructuredBuffer(
					GraphBuilder,
					TEXT("GPUSourceCounters"),
					sizeof(uint32),
					EGPUParticleSource::MaxSourceCount,
					CountsCopy.GetData(),
					CountsCopy.Num() * sizeof(uint32),
					ERDGInitialDataFlags::None
				);
				GraphBuilder.QueueBufferExtraction(CounterBuffer, &Self->SourceCounterBuffer);
			}
			else
			{
				// Buffer exists, just upload data
				CounterBuffer = GraphBuilder.RegisterExternalBuffer(Self->SourceCounterBuffer, TEXT("GPUSourceCounters"));
				GraphBuilder.QueueBufferUpload(CounterBuffer, CountsCopy.GetData(), CountsCopy.Num() * sizeof(uint32));
			}

			GraphBuilder.Execute();
		}
	);

	FlushRenderingCommands();

	// Log source counts
	int32 TotalCounted = 0;
	for (int32 i = 0; i < EGPUParticleSource::MaxSourceCount; ++i)
	{
		if (CachedSourceCounts[i] > 0)
		{
			UE_LOG(LogGPUSpawnManager, Log, TEXT("InitializeSourceCounters: SourceID %d = %d particles"), i, CachedSourceCounts[i]);
			TotalCounted += CachedSourceCounts[i];
		}
	}
	UE_LOG(LogGPUSpawnManager, Log, TEXT("InitializeSourceCounters: Total %d particles from %d input"), TotalCounted, Particles.Num());
}

//=============================================================================
// Stream Compaction Buffers (Persistent)
//=============================================================================

/**
 * @brief Ensure stream compaction buffers are allocated with sufficient capacity.
 * @param GraphBuilder RDG builder.
 * @param RequiredCapacity Required particle capacity.
 */
void FGPUSpawnManager::EnsureStreamCompactionBuffers(FRDGBuilder& GraphBuilder, int32 RequiredCapacity)
{
	// Already allocated with sufficient capacity
	if (PersistentAliveMaskBuffer.IsValid() && StreamCompactionCapacity >= RequiredCapacity)
	{
		return;
	}

	// Use MaxParticleCapacity for fixed-size allocation (no dynamic resize)
	const int32 Capacity = FMath::Max(RequiredCapacity, MaxParticleCapacity);
	const int32 BlockCount = FMath::DivideAndRoundUp(Capacity, static_cast<int32>(FPrefixSumBlockCS_RDG::ThreadGroupSize));

	// Create AliveMask buffer (uint32 × Capacity)
	FRDGBufferDesc AliveMaskDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), Capacity);
	FRDGBufferRef AliveMask = GraphBuilder.CreateBuffer(AliveMaskDesc, TEXT("PersistentAliveMask"));
	PersistentAliveMaskBuffer = GraphBuilder.ConvertToExternalBuffer(AliveMask);

	// Create PrefixSums buffer (uint32 × Capacity)
	FRDGBufferDesc PrefixSumsDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), Capacity);
	FRDGBufferRef PrefixSums = GraphBuilder.CreateBuffer(PrefixSumsDesc, TEXT("PersistentPrefixSums"));
	PersistentPrefixSumsBuffer = GraphBuilder.ConvertToExternalBuffer(PrefixSums);

	// Create BlockSums buffer (uint32 × BlockCount)
	FRDGBufferDesc BlockSumsDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), BlockCount);
	FRDGBufferRef BlockSums = GraphBuilder.CreateBuffer(BlockSumsDesc, TEXT("PersistentBlockSums"));
	PersistentBlockSumsBuffer = GraphBuilder.ConvertToExternalBuffer(BlockSums);

	// Create double-buffered CompactedParticles buffers (FGPUFluidParticle × Capacity)
	FRDGBufferDesc CompactedDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUFluidParticle), Capacity);
	FRDGBufferRef Compacted0 = GraphBuilder.CreateBuffer(CompactedDesc, TEXT("PersistentCompacted0"));
	FRDGBufferRef Compacted1 = GraphBuilder.CreateBuffer(CompactedDesc, TEXT("PersistentCompacted1"));
	PersistentCompactedBuffer[0] = GraphBuilder.ConvertToExternalBuffer(Compacted0);
	PersistentCompactedBuffer[1] = GraphBuilder.ConvertToExternalBuffer(Compacted1);

	StreamCompactionCapacity = Capacity;

	const int32 CompactedSize = Capacity * sizeof(FGPUFluidParticle) * 2;  // Double buffer
	UE_LOG(LogGPUSpawnManager, Log, TEXT("EnsureStreamCompactionBuffers: Allocated %d capacity (%d KB total)"),
		Capacity, (Capacity * 2 * 4 + BlockCount * 4 + CompactedSize) / 1024);
}

//=============================================================================
// Stream Compaction Buffer Accessors
//=============================================================================

FRDGBufferSRVRef FGPUSpawnManager::GetLastPrefixSumsSRV(FRDGBuilder& GraphBuilder) const
{
	check(PersistentPrefixSumsBuffer.IsValid());
	FRDGBufferRef Buffer = GraphBuilder.RegisterExternalBuffer(PersistentPrefixSumsBuffer, TEXT("DespawnPrefixSums"));
	return GraphBuilder.CreateSRV(Buffer);
}

FRDGBufferSRVRef FGPUSpawnManager::GetLastAliveMaskSRV(FRDGBuilder& GraphBuilder) const
{
	check(PersistentAliveMaskBuffer.IsValid());
	FRDGBufferRef Buffer = GraphBuilder.RegisterExternalBuffer(PersistentAliveMaskBuffer, TEXT("DespawnAliveMask"));
	return GraphBuilder.CreateSRV(Buffer);
}
