// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/Resources/KawaiiFluidRenderResource.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ClearQuad.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "ShaderParameterUtils.h"
#include "Simulation/Shaders/GPUFluidSimulatorShaders.h"
#include "Simulation/Resources/GPUFluidParticle.h"
#include "Simulation/GPUFluidSimulator.h"

/**
 * @brief Default constructor for the render resource.
 */
FKawaiiFluidRenderResource::FKawaiiFluidRenderResource()
	: ParticleCount(0)
	, BufferCapacity(0)
{
}

/**
 * @brief Destructor ensuring resources are released before object destruction.
 */
FKawaiiFluidRenderResource::~FKawaiiFluidRenderResource()
{
	check(!IsInitialized() && "RenderResource must be released before destruction!");
}

/**
 * @brief Initialize the RHI resources, creating initial small buffers.
 */
void FKawaiiFluidRenderResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (BufferCapacity == 0)
	{
		BufferCapacity = 100;
	}

	ResizeBuffer(RHICmdList, BufferCapacity);
}

/**
 * @brief Release all GPU buffers and views.
 */
void FKawaiiFluidRenderResource::ReleaseRHI()
{
	ParticleBuffer.SafeRelease();
	ParticleSRV.SafeRelease();
	ParticleUAV.SafeRelease();
	PooledParticleBuffer.SafeRelease();

	PooledPositionBuffer.SafeRelease();
	PooledVelocityBuffer.SafeRelease();

	ParticleCount = 0;
	BufferCapacity = 0;
	bBufferReadyForRendering.store(false);
}

/**
 * @brief Internal helper to determine if the buffer needs growth to accommodate a new particle count.
 */
bool FKawaiiFluidRenderResource::NeedsResize(int32 NewCount) const
{
	const bool bNeedGrow = NewCount > BufferCapacity;

	if (bNeedGrow)
	{
		UE_LOG(LogTemp, Warning, TEXT("RenderResource: Buffer resize needed (Count %d > Capacity %d)"),
			NewCount, BufferCapacity);
	}

	return bNeedGrow;
}

/**
 * @brief Recreates all GPU buffers with a new capacity.
 * 
 * Uses the Render Graph (RDG) to atomically create and clear new buffers while 
 * maintaining compatibility between legacy AoS and optimized SoA paths.
 */
