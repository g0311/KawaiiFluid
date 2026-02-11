// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// FGPUZOrderSortManager - Z-Order Morton Code Sorting System

#include "GPU/Managers/GPUZOrderSortManager.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "GPU/GPUBoundaryAttachment.h"  // For FGPUBoneDeltaAttachment
#include "GPU/GPUIndirectDispatchUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPUZOrderSort, Log, All);
DEFINE_LOG_CATEGORY(LogGPUZOrderSort);

//=============================================================================
// Constructor / Destructor
//=============================================================================

FGPUZOrderSortManager::FGPUZOrderSortManager()
	: bIsInitialized(false)
	, ZOrderBufferParticleCapacity(0)
{
}

FGPUZOrderSortManager::~FGPUZOrderSortManager()
{
	Release();
}

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Initialize the manager.
 */
void FGPUZOrderSortManager::Initialize()
{
	bIsInitialized = true;
	UE_LOG(LogGPUZOrderSort, Log, TEXT("GPUZOrderSortManager initialized"));
}

/**
 * @brief Release all resources.
 */
void FGPUZOrderSortManager::Release()
{
	ZOrderBufferParticleCapacity = 0;
	bIsInitialized = false;
	UE_LOG(LogGPUZOrderSort, Log, TEXT("GPUZOrderSortManager released"));
}

//=============================================================================
// Z-Order (Morton Code) Sorting Pipeline
//=============================================================================

/**
 * @brief Execute the complete Z-Order sorting pipeline.
 * @param GraphBuilder RDG builder.
 * @param InParticleBuffer Input particle buffer.
 * @param OutCellStartUAV Output cell start indices UAV.
 * @param OutCellStartSRV Output cell start indices SRV.
 * @param OutCellEndUAV Output cell end indices UAV.
 * @param OutCellEndSRV Output cell end indices SRV.
 * @param OutCellStartBuffer Output cell start buffer ref.
 * @param OutCellEndBuffer Output cell end buffer ref.
 * @param CurrentParticleCount Number of particles.
 * @param Params Simulation parameters.
 * @param AllocParticleCount Buffer allocation size.
 * @param InAttachmentBuffer Optional attachment buffer to reorder.
 * @param OutSortedAttachmentBuffer Optional output for sorted attachments.
 * @param IndirectArgsBuffer Optional indirect dispatch arguments.
 * @return Sorted particle buffer.
 */
