// Copyright KawaiiFluid Team. All Rights Reserved.
// FGPUStreamCompactionManager - GPU AABB filtering using parallel prefix sum

#include "GPU/Managers/GPUStreamCompactionManager.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPUStreamCompaction, Log, All);
DEFINE_LOG_CATEGORY(LogGPUStreamCompaction);

//=============================================================================
// Constructor / Destructor
//=============================================================================

FGPUStreamCompactionManager::FGPUStreamCompactionManager()
	: bIsInitialized(false)
	, MaxParticleCapacity(0)
	, bBuffersAllocated(false)
{
}

FGPUStreamCompactionManager::~FGPUStreamCompactionManager()
{
	Release();
}

//=============================================================================
// Lifecycle
//=============================================================================

void FGPUStreamCompactionManager::Initialize(int32 InMaxParticleCount)
{
	if (InMaxParticleCount <= 0)
	{
		UE_LOG(LogGPUStreamCompaction, Warning, TEXT("Initialize called with invalid particle count: %d"), InMaxParticleCount);
		return;
	}

	MaxParticleCapacity = InMaxParticleCount;
	bIsInitialized = true;

	UE_LOG(LogGPUStreamCompaction, Log, TEXT("GPUStreamCompactionManager initialized with capacity: %d"), MaxParticleCapacity);
}

void FGPUStreamCompactionManager::Release()
{
	ReleaseBuffers();
	bIsInitialized = false;
	MaxParticleCapacity = 0;

	UE_LOG(LogGPUStreamCompaction, Log, TEXT("GPUStreamCompactionManager released"));
}

//=============================================================================
// Buffer Management
//=============================================================================

void FGPUStreamCompactionManager::AllocateBuffers(FRHICommandListImmediate& RHICmdList)
{
	if (bBuffersAllocated || MaxParticleCapacity <= 0)
	{
		return;
	}

	const int32 BlockSize = 256;
	const int32 NumBlocks = FMath::DivideAndRoundUp(MaxParticleCapacity, BlockSize);

	// Marked flags buffer (uint per particle)
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_MarkedFlags"), MaxParticleCapacity * sizeof(uint32), sizeof(uint32))
			.AddUsage(BUF_UnorderedAccess | BUF_ShaderResource)
			.SetInitialState(ERHIAccess::UAVMask);
		MarkedFlagsBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
		MarkedFlagsSRV = RHICmdList.CreateShaderResourceView(MarkedFlagsBufferRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(MarkedFlagsBufferRHI));
		MarkedFlagsUAV = RHICmdList.CreateUnorderedAccessView(MarkedFlagsBufferRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(MarkedFlagsBufferRHI));
	}

	// Marked AABB index buffer (int per particle)
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_MarkedAABBIndex"), MaxParticleCapacity * sizeof(int32), sizeof(int32))
			.AddUsage(BUF_UnorderedAccess | BUF_ShaderResource)
			.SetInitialState(ERHIAccess::UAVMask);
		MarkedAABBIndexBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
		MarkedAABBIndexSRV = RHICmdList.CreateShaderResourceView(MarkedAABBIndexBufferRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(MarkedAABBIndexBufferRHI));
		MarkedAABBIndexUAV = RHICmdList.CreateUnorderedAccessView(MarkedAABBIndexBufferRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(MarkedAABBIndexBufferRHI));
	}

	// Prefix sums buffer
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_PrefixSums"), MaxParticleCapacity * sizeof(uint32), sizeof(uint32))
			.AddUsage(BUF_UnorderedAccess | BUF_ShaderResource)
			.SetInitialState(ERHIAccess::UAVMask);
		PrefixSumsBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
		PrefixSumsSRV = RHICmdList.CreateShaderResourceView(PrefixSumsBufferRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(PrefixSumsBufferRHI));
		PrefixSumsUAV = RHICmdList.CreateUnorderedAccessView(PrefixSumsBufferRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(PrefixSumsBufferRHI));
	}

	// Block sums buffer
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_BlockSums"), NumBlocks * sizeof(uint32), sizeof(uint32))
			.AddUsage(BUF_UnorderedAccess | BUF_ShaderResource)
			.SetInitialState(ERHIAccess::UAVMask);
		BlockSumsBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
		BlockSumsSRV = RHICmdList.CreateShaderResourceView(BlockSumsBufferRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(BlockSumsBufferRHI));
		BlockSumsUAV = RHICmdList.CreateUnorderedAccessView(BlockSumsBufferRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(BlockSumsBufferRHI));
	}

	// Compacted candidates buffer (worst case: all particles)
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_CompactedCandidates"), MaxParticleCapacity * sizeof(FGPUCandidateParticle), sizeof(FGPUCandidateParticle))
			.AddUsage(BUF_UnorderedAccess | BUF_ShaderResource)
			.SetInitialState(ERHIAccess::UAVMask);
		CompactedCandidatesBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
		CompactedCandidatesUAV = RHICmdList.CreateUnorderedAccessView(CompactedCandidatesBufferRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(CompactedCandidatesBufferRHI));
	}

	// Total count buffer (single uint)
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_TotalCount"), sizeof(uint32), sizeof(uint32))
			.AddUsage(BUF_UnorderedAccess | BUF_ShaderResource)
			.SetInitialState(ERHIAccess::UAVMask);
		TotalCountBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
		TotalCountUAV = RHICmdList.CreateUnorderedAccessView(TotalCountBufferRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(TotalCountBufferRHI));
	}

	// Staging buffers for readback
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_TotalCountStaging"), sizeof(uint32), sizeof(uint32))
			.AddUsage(BUF_None)
			.SetInitialState(ERHIAccess::CopyDest);
		TotalCountStagingBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
	}
	{
		const FRHIBufferCreateDesc BufferDesc =
			FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_CandidatesStaging"), MaxParticleCapacity * sizeof(FGPUCandidateParticle), sizeof(FGPUCandidateParticle))
			.AddUsage(BUF_None)
			.SetInitialState(ERHIAccess::CopyDest);
		CandidatesStagingBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
	}

	bBuffersAllocated = true;
	UE_LOG(LogGPUStreamCompaction, Log, TEXT("Stream Compaction buffers allocated (MaxParticles=%d, NumBlocks=%d)"), MaxParticleCapacity, NumBlocks);
}