void FKawaiiFluidRenderResource::ResizeBuffer(FRHICommandListBase& RHICmdList, int32 NewCapacity)
{
	ParticleBuffer.SafeRelease();
	ParticleSRV.SafeRelease();
	ParticleUAV.SafeRelease();
	PooledParticleBuffer.SafeRelease();

	PooledPositionBuffer.SafeRelease();
	PooledVelocityBuffer.SafeRelease();

	PooledBoundsBuffer.SafeRelease();
	PooledRenderParticleBuffer.SafeRelease();

	BufferCapacity = NewCapacity;

	if (NewCapacity == 0)
	{
		return;
	}

	const uint32 ElementSize = sizeof(FKawaiiFluidRenderParticle);

	FRHICommandListImmediate& ImmediateCmdList = static_cast<FRHICommandListImmediate&>(RHICmdList);
	FRDGBuilder GraphBuilder(ImmediateCmdList);

	//========================================
	// Create Legacy AoS buffers
	//========================================
	FRDGBufferDesc RDGBufferDesc = FRDGBufferDesc::CreateStructuredDesc(ElementSize, NewCapacity);
	RDGBufferDesc.Usage |= EBufferUsageFlags::UnorderedAccess;
	FRDGBufferRef RDGBuffer = GraphBuilder.CreateBuffer(RDGBufferDesc, TEXT("RenderParticlesPooled"));

	FRDGBufferUAVRef BufferUAV = GraphBuilder.CreateUAV(RDGBuffer);
	AddClearUAVPass(GraphBuilder, BufferUAV, 0u);
	GraphBuilder.QueueBufferExtraction(RDGBuffer, &PooledParticleBuffer, ERHIAccess::SRVMask);

	//========================================
	// Create SoA buffers
	//========================================

	FRDGBufferDesc PositionBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), NewCapacity);
	PositionBufferDesc.Usage |= EBufferUsageFlags::UnorderedAccess;
	FRDGBufferRef PositionRDGBuffer = GraphBuilder.CreateBuffer(PositionBufferDesc, TEXT("RenderPositionsSoA"));

	FRDGBufferUAVRef PositionUAV = GraphBuilder.CreateUAV(PositionRDGBuffer);
	AddClearUAVPass(GraphBuilder, PositionUAV, 0u);
	GraphBuilder.QueueBufferExtraction(PositionRDGBuffer, &PooledPositionBuffer, ERHIAccess::SRVMask);

	FRDGBufferDesc VelocityBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), NewCapacity);
	VelocityBufferDesc.Usage |= EBufferUsageFlags::UnorderedAccess;
	FRDGBufferRef VelocityRDGBuffer = GraphBuilder.CreateBuffer(VelocityBufferDesc, TEXT("RenderVelocitiesSoA"));

	FRDGBufferUAVRef VelocityUAV = GraphBuilder.CreateUAV(VelocityRDGBuffer);
	AddClearUAVPass(GraphBuilder, VelocityUAV, 0u);
	GraphBuilder.QueueBufferExtraction(VelocityRDGBuffer, &PooledVelocityBuffer, ERHIAccess::SRVMask);

	//========================================
	// Bounds and RenderParticle buffers
	//========================================
	FRDGBufferDesc BoundsBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), 2);
	BoundsBufferDesc.Usage |= EBufferUsageFlags::UnorderedAccess;
	FRDGBufferRef BoundsRDGBuffer = GraphBuilder.CreateBuffer(BoundsBufferDesc, TEXT("ParticleBounds"));

	FRDGBufferUAVRef BoundsUAV = GraphBuilder.CreateUAV(BoundsRDGBuffer);
	AddClearUAVPass(GraphBuilder, BoundsUAV, 0u);
	GraphBuilder.QueueBufferExtraction(BoundsRDGBuffer, &PooledBoundsBuffer, ERHIAccess::SRVMask);

	FRDGBufferDesc RenderParticleBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FKawaiiFluidRenderParticle), NewCapacity);
	RenderParticleBufferDesc.Usage |= EBufferUsageFlags::UnorderedAccess;
	FRDGBufferRef RenderParticleRDGBuffer = GraphBuilder.CreateBuffer(RenderParticleBufferDesc, TEXT("RenderParticles"));

	FRDGBufferUAVRef RenderParticleUAV = GraphBuilder.CreateUAV(RenderParticleRDGBuffer);
	AddClearUAVPass(GraphBuilder, RenderParticleUAV, 0u);
	GraphBuilder.QueueBufferExtraction(RenderParticleRDGBuffer, &PooledRenderParticleBuffer, ERHIAccess::SRVMask);

	GraphBuilder.Execute();

	if (PooledParticleBuffer.IsValid())
	{
		ParticleBuffer = PooledParticleBuffer->GetRHI();

		ParticleSRV = ImmediateCmdList.CreateShaderResourceView(
			ParticleBuffer,
			FRHIViewDesc::CreateBufferSRV()
				.SetTypeFromBuffer(ParticleBuffer)
		);

		ParticleUAV = ImmediateCmdList.CreateUnorderedAccessView(
			ParticleBuffer,
			FRHIViewDesc::CreateBufferUAV()
				.SetTypeFromBuffer(ParticleBuffer)
		);
	}
}

//========================================
// GPU simulator interface implementation
//========================================

/**
 * @brief Sets the GPU simulator reference and triggers buffer resizing if necessary.
 */
void FKawaiiFluidRenderResource::SetGPUSimulatorReference(
	FGPUFluidSimulator* InSimulator,
	int32 InMaxParticleCount,
	float InParticleRadius)
{
	CachedGPUSimulator.store(InSimulator);
	CachedParticleRadius.store(InParticleRadius);

	if (InSimulator)
	{
		// Use MaxParticleCount for buffer sizing — immune to CPU/GPU count desync.
		// This guarantees the buffer is always large enough for any GPU-side particle count.
		const bool bNeedsResizeNow = NeedsResize(InMaxParticleCount);
		if (bNeedsResizeNow)
		{
			FKawaiiFluidRenderResource* RenderResource = this;
			const int32 NewCount = InMaxParticleCount;

			ENQUEUE_RENDER_COMMAND(ResizeBufferForGPUMode)(
				[RenderResource, NewCount](FRHICommandListImmediate& RHICmdList)
				{
					int32 NewCapacity = FMath::Max(NewCount, RenderResource->BufferCapacity * 2);
					RenderResource->ResizeBuffer(RHICmdList, NewCapacity);
				}
			);
		}
	}
}

void FKawaiiFluidRenderResource::ClearGPUSimulatorReference()
{
	CachedGPUSimulator.store(nullptr);
}

FRDGBufferSRVRef FKawaiiFluidRenderResource::GetPhysicsBufferSRV(FRDGBuilder& GraphBuilder) const
{
	FGPUFluidSimulator* Simulator = CachedGPUSimulator.load();
	if (!Simulator)
	{
		return nullptr;
	}

	TRefCountPtr<FRDGPooledBuffer> PhysicsPooledBuffer = Simulator->GetPersistentParticleBuffer();
	if (!PhysicsPooledBuffer.IsValid())
	{
		return nullptr;
	}

	FRDGBufferRef PhysicsBuffer = GraphBuilder.RegisterExternalBuffer(
		PhysicsPooledBuffer,
		TEXT("UnifiedPhysicsParticles")
	);
	return GraphBuilder.CreateSRV(PhysicsBuffer);
}