FRDGBufferRef FGPUZOrderSortManager::ExecuteZOrderSortingPipeline(
	FRDGBuilder& GraphBuilder,
	FRDGBufferRef InParticleBuffer,
	FRDGBufferUAVRef& OutCellStartUAV,
	FRDGBufferSRVRef& OutCellStartSRV,
	FRDGBufferUAVRef& OutCellEndUAV,
	FRDGBufferSRVRef& OutCellEndSRV,
	FRDGBufferRef& OutCellStartBuffer,
	FRDGBufferRef& OutCellEndBuffer,
	int32 CurrentParticleCount,
	const FGPUFluidSimulationParams& Params,
	int32 AllocParticleCount,
	FRDGBufferRef InAttachmentBuffer,
	FRDGBufferRef* OutSortedAttachmentBuffer,
	FRDGBufferRef IndirectArgsBuffer)
{
	RDG_EVENT_SCOPE(GraphBuilder, "GPUFluid::ZOrderSorting");

	// Use AllocParticleCount for buffer sizing to avoid D3D12 pool allocation hitches
	if (AllocParticleCount <= 0)
	{
		AllocParticleCount = CurrentParticleCount;
	}

	if (CurrentParticleCount <= 0)
	{
		// Still create valid CellStart/CellEnd buffers so callers never get null SRVs
		static uint32 InvalidIndex = 0xFFFFFFFF;
		OutCellStartBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("ZOrder.CellStart.Empty"));
		OutCellEndBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("ZOrder.CellEnd.Empty"));
		GraphBuilder.QueueBufferUpload(OutCellStartBuffer, &InvalidIndex, sizeof(uint32));
		GraphBuilder.QueueBufferUpload(OutCellEndBuffer, &InvalidIndex, sizeof(uint32));
		OutCellStartUAV = GraphBuilder.CreateUAV(OutCellStartBuffer);
		OutCellStartSRV = GraphBuilder.CreateSRV(OutCellStartBuffer);
		OutCellEndUAV = GraphBuilder.CreateUAV(OutCellEndBuffer);
		OutCellEndSRV = GraphBuilder.CreateSRV(OutCellEndBuffer);
		return InParticleBuffer;
	}

	// Get grid parameters from current preset
	// CRITICAL: Hybrid Tiled Z-Order mode uses fixed 21-bit keys (3-bit TileHash + 18-bit LocalMorton)
	// The key range is always 0 to 2^21-1, regardless of volume size.
	// Therefore, in Hybrid mode we MUST use Medium preset (7-bit = 2^21 cells) to match the key range.
	// Using smaller presets would truncate the key and cause cell ID mismatches.
	const EGridResolutionPreset EffectivePreset = bUseHybridTiledZOrder ? EGridResolutionPreset::Medium : GridResolutionPreset;
	const int32 GridAxisBits = GridResolutionPresetHelper::GetAxisBits(EffectivePreset);
	const int32 GridSize = GridResolutionPresetHelper::GetGridResolution(EffectivePreset);
	const int32 CellCount = GridResolutionPresetHelper::GetMaxCells(EffectivePreset);

	const int32 NumBlocks = FMath::DivideAndRoundUp(CurrentParticleCount, 256);

	//=========================================================================
	// Step 1: Create Morton code and index buffers
	//=========================================================================
	FRDGBufferRef MortonCodesRDG;
	FRDGBufferRef MortonCodesTempRDG;
	FRDGBufferRef SortIndicesRDG;
	FRDGBufferRef SortIndicesTempRDG;
	FRDGBufferRef CellStartRDG;
	FRDGBufferRef CellEndRDG;

	// Morton codes and indices
	{
		FRDGBufferDesc MortonDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), AllocParticleCount);
		MortonCodesRDG = GraphBuilder.CreateBuffer(MortonDesc, TEXT("GPUFluid.MortonCodes"));
		MortonCodesTempRDG = GraphBuilder.CreateBuffer(MortonDesc, TEXT("GPUFluid.MortonCodesTemp"));
		SortIndicesRDG = GraphBuilder.CreateBuffer(MortonDesc, TEXT("GPUFluid.SortIndices"));
		SortIndicesTempRDG = GraphBuilder.CreateBuffer(MortonDesc, TEXT("GPUFluid.SortIndicesTemp"));
	}

	// Radix sort uses stale CPU CurrentParticleCount which may differ from GPU-accurate count.
	// ComputeMortonCodes (indirect) writes only GPU-accurate count entries.
	// Without clearing, entries beyond GPU count are uninitialized garbage.
	// - MortonCodes: 0xFFFFFFFF → OOB entries sort to end (stale-HIGH protection)
	// - SortIndices: 0 → prevents crash from reading uninitialized indices
	//   (critical when GPU count=0 after full despawn but stale readback > 0)
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(MortonCodesRDG), 0xFFFFFFFFu);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(SortIndicesRDG), 0u);

	// Cell Start/End
	{
		FRDGBufferDesc CellDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), CellCount);
		CellStartRDG = GraphBuilder.CreateBuffer(CellDesc, TEXT("GPUFluid.CellStart"));
		CellEndRDG = GraphBuilder.CreateBuffer(CellDesc, TEXT("GPUFluid.CellEnd"));
	}

	//=========================================================================
	// Step 2: Compute Morton codes
	//=========================================================================
	{
		FRDGBufferSRVRef ParticlesSRV = GraphBuilder.CreateSRV(InParticleBuffer);
		FRDGBufferUAVRef MortonCodesUAV = GraphBuilder.CreateUAV(MortonCodesRDG);
		FRDGBufferUAVRef IndicesUAV = GraphBuilder.CreateUAV(SortIndicesRDG);

		AddComputeMortonCodesPass(GraphBuilder, ParticlesSRV, MortonCodesUAV, IndicesUAV, CurrentParticleCount, Params, IndirectArgsBuffer);
	}

	//=========================================================================
	// Step 3: Radix Sort
	//=========================================================================
	AddRadixSortPasses(GraphBuilder, MortonCodesRDG, SortIndicesRDG, CurrentParticleCount, AllocParticleCount);

	//=========================================================================
	// Step 4: Reorder particle data based on sorted indices
	// Also reorder BoneDeltaAttachment buffer if provided (keeps indices synchronized)
	//=========================================================================
	FRDGBufferRef SortedParticleBuffer;
	FRDGBufferRef SortedAttachmentBuffer = nullptr;
	{
		FRDGBufferDesc SortedDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUFluidParticle), AllocParticleCount);
		SortedParticleBuffer = GraphBuilder.CreateBuffer(SortedDesc, TEXT("GPUFluid.SortedParticles"));

		FRDGBufferSRVRef OldParticlesSRV = GraphBuilder.CreateSRV(InParticleBuffer);
		FRDGBufferSRVRef SortedIndicesSRV = GraphBuilder.CreateSRV(SortIndicesRDG);
		FRDGBufferUAVRef SortedParticlesUAV = GraphBuilder.CreateUAV(SortedParticleBuffer);

		// Optional: Create sorted attachment buffer if input is provided
		FRDGBufferSRVRef OldAttachmentsSRV = nullptr;
		FRDGBufferUAVRef SortedAttachmentsUAV = nullptr;
		if (InAttachmentBuffer)
		{
			FRDGBufferDesc AttachmentDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUBoneDeltaAttachment), AllocParticleCount);
			SortedAttachmentBuffer = GraphBuilder.CreateBuffer(AttachmentDesc, TEXT("GPUFluid.SortedBoneDeltaAttachments"));
			OldAttachmentsSRV = GraphBuilder.CreateSRV(InAttachmentBuffer);
			SortedAttachmentsUAV = GraphBuilder.CreateUAV(SortedAttachmentBuffer);
		}

		AddReorderParticlesPass(GraphBuilder, OldParticlesSRV, SortedIndicesSRV, SortedParticlesUAV, CurrentParticleCount,
			OldAttachmentsSRV, SortedAttachmentsUAV, IndirectArgsBuffer);

		// Output sorted attachment buffer if requested
		if (OutSortedAttachmentBuffer && SortedAttachmentBuffer)
		{
			*OutSortedAttachmentBuffer = SortedAttachmentBuffer;
		}
	}

	//=========================================================================
	// Step 5: Compute Cell Start/End indices
	//=========================================================================
	{
		FRDGBufferSRVRef SortedMortonSRV = GraphBuilder.CreateSRV(MortonCodesRDG);
		OutCellStartUAV = GraphBuilder.CreateUAV(CellStartRDG);
		OutCellEndUAV = GraphBuilder.CreateUAV(CellEndRDG);

		AddComputeCellStartEndPass(GraphBuilder, SortedMortonSRV, OutCellStartUAV, OutCellEndUAV, CurrentParticleCount, IndirectArgsBuffer);

		OutCellStartSRV = GraphBuilder.CreateSRV(CellStartRDG);
		OutCellEndSRV = GraphBuilder.CreateSRV(CellEndRDG);

		// Output buffer refs for persistent extraction (Ray Marching)
		OutCellStartBuffer = CellStartRDG;
		OutCellEndBuffer = CellEndRDG;
	}

	ZOrderBufferParticleCapacity = CurrentParticleCount;

	return SortedParticleBuffer;
}