void FGPUStreamCompactionManager::ReleaseBuffers()
{
	MarkedFlagsBufferRHI.SafeRelease();
	MarkedFlagsSRV.SafeRelease();
	MarkedFlagsUAV.SafeRelease();

	MarkedAABBIndexBufferRHI.SafeRelease();
	MarkedAABBIndexSRV.SafeRelease();
	MarkedAABBIndexUAV.SafeRelease();

	PrefixSumsBufferRHI.SafeRelease();
	PrefixSumsSRV.SafeRelease();
	PrefixSumsUAV.SafeRelease();

	BlockSumsBufferRHI.SafeRelease();
	BlockSumsSRV.SafeRelease();
	BlockSumsUAV.SafeRelease();

	CompactedCandidatesBufferRHI.SafeRelease();
	CompactedCandidatesUAV.SafeRelease();

	TotalCountBufferRHI.SafeRelease();
	TotalCountUAV.SafeRelease();

	FilterAABBsBufferRHI.SafeRelease();
	FilterAABBsSRV.SafeRelease();

	TotalCountStagingBufferRHI.SafeRelease();
	CandidatesStagingBufferRHI.SafeRelease();

	bBuffersAllocated = false;
	bHasFilteredCandidates = false;
	FilteredCandidateCount = 0;
}

//=============================================================================
// AABB Filtering
//=============================================================================

