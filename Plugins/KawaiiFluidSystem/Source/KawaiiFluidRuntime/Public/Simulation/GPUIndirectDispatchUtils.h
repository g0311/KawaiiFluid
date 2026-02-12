// Copyright 2026 Team_Bruteforce. All Rights Reserved.
// Indirect Dispatch Utilities for GPU Fluid Simulation
// Provides helper functions for DispatchIndirect compute passes

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RHICommandList.h"

/**
 * @namespace GPUIndirectDispatch
 * @brief Indirect Dispatch Utilities for GPU Fluid Simulation.
 * 
 * Provides helper functions for DispatchIndirect compute passes and manages
 * the layout of PersistentParticleCountBuffer.
 * 
 * PersistentParticleCountBuffer Layout (44 bytes = 11 x uint32):
 * - Offset  0: uint32 GroupCountX_256  // ceil(Count / 256)
 * - Offset  4: uint32 1               // GroupCountY
 * - Offset  8: uint32 1               // GroupCountZ
 * - Offset 12: uint32 GroupCountX_512  // ceil(Count / 512)
 * - Offset 16: uint32 1
 * - Offset 20: uint32 1
 * - Offset 24: uint32 ParticleCount   // raw count
 * - Offset 28: uint32 4               // VertexCountPerInstance (tri-strip quad)
 * - Offset 32: uint32 InstanceCount   // = ParticleCount
 * - Offset 36: uint32 0               // StartVertexLocation
 * - Offset 40: uint32 0               // StartInstanceLocation
 */
namespace GPUIndirectDispatch
{
	/** @brief Byte offset for TG=256 indirect args (GroupCountX, Y, Z). */
	static constexpr uint32 IndirectArgsOffset_TG256 = 0;

	/** @brief Byte offset for TG=512 indirect args (GroupCountX, Y, Z). */
	static constexpr uint32 IndirectArgsOffset_TG512 = 12;

	/** @brief Element index for raw particle count (for SRV reads). */
	static constexpr uint32 ParticleCountElementIndex = 6;

	/** @brief Byte offset for raw particle count. */
	static constexpr uint32 ParticleCountByteOffset = 24;

	/** @brief Byte offset for DrawInstancedIndirect args (4 x uint32). */
	static constexpr uint32 DrawIndirectArgsOffset = 28;

	/** @brief Total buffer size in bytes. */
	static constexpr uint32 BufferSizeBytes = 44;

	/** @brief Total buffer size in uint32 elements. */
	static constexpr uint32 BufferSizeElements = 11;

	/**
	 * @brief Create initial data for PersistentParticleCountBuffer.
	 * @param ParticleCount Current particle count.
	 * @param OutData Output array (must have 11 elements).
	 */
	inline void BuildInitData(uint32 ParticleCount, uint32 OutData[BufferSizeElements])
	{
		OutData[0] = FMath::DivideAndRoundUp(ParticleCount, 256u);  // GroupCountX_256
		OutData[1] = 1;                                              // GroupCountY
		OutData[2] = 1;                                              // GroupCountZ
		OutData[3] = FMath::DivideAndRoundUp(ParticleCount, 512u);  // GroupCountX_512
		OutData[4] = 1;                                              // GroupCountY
		OutData[5] = 1;                                              // GroupCountZ
		OutData[6] = ParticleCount;                                  // Raw count
		OutData[7] = 4;                                              // VertexCountPerInstance (tri-strip quad)
		OutData[8] = ParticleCount;                                  // InstanceCount
		OutData[9] = 0;                                              // StartVertexLocation
		OutData[10] = 0;                                             // StartInstanceLocation
	}

	/**
	 * @brief Build zero-count initial data (for ClearCachedParticles).
	 * @param OutData Output array (must have 11 elements).
	 */
	inline void BuildZeroData(uint32 OutData[BufferSizeElements])
	{
		OutData[0] = 0; OutData[1] = 1; OutData[2] = 1;
		OutData[3] = 0; OutData[4] = 1; OutData[5] = 1;
		OutData[6] = 0;
		OutData[7] = 4; OutData[8] = 0; OutData[9] = 0; OutData[10] = 0;
	}

	/**
	 * @brief Add an indirect compute pass using DispatchIndirect.
	 * 
	 * This is the indirect dispatch equivalent of FComputeShaderUtils::AddPass.
	 * Instead of providing FIntVector group counts, it reads them from IndirectArgsBuffer.
	 *
	 * @param GraphBuilder RDG builder.
	 * @param EventName RDG event name.
	 * @param ComputeShader Compute shader reference.
	 * @param Parameters Shader parameters.
	 * @param IndirectArgsBuffer Buffer containing dispatch args (3 x uint32).
	 * @param IndirectArgsOffset Byte offset into IndirectArgsBuffer.
	 */
	template<typename ShaderType>
	void AddIndirectComputePass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& EventName,
		const TShaderRef<ShaderType>& ComputeShader,
		typename ShaderType::FParameters* Parameters,
		FRDGBufferRef IndirectArgsBuffer,
		uint32 IndirectArgsOffset)
	{
		// Create a pass parameters struct that wraps the shader params + indirect args
		// We use the shader's native FParameters and add IndirectArgs access in the lambda
		GraphBuilder.AddPass(
			MoveTemp(EventName),
			Parameters,
			ERDGPassFlags::Compute,
			[Parameters, ComputeShader, IndirectArgsBuffer, IndirectArgsOffset]
			(FRHIComputeCommandList& RHICmdList)
			{
				FComputeShaderUtils::ValidateGroupCount(FIntVector(1, 1, 1));  // Just for validation
				SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());
				SetShaderParameters(RHICmdList, ComputeShader, ComputeShader.GetComputeShader(), *Parameters);
				RHICmdList.DispatchIndirectComputeShader(IndirectArgsBuffer->GetRHI(), IndirectArgsOffset);
				UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShader.GetComputeShader());
			}
		);
	}

	/**
	 * @brief Overload for permutation-based shaders.
	 * @param GraphBuilder RDG builder.
	 * @param EventName RDG event name.
	 * @param ComputeShader Compute shader reference (TShaderMapRef).
	 * @param Parameters Shader parameters.
	 * @param IndirectArgsBuffer Buffer containing dispatch args.
	 * @param IndirectArgsOffset Byte offset into IndirectArgsBuffer.
	 */
	template<typename ShaderType>
	void AddIndirectComputePass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& EventName,
		const TShaderMapRef<ShaderType>& ComputeShader,
		typename ShaderType::FParameters* Parameters,
		FRDGBufferRef IndirectArgsBuffer,
		uint32 IndirectArgsOffset)
	{
		AddIndirectComputePass<ShaderType>(
			GraphBuilder,
			MoveTemp(EventName),
			static_cast<const TShaderRef<ShaderType>&>(ComputeShader),
			Parameters,
			IndirectArgsBuffer,
			IndirectArgsOffset);
	}
}