/**
 * @brief Step 1: Compute Morton codes from particle positions.
 * @param GraphBuilder RDG builder.
 * @param ParticlesSRV Input particles SRV.
 * @param MortonCodesUAV Output Morton codes UAV.
 * @param InParticleIndicesUAV Output particle indices UAV.
 * @param CurrentParticleCount Number of particles.
 * @param Params Simulation parameters.
 * @param IndirectArgsBuffer Optional indirect dispatch arguments.
 */
void FGPUZOrderSortManager::AddComputeMortonCodesPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef ParticlesSRV,
	FRDGBufferUAVRef MortonCodesUAV,
	FRDGBufferUAVRef InParticleIndicesUAV,
	int32 CurrentParticleCount,
	const FGPUFluidSimulationParams& Params,
	FRDGBufferRef IndirectArgsBuffer)
{
	// Validate CellSize
	if (Params.CellSize <= 0.0f)
	{
		UE_LOG(LogGPUZOrderSort, Error, TEXT("Morton code ERROR: Invalid CellSize (%.4f)!"), Params.CellSize);
	}

	// Get grid parameters - use Medium preset in Hybrid mode for 21-bit keys
	const EGridResolutionPreset EffectivePreset = bUseHybridTiledZOrder ? EGridResolutionPreset::Medium : GridResolutionPreset;
	const int32 GridSize = GridResolutionPresetHelper::GetGridResolution(EffectivePreset);

	// Validate bounds fit within Morton code capacity (Classic mode only)
	// Hybrid mode has unlimited range via hash-based tile addressing
	if (!bUseHybridTiledZOrder)
	{
		const float CellSizeToUse = FMath::Max(Params.CellSize, 0.001f);
		const float MaxExtent = static_cast<float>(GridSize) * CellSizeToUse;
		const FVector3f BoundsExtent = SimulationBoundsMax - SimulationBoundsMin;

		if (BoundsExtent.X > MaxExtent || BoundsExtent.Y > MaxExtent || BoundsExtent.Z > MaxExtent)
		{
			UE_LOG(LogGPUZOrderSort, Warning,
				TEXT("Morton code bounds overflow! BoundsExtent(%.1f, %.1f, %.1f) exceeds MaxExtent(%.1f). Preset=%d"),
				BoundsExtent.X, BoundsExtent.Y, BoundsExtent.Z, MaxExtent, static_cast<int32>(GridResolutionPreset));
		}
	}

	// Get shader with correct permutation
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	FComputeMortonCodesCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FGridResolutionDim>(GridResolutionPermutation::FromPreset(EffectivePreset));
	TShaderMapRef<FComputeMortonCodesCS> ComputeShader(ShaderMap, PermutationVector);

	FComputeMortonCodesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeMortonCodesCS::FParameters>();
	PassParameters->Particles = ParticlesSRV;
	PassParameters->MortonCodes = MortonCodesUAV;
	PassParameters->ParticleIndices = InParticleIndicesUAV;
	PassParameters->ParticleCount = CurrentParticleCount;
	if (IndirectArgsBuffer)
	{
		PassParameters->ParticleCountBuffer = GraphBuilder.CreateSRV(IndirectArgsBuffer);
	}
	PassParameters->BoundsMin = SimulationBoundsMin;
	PassParameters->BoundsExtent = SimulationBoundsMax - SimulationBoundsMin;
	PassParameters->CellSize = Params.CellSize;
	PassParameters->bUseHybridTiledZOrder = bUseHybridTiledZOrder ? 1 : 0;

	if (IndirectArgsBuffer)
	{
		GPUIndirectDispatch::AddIndirectComputePass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::ComputeMortonCodes(%d,Preset=%d,Hybrid=%d)", CurrentParticleCount, static_cast<int32>(EffectivePreset), bUseHybridTiledZOrder ? 1 : 0),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			GPUIndirectDispatch::IndirectArgsOffset_TG256);
	}
	else
	{
		const int32 NumGroups = FMath::DivideAndRoundUp(CurrentParticleCount, FComputeMortonCodesCS::ThreadGroupSize);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::ComputeMortonCodes(%d,Preset=%d,Hybrid=%d)", CurrentParticleCount, static_cast<int32>(EffectivePreset), bUseHybridTiledZOrder ? 1 : 0),
			ComputeShader,
			PassParameters,
			FIntVector(NumGroups, 1, 1)
		);
	}
}