void FGPUStreamCompactionManager::ExecuteAABBFiltering(
	const TArray<FGPUFilterAABB>& FilterAABBs,
	int32 CurrentParticleCount,
	TRefCountPtr<FRDGPooledBuffer> PersistentParticleBuffer,
	FShaderResourceViewRHIRef FallbackParticleSRV)
{
	if (!bIsInitialized || FilterAABBs.Num() == 0 || CurrentParticleCount == 0)
	{
		bHasFilteredCandidates = false;
		FilteredCandidateCount = 0;
		return;
	}

	// Make a copy of the filter AABBs for the render thread
	TArray<FGPUFilterAABB> FilterAABBsCopy = FilterAABBs;
	FGPUStreamCompactionManager* Self = this;
	const int32 ParticleCount = CurrentParticleCount;

	ENQUEUE_RENDER_COMMAND(ExecuteAABBFiltering)(
		[Self, FilterAABBsCopy, ParticleCount, PersistentParticleBuffer, FallbackParticleSRV](FRHICommandListImmediate& RHICmdList)
		{
			// Allocate buffers if needed
			if (!Self->bBuffersAllocated)
			{
				Self->AllocateBuffers(RHICmdList);
			}

			// Upload filter AABBs
			const int32 NumAABBs = FilterAABBsCopy.Num();
			if (!Self->FilterAABBsBufferRHI.IsValid() || Self->CurrentFilterAABBCount < NumAABBs)
			{
				Self->FilterAABBsBufferRHI.SafeRelease();
				Self->FilterAABBsSRV.SafeRelease();

				const FRHIBufferCreateDesc BufferDesc =
					FRHIBufferCreateDesc::CreateStructured(TEXT("StreamCompaction_FilterAABBs"), NumAABBs * sizeof(FGPUFilterAABB), sizeof(FGPUFilterAABB))
					.AddUsage(BUF_ShaderResource)
					.SetInitialState(ERHIAccess::SRVMask);
				Self->FilterAABBsBufferRHI = RHICmdList.CreateBuffer(BufferDesc);
				Self->FilterAABBsSRV = RHICmdList.CreateShaderResourceView(Self->FilterAABBsBufferRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(Self->FilterAABBsBufferRHI));
				Self->CurrentFilterAABBCount = NumAABBs;
			}

			// Upload AABB data
			void* AABBData = RHICmdList.LockBuffer(Self->FilterAABBsBufferRHI, 0,
				NumAABBs * sizeof(FGPUFilterAABB), RLM_WriteOnly);
			FMemory::Memcpy(AABBData, FilterAABBsCopy.GetData(), NumAABBs * sizeof(FGPUFilterAABB));
			RHICmdList.UnlockBuffer(Self->FilterAABBsBufferRHI);

			// Get the correct particle SRV - prefer PersistentParticleBuffer if valid
			FShaderResourceViewRHIRef ParticleSRVToUse = FallbackParticleSRV;
			if (PersistentParticleBuffer.IsValid())
			{
				FBufferRHIRef PersistentRHI = PersistentParticleBuffer->GetRHI();
				if (PersistentRHI.IsValid())
				{
					ParticleSRVToUse = RHICmdList.CreateShaderResourceView(PersistentRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(PersistentRHI));
					UE_LOG(LogGPUStreamCompaction, Log, TEXT("AABB Filtering: Using PersistentParticleBuffer SRV (GPU simulation mode)"));
				}
			}
			else
			{
				UE_LOG(LogGPUStreamCompaction, Warning, TEXT("AABB Filtering: PersistentParticleBuffer not valid, using fallback ParticleSRV"));
			}

			// Execute stream compaction
			Self->DispatchStreamCompactionShaders(RHICmdList, ParticleCount, NumAABBs, ParticleSRVToUse);
		}
	);
}