bool FKawaiiFluidRenderResource::GetAnisotropyBufferSRVs(
	FRDGBuilder& GraphBuilder,
	FRDGBufferSRVRef& OutAxis1SRV,
	FRDGBufferSRVRef& OutAxis2SRV,
	FRDGBufferSRVRef& OutAxis3SRV) const
{
	FGPUFluidSimulator* Simulator = CachedGPUSimulator.load();
	if (!Simulator || !Simulator->IsAnisotropyEnabled())
	{
		OutAxis1SRV = nullptr;
		OutAxis2SRV = nullptr;
		OutAxis3SRV = nullptr;
		return false;
	}

	TRefCountPtr<FRDGPooledBuffer> Axis1Pooled = Simulator->GetPersistentAnisotropyAxis1Buffer();
	TRefCountPtr<FRDGPooledBuffer> Axis2Pooled = Simulator->GetPersistentAnisotropyAxis2Buffer();
	TRefCountPtr<FRDGPooledBuffer> Axis3Pooled = Simulator->GetPersistentAnisotropyAxis3Buffer();

	if (!Axis1Pooled.IsValid() || !Axis2Pooled.IsValid() || !Axis3Pooled.IsValid())
	{
		OutAxis1SRV = nullptr;
		OutAxis2SRV = nullptr;
		OutAxis3SRV = nullptr;
		return false;
	}

	FRDGBufferRef Axis1Buffer = GraphBuilder.RegisterExternalBuffer(Axis1Pooled, TEXT("UnifiedAnisotropyAxis1"));
	FRDGBufferRef Axis2Buffer = GraphBuilder.RegisterExternalBuffer(Axis2Pooled, TEXT("UnifiedAnisotropyAxis2"));
	FRDGBufferRef Axis3Buffer = GraphBuilder.RegisterExternalBuffer(Axis3Pooled, TEXT("UnifiedAnisotropyAxis3"));

	OutAxis1SRV = GraphBuilder.CreateSRV(Axis1Buffer);
	OutAxis2SRV = GraphBuilder.CreateSRV(Axis2Buffer);
	OutAxis3SRV = GraphBuilder.CreateSRV(Axis3Buffer);

	return true;
}

bool FKawaiiFluidRenderResource::IsAnisotropyEnabled() const
{
	FGPUFluidSimulator* Simulator = CachedGPUSimulator.load();
	return Simulator && Simulator->IsAnisotropyEnabled();
}

FRDGBufferSRVRef FKawaiiFluidRenderResource::GetRenderOffsetBufferSRV(FRDGBuilder& GraphBuilder) const
{
	FGPUFluidSimulator* Simulator = CachedGPUSimulator.load();
	if (!Simulator || !Simulator->IsAnisotropyEnabled())
	{
		return nullptr;
	}

	TRefCountPtr<FRDGPooledBuffer> RenderOffsetPooled = Simulator->GetPersistentRenderOffsetBuffer();
	if (!RenderOffsetPooled.IsValid())
	{
		return nullptr;
	}

	FRDGBufferRef RenderOffsetBuffer = GraphBuilder.RegisterExternalBuffer(
		RenderOffsetPooled,
		TEXT("UnifiedRenderOffset"));
	return GraphBuilder.CreateSRV(RenderOffsetBuffer);
}

//========================================
// Bounds and RenderParticle buffer management
//========================================

void FKawaiiFluidRenderResource::SetBoundsBuffer(TRefCountPtr<FRDGPooledBuffer> InBoundsBuffer)
{
	PooledBoundsBuffer = InBoundsBuffer;
}

void FKawaiiFluidRenderResource::SetRenderParticleBuffer(TRefCountPtr<FRDGPooledBuffer> InBuffer)
{
	PooledRenderParticleBuffer = InBuffer;
}

//========================================
// Z-Order buffer access (for Ray Marching volume building)
//========================================

TRefCountPtr<FRDGPooledBuffer> FKawaiiFluidRenderResource::GetPooledCellStartBuffer() const
{
	FGPUFluidSimulator* Simulator = CachedGPUSimulator.load();
	if (Simulator)
	{
		return Simulator->GetPersistentCellStartBuffer();
	}
	return nullptr;
}

TRefCountPtr<FRDGPooledBuffer> FKawaiiFluidRenderResource::GetPooledCellEndBuffer() const
{
	FGPUFluidSimulator* Simulator = CachedGPUSimulator.load();
	if (Simulator)
	{
		return Simulator->GetPersistentCellEndBuffer();
	}
	return nullptr;
}

bool FKawaiiFluidRenderResource::HasValidZOrderBuffers() const
{
	FGPUFluidSimulator* Simulator = CachedGPUSimulator.load();
	if (Simulator)
	{
		return Simulator->HasValidZOrderBuffers();
	}
	return false;
}