/**
 * @brief Step 2: GPU Radix Sort (sorts Morton codes + particle indices).
 * @param GraphBuilder RDG builder.
 * @param InOutMortonCodes Input/Output Morton codes.
 * @param InOutParticleIndices Input/Output particle indices.
 * @param ParticleCount Number of particles to sort.
 * @param AllocParticleCount Allocation size.
 */
void FGPUZOrderSortManager::AddRadixSortPasses(
	FRDGBuilder& GraphBuilder,
	FRDGBufferRef& InOutMortonCodes,
	FRDGBufferRef& InOutParticleIndices,
	int32 ParticleCount,
	int32 AllocParticleCount)
{
	if (AllocParticleCount <= 0)
	{
		AllocParticleCount = ParticleCount;
	}
	if (ParticleCount <= 0)
	{
		return;
	}

	// Calculate radix sort passes based on Morton code bits from preset
	int32 SortKeyBits;
	if (bUseHybridTiledZOrder)
	{
		// Hybrid Tiled Z-Order: 21-bit keys (3-bit TileHash + 18-bit LocalMorton)
		SortKeyBits = 21;
	}
	else
	{
		// Classic Morton code: 18/21/24-bit keys based on preset
		const int32 GridAxisBits = GridResolutionPresetHelper::GetAxisBits(GridResolutionPreset);
		SortKeyBits = GridAxisBits * 3;
	}
	int32 RadixSortPasses = (SortKeyBits + GPU_RADIX_BITS - 1) / GPU_RADIX_BITS;
	// Round up to even for ping-pong buffer to end in original buffer
	if (RadixSortPasses % 2 != 0)
	{
		RadixSortPasses++;
	}

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	const int32 NumBlocks = FMath::DivideAndRoundUp(ParticleCount, GPU_RADIX_ELEMENTS_PER_GROUP);
	const int32 RequiredHistogramSize = GPU_RADIX_SIZE * NumBlocks;

	// Create transient ping-pong buffers
	FRDGBufferDesc KeysTempDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), AllocParticleCount);
	FRDGBufferRef KeysTemp = GraphBuilder.CreateBuffer(KeysTempDesc, TEXT("RadixSort.KeysTemp"));

	FRDGBufferDesc ValuesTempDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), AllocParticleCount);
	FRDGBufferRef ValuesTemp = GraphBuilder.CreateBuffer(ValuesTempDesc, TEXT("RadixSort.ValuesTemp"));

	FRDGBufferDesc HistogramDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), RequiredHistogramSize);
	FRDGBufferRef Histogram = GraphBuilder.CreateBuffer(HistogramDesc, TEXT("RadixSort.Histogram"));

	FRDGBufferDesc BucketOffsetsDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GPU_RADIX_SIZE);
	FRDGBufferRef BucketOffsets = GraphBuilder.CreateBuffer(BucketOffsetsDesc, TEXT("RadixSort.BucketOffsets"));

	// Ping-pong buffers
	FRDGBufferRef Keys[2] = { InOutMortonCodes, KeysTemp };
	FRDGBufferRef Values[2] = { InOutParticleIndices, ValuesTemp };
	int32 BufferIndex = 0;

	for (int32 Pass = 0; Pass < RadixSortPasses; ++Pass)
	{
		const int32 BitOffset = Pass * GPU_RADIX_BITS;
		const int32 SrcIndex = BufferIndex;
		const int32 DstIndex = BufferIndex ^ 1;

		RDG_EVENT_SCOPE(GraphBuilder, "RadixSort Pass %d (bits %d-%d)", Pass, BitOffset, BitOffset + 7);

		// Pass 1: Histogram
		{
			TShaderMapRef<FRadixSortHistogramCS> HistogramShader(ShaderMap);
			FRadixSortHistogramCS::FParameters* Params = GraphBuilder.AllocParameters<FRadixSortHistogramCS::FParameters>();
			Params->KeysIn = GraphBuilder.CreateSRV(Keys[SrcIndex]);
			Params->ValuesIn = GraphBuilder.CreateSRV(Values[SrcIndex]);
			Params->Histogram = GraphBuilder.CreateUAV(Histogram);
			Params->ElementCount = ParticleCount;
			Params->BitOffset = BitOffset;
			Params->NumGroups = NumBlocks;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Histogram"), HistogramShader, Params, FIntVector(NumBlocks, 1, 1));
		}

		// Pass 2a: Global Prefix Sum
		{
			TShaderMapRef<FRadixSortGlobalPrefixSumCS> PrefixSumShader(ShaderMap);
			FRadixSortGlobalPrefixSumCS::FParameters* Params = GraphBuilder.AllocParameters<FRadixSortGlobalPrefixSumCS::FParameters>();
			Params->Histogram = GraphBuilder.CreateUAV(Histogram);
			Params->GlobalOffsets = GraphBuilder.CreateUAV(BucketOffsets);
			Params->NumGroups = NumBlocks;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("GlobalPrefixSum"), PrefixSumShader, Params, FIntVector(1, 1, 1));
		}

		// Pass 2b: Bucket Prefix Sum
		{
			TShaderMapRef<FRadixSortBucketPrefixSumCS> BucketSumShader(ShaderMap);
			FRadixSortBucketPrefixSumCS::FParameters* Params = GraphBuilder.AllocParameters<FRadixSortBucketPrefixSumCS::FParameters>();
			Params->GlobalOffsets = GraphBuilder.CreateUAV(BucketOffsets);

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BucketPrefixSum"), BucketSumShader, Params, FIntVector(1, 1, 1));
		}

		// Pass 3: Scatter
		{
			TShaderMapRef<FRadixSortScatterCS> ScatterShader(ShaderMap);
			FRadixSortScatterCS::FParameters* Params = GraphBuilder.AllocParameters<FRadixSortScatterCS::FParameters>();
			Params->KeysIn = GraphBuilder.CreateSRV(Keys[SrcIndex]);
			Params->ValuesIn = GraphBuilder.CreateSRV(Values[SrcIndex]);
			Params->KeysOut = GraphBuilder.CreateUAV(Keys[DstIndex]);
			Params->ValuesOut = GraphBuilder.CreateUAV(Values[DstIndex]);
			Params->HistogramSRV = GraphBuilder.CreateSRV(Histogram);
			Params->GlobalOffsetsSRV = GraphBuilder.CreateSRV(BucketOffsets);
			Params->ElementCount = ParticleCount;
			Params->BitOffset = BitOffset;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Scatter"), ScatterShader, Params, FIntVector(NumBlocks, 1, 1));
		}

		BufferIndex ^= 1;
	}

	InOutMortonCodes = Keys[BufferIndex];
	InOutParticleIndices = Values[BufferIndex];
}