void FGPUStreamCompactionManager::DispatchStreamCompactionShaders(
	FRHICommandListImmediate& RHICmdList,
	int32 ParticleCount,
	int32 NumAABBs,
	FShaderResourceViewRHIRef InParticleSRV)
{
	const int32 BlockSize = 256;
	const int32 NumBlocks = FMath::DivideAndRoundUp(ParticleCount, BlockSize);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// Pass 1: AABB Mark - Mark particles that are inside any AABB
	{
		TShaderMapRef<FAABBMarkCS> ComputeShader(ShaderMap);
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
		SetComputePipelineState(RHICmdList, ShaderRHI);

		FAABBMarkCS::FParameters Parameters;
		Parameters.Particles = InParticleSRV;
		Parameters.FilterAABBs = FilterAABBsSRV;
		Parameters.MarkedFlags = MarkedFlagsUAV;
		Parameters.MarkedAABBIndex = MarkedAABBIndexUAV;
		Parameters.ParticleCount = ParticleCount;
		Parameters.NumAABBs = NumAABBs;
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

		const int32 NumGroups = FMath::DivideAndRoundUp(ParticleCount, FAABBMarkCS::ThreadGroupSize);
		RHICmdList.DispatchComputeShader(NumGroups, 1, 1);

		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	// UAV barrier between passes
	RHICmdList.Transition(FRHITransitionInfo(MarkedFlagsBufferRHI, ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));

	// Pass 2a: Prefix Sum Block - Blelloch scan within each block
	{
		TShaderMapRef<FPrefixSumBlockCS> ComputeShader(ShaderMap);
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
		SetComputePipelineState(RHICmdList, ShaderRHI);

		FPrefixSumBlockCS::FParameters Parameters;
		Parameters.MarkedFlags = MarkedFlagsSRV;
		Parameters.PrefixSums = PrefixSumsUAV;
		Parameters.BlockSums = BlockSumsUAV;
		Parameters.ElementCount = ParticleCount;
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

		RHICmdList.DispatchComputeShader(NumBlocks, 1, 1);

		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	// UAV barrier
	RHICmdList.Transition(FRHITransitionInfo(BlockSumsBufferRHI, ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));

	// Pass 2b: Scan Block Sums - Sequential scan of block sums
	{
		TShaderMapRef<FScanBlockSumsCS> ComputeShader(ShaderMap);
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
		SetComputePipelineState(RHICmdList, ShaderRHI);

		FScanBlockSumsCS::FParameters Parameters;
		Parameters.BlockSums = BlockSumsUAV;
		Parameters.BlockCount = NumBlocks;
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

		RHICmdList.DispatchComputeShader(1, 1, 1);

		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	// UAV barrier
	RHICmdList.Transition(FRHITransitionInfo(BlockSumsBufferRHI, ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));

	// Pass 2c: Add Block Offsets - Add scanned block sums to each element
	{
		TShaderMapRef<FAddBlockOffsetsCS> ComputeShader(ShaderMap);
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
		SetComputePipelineState(RHICmdList, ShaderRHI);

		FAddBlockOffsetsCS::FParameters Parameters;
		Parameters.PrefixSums = PrefixSumsUAV;
		Parameters.BlockSums = BlockSumsUAV;
		Parameters.ElementCount = ParticleCount;
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

		RHICmdList.DispatchComputeShader(NumBlocks, 1, 1);

		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	// UAV barrier
	RHICmdList.Transition(FRHITransitionInfo(PrefixSumsBufferRHI, ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));

	// Pass 3: Compact - Write marked particles to compacted output
	{
		TShaderMapRef<FCompactCS> ComputeShader(ShaderMap);
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
		SetComputePipelineState(RHICmdList, ShaderRHI);

		FCompactCS::FParameters Parameters;
		Parameters.Particles = InParticleSRV;
		Parameters.MarkedFlags = MarkedFlagsSRV;
		Parameters.PrefixSums = PrefixSumsSRV;
		Parameters.MarkedAABBIndex = MarkedAABBIndexSRV;
		Parameters.CompactedParticles = CompactedCandidatesUAV;
		Parameters.ParticleCount = ParticleCount;
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

		const int32 NumGroups = FMath::DivideAndRoundUp(ParticleCount, FCompactCS::ThreadGroupSize);
		RHICmdList.DispatchComputeShader(NumGroups, 1, 1);

		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	// UAV barrier
	RHICmdList.Transition(FRHITransitionInfo(CompactedCandidatesBufferRHI, ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));

	// Pass 4: Write Total Count
	{
		TShaderMapRef<FWriteTotalCountCS> ComputeShader(ShaderMap);
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
		SetComputePipelineState(RHICmdList, ShaderRHI);

		FWriteTotalCountCS::FParameters Parameters;
		Parameters.MarkedFlagsForCount = MarkedFlagsSRV;
		Parameters.PrefixSumsForCount = PrefixSumsSRV;
		Parameters.TotalCount = TotalCountUAV;
		Parameters.ParticleCount = ParticleCount;
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

		RHICmdList.DispatchComputeShader(1, 1, 1);

		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	// Readback total count
	RHICmdList.Transition(FRHITransitionInfo(TotalCountBufferRHI, ERHIAccess::UAVCompute, ERHIAccess::CopySrc));
	RHICmdList.CopyBufferRegion(TotalCountStagingBufferRHI, 0, TotalCountBufferRHI, 0, sizeof(uint32));

	uint32* CountPtr = (uint32*)RHICmdList.LockBuffer(TotalCountStagingBufferRHI, 0, sizeof(uint32), RLM_ReadOnly);
	FilteredCandidateCount = static_cast<int32>(*CountPtr);
	RHICmdList.UnlockBuffer(TotalCountStagingBufferRHI);

	bHasFilteredCandidates = (FilteredCandidateCount > 0);

	UE_LOG(LogGPUStreamCompaction, Log, TEXT("AABB Filtering complete: %d/%d particles matched %d AABBs"),
		FilteredCandidateCount, ParticleCount, NumAABBs);
}

bool FGPUStreamCompactionManager::GetFilteredCandidates(TArray<FGPUCandidateParticle>& OutCandidates)
{
	if (!bHasFilteredCandidates || FilteredCandidateCount == 0 || !CompactedCandidatesBufferRHI.IsValid())
	{
		OutCandidates.Empty();
		return false;
	}

	FGPUStreamCompactionManager* Self = this;
	TArray<FGPUCandidateParticle>* OutPtr = &OutCandidates;
	const int32 Count = FilteredCandidateCount;

	// Synchronous readback (blocks until GPU is ready)
	ENQUEUE_RENDER_COMMAND(GetFilteredCandidates)(
		[Self, OutPtr, Count](FRHICommandListImmediate& RHICmdList)
		{
			if (!Self->CompactedCandidatesBufferRHI.IsValid())
			{
				return;
			}

			const uint32 CopySize = Count * sizeof(FGPUCandidateParticle);

			// Transition buffer for copy
			RHICmdList.Transition(FRHITransitionInfo(Self->CompactedCandidatesBufferRHI, ERHIAccess::UAVCompute, ERHIAccess::CopySrc));

			// Copy to staging buffer
			RHICmdList.CopyBufferRegion(Self->CandidatesStagingBufferRHI, 0, Self->CompactedCandidatesBufferRHI, 0, CopySize);

			// Read back
			OutPtr->SetNumUninitialized(Count);
			FGPUCandidateParticle* DataPtr = (FGPUCandidateParticle*)RHICmdList.LockBuffer(
				Self->CandidatesStagingBufferRHI, 0, CopySize, RLM_ReadOnly);
			FMemory::Memcpy(OutPtr->GetData(), DataPtr, CopySize);
			RHICmdList.UnlockBuffer(Self->CandidatesStagingBufferRHI);
		}
	);

	// Wait for render command to complete
	FlushRenderingCommands();

	return OutCandidates.Num() > 0;
}

//=============================================================================
// Collision Corrections
//=============================================================================

void FGPUStreamCompactionManager::ApplyCorrections(
	const TArray<FParticleCorrection>& Corrections,
	TRefCountPtr<FRDGPooledBuffer> PersistentParticleBuffer)
{
	if (!bIsInitialized || Corrections.Num() == 0 || !PersistentParticleBuffer.IsValid())
	{
		return;
	}

	// Make a copy of corrections for the render thread
	TArray<FParticleCorrection> CorrectionsCopy = Corrections;
	const int32 CorrectionCount = Corrections.Num();

	ENQUEUE_RENDER_COMMAND(ApplyPerPolygonCorrections)(
		[CorrectionsCopy, CorrectionCount, PersistentParticleBuffer](FRHICommandListImmediate& RHICmdList)
		{
			if (!PersistentParticleBuffer.IsValid())
			{
				UE_LOG(LogGPUStreamCompaction, Warning, TEXT("ApplyCorrections: PersistentParticleBuffer not valid"));
				return;
			}

			// Create corrections buffer
			const FRHIBufferCreateDesc BufferDesc =
				FRHIBufferCreateDesc::CreateStructured(TEXT("PerPolygonCorrections"), CorrectionCount * sizeof(FParticleCorrection), sizeof(FParticleCorrection))
				.AddUsage(BUF_ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);
			FBufferRHIRef CorrectionsBufferRHI = RHICmdList.CreateBuffer(BufferDesc);

			// Upload corrections data
			void* CorrectionData = RHICmdList.LockBuffer(CorrectionsBufferRHI, 0,
				CorrectionCount * sizeof(FParticleCorrection), RLM_WriteOnly);
			FMemory::Memcpy(CorrectionData, CorrectionsCopy.GetData(), CorrectionCount * sizeof(FParticleCorrection));
			RHICmdList.UnlockBuffer(CorrectionsBufferRHI);

			// Create SRV for corrections
			FShaderResourceViewRHIRef CorrectionsSRV = RHICmdList.CreateShaderResourceView(CorrectionsBufferRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(CorrectionsBufferRHI));

			// Create UAV for particles from PersistentParticleBuffer
			FBufferRHIRef ParticleRHI = PersistentParticleBuffer->GetRHI();
			if (!ParticleRHI.IsValid())
			{
				UE_LOG(LogGPUStreamCompaction, Warning, TEXT("ApplyCorrections: Failed to get ParticleRHI from PersistentParticleBuffer"));
				return;
			}
			FUnorderedAccessViewRHIRef ParticlesUAV = RHICmdList.CreateUnorderedAccessView(ParticleRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(ParticleRHI));

			// Dispatch ApplyCorrections compute shader
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FApplyCorrectionsCS> ComputeShader(ShaderMap);
			FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
			SetComputePipelineState(RHICmdList, ShaderRHI);

			FApplyCorrectionsCS::FParameters Parameters;
			Parameters.Corrections = CorrectionsSRV;
			Parameters.Particles = ParticlesUAV;
			Parameters.CorrectionCount = CorrectionCount;
			SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

			const int32 NumGroups = FMath::DivideAndRoundUp(CorrectionCount, FApplyCorrectionsCS::ThreadGroupSize);
			RHICmdList.DispatchComputeShader(NumGroups, 1, 1);

			UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);

			UE_LOG(LogGPUStreamCompaction, Log, TEXT("ApplyCorrections: Applied %d corrections"), CorrectionCount);
		}
	);
}

void FGPUStreamCompactionManager::ApplyAttachmentUpdates(
	const TArray<FAttachedParticleUpdate>& Updates,
	TRefCountPtr<FRDGPooledBuffer> PersistentParticleBuffer)
{
	if (!bIsInitialized || Updates.Num() == 0 || !PersistentParticleBuffer.IsValid())
	{
		return;
	}

	// Make a copy of updates for the render thread
	TArray<FAttachedParticleUpdate> UpdatesCopy = Updates;
	const int32 UpdateCount = Updates.Num();

	ENQUEUE_RENDER_COMMAND(ApplyAttachmentUpdates)(
		[UpdatesCopy, UpdateCount, PersistentParticleBuffer](FRHICommandListImmediate& RHICmdList)
		{
			if (!PersistentParticleBuffer.IsValid())
			{
				UE_LOG(LogGPUStreamCompaction, Warning, TEXT("ApplyAttachmentUpdates: PersistentParticleBuffer not valid"));
				return;
			}

			// Create updates buffer
			const FRHIBufferCreateDesc BufferDesc =
				FRHIBufferCreateDesc::CreateStructured(TEXT("AttachmentUpdates"), UpdateCount * sizeof(FAttachedParticleUpdate), sizeof(FAttachedParticleUpdate))
				.AddUsage(BUF_ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);
			FBufferRHIRef UpdatesBufferRHI = RHICmdList.CreateBuffer(BufferDesc);

			// Upload updates data
			void* UpdateData = RHICmdList.LockBuffer(UpdatesBufferRHI, 0,
				UpdateCount * sizeof(FAttachedParticleUpdate), RLM_WriteOnly);
			FMemory::Memcpy(UpdateData, UpdatesCopy.GetData(), UpdateCount * sizeof(FAttachedParticleUpdate));
			RHICmdList.UnlockBuffer(UpdatesBufferRHI);

			// Create SRV for updates
			FShaderResourceViewRHIRef UpdatesSRV = RHICmdList.CreateShaderResourceView(UpdatesBufferRHI, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(UpdatesBufferRHI));

			// Create UAV for particles from PersistentParticleBuffer
			FBufferRHIRef ParticleRHI = PersistentParticleBuffer->GetRHI();
			if (!ParticleRHI.IsValid())
			{
				UE_LOG(LogGPUStreamCompaction, Warning, TEXT("ApplyAttachmentUpdates: Failed to get ParticleRHI from PersistentParticleBuffer"));
				return;
			}
			FUnorderedAccessViewRHIRef ParticlesUAV = RHICmdList.CreateUnorderedAccessView(ParticleRHI, FRHIViewDesc::CreateBufferUAV().SetTypeFromBuffer(ParticleRHI));

			// Dispatch ApplyAttachmentUpdates compute shader
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FApplyAttachmentUpdatesCS> ComputeShader(ShaderMap);
			FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
			SetComputePipelineState(RHICmdList, ShaderRHI);

			FApplyAttachmentUpdatesCS::FParameters Parameters;
			Parameters.AttachmentUpdates = UpdatesSRV;
			Parameters.Particles = ParticlesUAV;
			Parameters.UpdateCount = UpdateCount;
			SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);

			const int32 NumGroups = FMath::DivideAndRoundUp(UpdateCount, FApplyAttachmentUpdatesCS::ThreadGroupSize);
			RHICmdList.DispatchComputeShader(NumGroups, 1, 1);

			UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);

			UE_LOG(LogGPUStreamCompaction, Verbose, TEXT("ApplyAttachmentUpdates: Applied %d updates"), UpdateCount);
		}
	);
}