/**
 * @brief Step 3: Reorder particle data based on sorted indices.
 * @param GraphBuilder RDG builder.
 * @param OldParticlesSRV Input particles SRV.
 * @param SortedIndicesSRV Sorted indices SRV.
 * @param SortedParticlesUAV Output sorted particles UAV.
 * @param CurrentParticleCount Number of particles.
 * @param OldAttachmentsSRV Optional input attachments.
 * @param SortedAttachmentsUAV Optional output sorted attachments.
 * @param IndirectArgsBuffer Optional indirect dispatch arguments.
 */
void FGPUZOrderSortManager::AddReorderParticlesPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef OldParticlesSRV,
	FRDGBufferSRVRef SortedIndicesSRV,
	FRDGBufferUAVRef SortedParticlesUAV,
	int32 CurrentParticleCount,
	FRDGBufferSRVRef OldAttachmentsSRV,
	FRDGBufferUAVRef SortedAttachmentsUAV,
	FRDGBufferRef IndirectArgsBuffer)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FReorderParticlesCS> ComputeShader(ShaderMap);

	// Determine if we should reorder attachments
	const bool bReorderAttachments = (OldAttachmentsSRV != nullptr && SortedAttachmentsUAV != nullptr);

	// Create dummy buffers if attachments are not provided (shader requires non-null parameters)
	FRDGBufferSRVRef AttachmentsSRV = OldAttachmentsSRV;
	FRDGBufferUAVRef AttachmentsUAV = SortedAttachmentsUAV;
	if (!bReorderAttachments)
	{
		// Create minimal dummy buffer (1 element) to satisfy shader parameter requirements
		FRDGBufferDesc DummyDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUBoneDeltaAttachment), 1);
		FRDGBufferRef DummyBuffer = GraphBuilder.CreateBuffer(DummyDesc, TEXT("GPUFluid.DummyAttachment"));
		// Clear dummy buffer to satisfy RDG validation (buffer must be written before read)
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DummyBuffer), 0);
		AttachmentsSRV = GraphBuilder.CreateSRV(DummyBuffer);
		AttachmentsUAV = GraphBuilder.CreateUAV(DummyBuffer);
	}

	FReorderParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReorderParticlesCS::FParameters>();
	PassParameters->OldParticles = OldParticlesSRV;
	PassParameters->SortedIndices = SortedIndicesSRV;
	PassParameters->SortedParticles = SortedParticlesUAV;
	// BoneDeltaAttachment reordering (dummy buffer used if not actually reordering)
	PassParameters->OldBoneDeltaAttachments = AttachmentsSRV;
	PassParameters->SortedBoneDeltaAttachments = AttachmentsUAV;
	PassParameters->bReorderAttachments = bReorderAttachments ? 1 : 0;
	PassParameters->ParticleCount = CurrentParticleCount;
	if (IndirectArgsBuffer)
	{
		PassParameters->ParticleCountBuffer = GraphBuilder.CreateSRV(IndirectArgsBuffer);
	}

	if (IndirectArgsBuffer)
	{
		GPUIndirectDispatch::AddIndirectComputePass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::ReorderParticles(%d)%s", CurrentParticleCount,
				bReorderAttachments ? TEXT("+Attachments") : TEXT("")),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			GPUIndirectDispatch::IndirectArgsOffset_TG256);
	}
	else
	{
		const int32 NumGroups = FMath::DivideAndRoundUp(CurrentParticleCount, FReorderParticlesCS::ThreadGroupSize);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::ReorderParticles(%d)%s", CurrentParticleCount,
				bReorderAttachments ? TEXT("+Attachments") : TEXT("")),
			ComputeShader,
			PassParameters,
			FIntVector(NumGroups, 1, 1)
		);
	}
}

/**
 * @brief Step 4: Compute Cell Start/End indices from sorted Morton codes.
 * @param GraphBuilder RDG builder.
 * @param SortedMortonCodesSRV Input sorted Morton codes SRV.
 * @param CellStartUAV Output cell start indices UAV.
 * @param CellEndUAV Output cell end indices UAV.
 * @param CurrentParticleCount Number of particles.
 * @param IndirectArgsBuffer Optional indirect dispatch arguments.
 */
void FGPUZOrderSortManager::AddComputeCellStartEndPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef SortedMortonCodesSRV,
	FRDGBufferUAVRef CellStartUAV,
	FRDGBufferUAVRef CellEndUAV,
	int32 CurrentParticleCount,
	FRDGBufferRef IndirectArgsBuffer)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// Get cell count from preset
	// CRITICAL: Hybrid Tiled Z-Order uses fixed 21-bit keys, so we must use Medium preset (2^21 cells)
	const EGridResolutionPreset EffectivePreset = bUseHybridTiledZOrder ? EGridResolutionPreset::Medium : GridResolutionPreset;
	const int32 CellCount = GridResolutionPresetHelper::GetMaxCells(EffectivePreset);

	// Create permutation vector for effective preset
	FClearCellIndicesCS::FPermutationDomain ClearPermutation;
	ClearPermutation.Set<FGridResolutionDim>(GridResolutionPermutation::FromPreset(EffectivePreset));

	FComputeCellStartEndCS::FPermutationDomain CellStartEndPermutation;
	CellStartEndPermutation.Set<FGridResolutionDim>(GridResolutionPermutation::FromPreset(EffectivePreset));

	// Step 1: Clear cell indices
	{
		TShaderMapRef<FClearCellIndicesCS> ClearShader(ShaderMap, ClearPermutation);
		FClearCellIndicesCS::FParameters* ClearParams = GraphBuilder.AllocParameters<FClearCellIndicesCS::FParameters>();
		ClearParams->CellStart = CellStartUAV;
		ClearParams->CellEnd = CellEndUAV;

		const int32 NumGroups = FMath::DivideAndRoundUp(CellCount, FClearCellIndicesCS::ThreadGroupSize);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUFluid::ClearCellIndices(%d,Preset=%d)", CellCount, static_cast<int32>(EffectivePreset)),
			ClearShader,
			ClearParams,
			FIntVector(NumGroups, 1, 1)
		);
	}

	// Step 2: Compute cell start/end
	{
		TShaderMapRef<FComputeCellStartEndCS> ComputeShader(ShaderMap, CellStartEndPermutation);
		FComputeCellStartEndCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeCellStartEndCS::FParameters>();
		PassParameters->SortedMortonCodes = SortedMortonCodesSRV;
		PassParameters->CellStart = CellStartUAV;
		PassParameters->CellEnd = CellEndUAV;
		PassParameters->ParticleCount = CurrentParticleCount;
		if (IndirectArgsBuffer)
		{
			PassParameters->ParticleCountBuffer = GraphBuilder.CreateSRV(IndirectArgsBuffer);
		}

		if (IndirectArgsBuffer)
		{
			GPUIndirectDispatch::AddIndirectComputePass(
				GraphBuilder,
				RDG_EVENT_NAME("GPUFluid::ComputeCellStartEnd(%d,Preset=%d)", CurrentParticleCount, static_cast<int32>(EffectivePreset)),
				ComputeShader,
				PassParameters,
				IndirectArgsBuffer,
				GPUIndirectDispatch::IndirectArgsOffset_TG512);
		}
		else
		{
			const int32 NumGroups = FMath::DivideAndRoundUp(CurrentParticleCount, FComputeCellStartEndCS::ThreadGroupSize);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GPUFluid::ComputeCellStartEnd(%d,Preset=%d)", CurrentParticleCount, static_cast<int32>(EffectivePreset)),
				ComputeShader,
				PassParameters,
				FIntVector(NumGroups, 1, 1)
			);
		}
	}
}
