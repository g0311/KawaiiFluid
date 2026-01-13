// Copyright KawaiiFluid Team. All Rights Reserved.
// GPUFluidSimulator - Data Transfer Functions (CPU <-> GPU)

#include "GPU/GPUFluidSimulator.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "Core/FluidParticle.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPUFluidSimulator, Log, All);

//=============================================================================
// Data Transfer (CPU <-> GPU)
//=============================================================================

FGPUFluidParticle FGPUFluidSimulator::ConvertToGPU(const FFluidParticle& CPUParticle)
{
	FGPUFluidParticle GPUParticle;

	GPUParticle.Position = FVector3f(CPUParticle.Position);
	GPUParticle.Mass = CPUParticle.Mass;
	GPUParticle.PredictedPosition = FVector3f(CPUParticle.PredictedPosition);
	GPUParticle.Density = CPUParticle.Density;
	GPUParticle.Velocity = FVector3f(CPUParticle.Velocity);
	GPUParticle.Lambda = CPUParticle.Lambda;
	GPUParticle.ParticleID = CPUParticle.ParticleID;
	GPUParticle.SourceID = CPUParticle.SourceID;

	// Pack flags
	uint32 Flags = 0;
	if (CPUParticle.bIsAttached) Flags |= EGPUParticleFlags::IsAttached;
	if (CPUParticle.bIsSurfaceParticle) Flags |= EGPUParticleFlags::IsSurface;
	if (CPUParticle.bIsCoreParticle) Flags |= EGPUParticleFlags::IsCore;
	if (CPUParticle.bJustDetached) Flags |= EGPUParticleFlags::JustDetached;
	if (CPUParticle.bNearGround) Flags |= EGPUParticleFlags::NearGround;
	GPUParticle.Flags = Flags;

	// NeighborCount is calculated on GPU during density solve
	GPUParticle.NeighborCount = 0;

	return GPUParticle;
}

void FGPUFluidSimulator::ConvertFromGPU(FFluidParticle& OutCPUParticle, const FGPUFluidParticle& GPUParticle)
{
	// Safety check: validate GPU data before converting
	// If data is NaN or invalid, keep the original CPU values
	FVector NewPosition = FVector(GPUParticle.Position);
	FVector NewVelocity = FVector(GPUParticle.Velocity);

	// Check for NaN or extremely large values (indicates invalid data)
	const float MaxValidValue = 1000000.0f;
	bool bValidPosition = !NewPosition.ContainsNaN() && NewPosition.GetAbsMax() < MaxValidValue;
	bool bValidVelocity = !NewVelocity.ContainsNaN() && NewVelocity.GetAbsMax() < MaxValidValue;

	if (!bValidPosition || !bValidVelocity)
	{
		// Invalid GPU data - don't update the particle
		// This can happen if readback hasn't completed yet
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			UE_LOG(LogGPUFluidSimulator, Warning, TEXT("ConvertFromGPU: Invalid data detected (NaN or extreme values) - skipping update"));
			bLoggedOnce = true;
		}
		return;
	}

	OutCPUParticle.Position = NewPosition;
	OutCPUParticle.PredictedPosition = FVector(GPUParticle.PredictedPosition);
	OutCPUParticle.Velocity = NewVelocity;
	OutCPUParticle.Mass = FMath::IsFinite(GPUParticle.Mass) ? GPUParticle.Mass : OutCPUParticle.Mass;
	OutCPUParticle.Density = FMath::IsFinite(GPUParticle.Density) ? GPUParticle.Density : OutCPUParticle.Density;
	OutCPUParticle.Lambda = FMath::IsFinite(GPUParticle.Lambda) ? GPUParticle.Lambda : OutCPUParticle.Lambda;

	// Unpack flags
	OutCPUParticle.bJustDetached = (GPUParticle.Flags & EGPUParticleFlags::JustDetached) != 0;
	OutCPUParticle.bNearGround = (GPUParticle.Flags & EGPUParticleFlags::NearGround) != 0;

	// Note: bIsAttached is not updated from GPU - CPU handles attachment state
}

void FGPUFluidSimulator::UploadParticles(const TArray<FFluidParticle>& CPUParticles)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogGPUFluidSimulator, Warning, TEXT("UploadParticles: Simulator not initialized"));
		return;
	}

	const int32 NewCount = CPUParticles.Num();
	if (NewCount == 0)
	{
		CurrentParticleCount = 0;
		CachedGPUParticles.Empty();
		return;
	}

	if (NewCount > MaxParticleCount)
	{
		UE_LOG(LogGPUFluidSimulator, Warning, TEXT("UploadParticles: Particle count (%d) exceeds capacity (%d)"),
			NewCount, MaxParticleCount);
		return;
	}

	FScopeLock Lock(&BufferLock);

	// Store old count for comparison BEFORE updating
	const int32 OldCount = CurrentParticleCount;

	// Determine upload strategy based on persistent buffer state and particle count changes
	const bool bHasPersistentBuffer = PersistentParticleBuffer.IsValid() && OldCount > 0;
	const bool bSameCount = bHasPersistentBuffer && (NewCount == OldCount);
	const bool bCanAppend = bHasPersistentBuffer && (NewCount > OldCount);

	if (bSameCount)
	{
		// Same particle count - NO UPLOAD needed, reuse GPU buffer entirely
		// GPU simulation results are preserved in PersistentParticleBuffer
		NewParticleCount = 0;
		NewParticlesToAppend.Empty();
		// Note: Don't set bNeedsFullUpload = false here, it should already be false

		static int32 ReuseLogCounter = 0;
		if (++ReuseLogCounter % 60 == 0)  // Log every 60 frames
		{
			UE_LOG(LogGPUFluidSimulator, Log, TEXT("UploadParticles: Reusing GPU buffer (no upload, %d particles)"), OldCount);
		}
		return;  // Skip upload entirely!
	}
	else if (bCanAppend)
	{
		// Only cache the NEW particles (indices OldCount to NewCount-1)
		const int32 NumNewParticles = NewCount - OldCount;
		NewParticlesToAppend.SetNumUninitialized(NumNewParticles);

		for (int32 i = 0; i < NumNewParticles; ++i)
		{
			NewParticlesToAppend[i] = ConvertToGPU(CPUParticles[OldCount + i]);
		}

		NewParticleCount = NumNewParticles;
		CurrentParticleCount = NewCount;

		UE_LOG(LogGPUFluidSimulator, Log, TEXT("UploadParticles: Appending %d new particles (total: %d)"),
			NumNewParticles, NewCount);
	}
	else
	{
		// Full upload needed: first frame, buffer invalid, or particles reduced
		CachedGPUParticles.SetNumUninitialized(NewCount);

		// Convert particles to GPU format
		for (int32 i = 0; i < NewCount; ++i)
		{
			CachedGPUParticles[i] = ConvertToGPU(CPUParticles[i]);
		}

		// Simulation bounds for Morton code (Z-Order sorting) are set via SetSimulationBounds()
		// from SimulateGPU before this call (preset bounds + component location offset)
		UE_LOG(LogGPUFluidSimulator, Log, TEXT("UploadParticles: Using bounds: Min(%.1f, %.1f, %.1f) Max(%.1f, %.1f, %.1f)"),
			SimulationBoundsMin.X, SimulationBoundsMin.Y, SimulationBoundsMin.Z,
			SimulationBoundsMax.X, SimulationBoundsMax.Y, SimulationBoundsMax.Z);

		NewParticleCount = 0;
		NewParticlesToAppend.Empty();
		CurrentParticleCount = NewCount;
		bNeedsFullUpload = true;
	}
}

void FGPUFluidSimulator::DownloadParticles(TArray<FFluidParticle>& OutCPUParticles)
{
	if (!bIsInitialized || CurrentParticleCount == 0)
	{
		return;
	}

	// Only download if we have valid GPU results from a previous simulation
	if (!bHasValidGPUResults.load())
	{
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			UE_LOG(LogGPUFluidSimulator, Log, TEXT("DownloadParticles: No valid GPU results yet, skipping"));
			bLoggedOnce = true;
		}
		return;
	}

	FScopeLock Lock(&BufferLock);

	// Read from separate readback buffer (not CachedGPUParticles)
	const int32 Count = ReadbackGPUParticles.Num();
	if (Count == 0)
	{
		return;
	}

	// Build ParticleID -> CPU index map for matching
	TMap<int32, int32> ParticleIDToIndex;
	ParticleIDToIndex.Reserve(OutCPUParticles.Num());
	for (int32 i = 0; i < OutCPUParticles.Num(); ++i)
	{
		ParticleIDToIndex.Add(OutCPUParticles[i].ParticleID, i);
	}

	// Debug: Log first particle before conversion
	static int32 DebugFrameCounter = 0;
	if (DebugFrameCounter++ % 60 == 0)
	{
		const FGPUFluidParticle& P = ReadbackGPUParticles[0];
		UE_LOG(LogGPUFluidSimulator, Log, TEXT("DownloadParticles: GPUCount=%d, CPUCount=%d, Readback[0] Pos=(%.2f, %.2f, %.2f)"),
			Count, OutCPUParticles.Num(), P.Position.X, P.Position.Y, P.Position.Z);
	}

	// Update existing particles by matching ParticleID (don't overwrite newly spawned ones)
	// Also track bounds to detect Black Hole Cell potential
	int32 UpdatedCount = 0;
	int32 OutOfBoundsCount = 0;
	const float BoundsMargin = 100.0f;  // Warn if particles within 100 units of bounds edge

	for (int32 i = 0; i < Count; ++i)
	{
		const FGPUFluidParticle& GPUParticle = ReadbackGPUParticles[i];
		if (int32* CPUIndex = ParticleIDToIndex.Find(GPUParticle.ParticleID))
		{
			ConvertFromGPU(OutCPUParticles[*CPUIndex], GPUParticle);
			++UpdatedCount;

			// Check if particle is near or outside bounds
			const FVector3f& Pos = GPUParticle.PredictedPosition;
			if (Pos.X < SimulationBoundsMin.X + BoundsMargin ||
				Pos.Y < SimulationBoundsMin.Y + BoundsMargin ||
				Pos.Z < SimulationBoundsMin.Z + BoundsMargin ||
				Pos.X > SimulationBoundsMax.X - BoundsMargin ||
				Pos.Y > SimulationBoundsMax.Y - BoundsMargin ||
				Pos.Z > SimulationBoundsMax.Z - BoundsMargin)
			{
				OutOfBoundsCount++;
			}
		}
	}

	// Warn if many particles are near bounds edge (potential Black Hole Cell issue)
	static int32 LastBoundsWarningFrame = -1000;
	if (OutOfBoundsCount > Count / 10 && (GFrameCounter - LastBoundsWarningFrame) > 300)  // >10% near edge, warn every 5 sec
	{
		LastBoundsWarningFrame = GFrameCounter;
		UE_LOG(LogGPUFluidSimulator, Warning,
			TEXT("Z-Order WARNING: %d/%d particles (%.1f%%) are near simulation bounds edge! "
			     "This may cause Black Hole Cell problem with Z-Order sorting. "
			     "Bounds: Min(%.1f, %.1f, %.1f) Max(%.1f, %.1f, %.1f)"),
			OutOfBoundsCount, Count, 100.0f * OutOfBoundsCount / Count,
			SimulationBoundsMin.X, SimulationBoundsMin.Y, SimulationBoundsMin.Z,
			SimulationBoundsMax.X, SimulationBoundsMax.Y, SimulationBoundsMax.Z);
	}

	UE_LOG(LogGPUFluidSimulator, Verbose, TEXT("DownloadParticles: Updated %d/%d particles"), UpdatedCount, Count);
}

bool FGPUFluidSimulator::GetAllGPUParticles(TArray<FFluidParticle>& OutParticles)
{
	if (!bIsInitialized || CurrentParticleCount == 0)
	{
		return false;
	}

	// Only download if we have valid GPU results from a previous simulation
	if (!bHasValidGPUResults.load())
	{
		return false;
	}

	FScopeLock Lock(&BufferLock);

	// Read from readback buffer
	const int32 Count = ReadbackGPUParticles.Num();
	if (Count == 0)
	{
		return false;
	}

	// Create new particles from GPU data (no ParticleID matching required)
	OutParticles.SetNum(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const FGPUFluidParticle& GPUParticle = ReadbackGPUParticles[i];
		FFluidParticle& OutParticle = OutParticles[i];

		// Initialize with default values
		OutParticle = FFluidParticle();

		// Convert GPU data to CPU particle
		FVector NewPosition = FVector(GPUParticle.Position);
		FVector NewVelocity = FVector(GPUParticle.Velocity);

		// Validate data
		const float MaxValidValue = 1000000.0f;
		bool bValidPosition = !NewPosition.ContainsNaN() && NewPosition.GetAbsMax() < MaxValidValue;
		bool bValidVelocity = !NewVelocity.ContainsNaN() && NewVelocity.GetAbsMax() < MaxValidValue;

		if (bValidPosition)
		{
			OutParticle.Position = NewPosition;
			OutParticle.PredictedPosition = FVector(GPUParticle.PredictedPosition);
		}

		if (bValidVelocity)
		{
			OutParticle.Velocity = NewVelocity;
		}

		OutParticle.Mass = FMath::IsFinite(GPUParticle.Mass) ? GPUParticle.Mass : 1.0f;
		OutParticle.Density = FMath::IsFinite(GPUParticle.Density) ? GPUParticle.Density : 0.0f;
		OutParticle.Lambda = FMath::IsFinite(GPUParticle.Lambda) ? GPUParticle.Lambda : 0.0f;
		OutParticle.ParticleID = GPUParticle.ParticleID;
		OutParticle.SourceID = GPUParticle.SourceID;

		// Unpack flags
		OutParticle.bIsAttached = (GPUParticle.Flags & EGPUParticleFlags::IsAttached) != 0;
		OutParticle.bIsSurfaceParticle = (GPUParticle.Flags & EGPUParticleFlags::IsSurface) != 0;
		OutParticle.bIsCoreParticle = (GPUParticle.Flags & EGPUParticleFlags::IsCore) != 0;
		OutParticle.bJustDetached = (GPUParticle.Flags & EGPUParticleFlags::JustDetached) != 0;
		OutParticle.bNearGround = (GPUParticle.Flags & EGPUParticleFlags::NearGround) != 0;

		// Set neighbor count (resize array so NeighborIndices.Num() returns the count)
		// GPU stores count only, not actual indices (computed on-the-fly during spatial hash queries)
		if (GPUParticle.NeighborCount > 0)
		{
			OutParticle.NeighborIndices.SetNum(GPUParticle.NeighborCount);
		}
	}

	static int32 DebugFrameCounter = 0;
	if (++DebugFrameCounter % 60 == 0)
	{
		UE_LOG(LogGPUFluidSimulator, Log, TEXT("GetAllGPUParticles: Retrieved %d particles"), Count);
	}

	return true;
}

//=============================================================================
// Stream Compaction API (Delegated to FGPUStreamCompactionManager)
//=============================================================================

void FGPUFluidSimulator::ExecuteAABBFiltering(const TArray<FGPUFilterAABB>& FilterAABBs)
{
	if (!StreamCompactionManager.IsValid() || !bIsInitialized || CurrentParticleCount == 0)
	{
		return;
	}

	// Pass PersistentParticleBuffer and fallback SRV to manager
	// Manager will create proper SRV on render thread from PersistentParticleBuffer if valid
	StreamCompactionManager->ExecuteAABBFiltering(FilterAABBs, CurrentParticleCount, PersistentParticleBuffer, ParticleSRV);
}

bool FGPUFluidSimulator::GetFilteredCandidates(TArray<FGPUCandidateParticle>& OutCandidates)
{
	if (!StreamCompactionManager.IsValid())
	{
		OutCandidates.Empty();
		return false;
	}
	return StreamCompactionManager->GetFilteredCandidates(OutCandidates);
}

void FGPUFluidSimulator::ApplyCorrections(const TArray<FParticleCorrection>& Corrections)
{
	if (!StreamCompactionManager.IsValid() || !bIsInitialized)
	{
		return;
	}
	StreamCompactionManager->ApplyCorrections(Corrections, PersistentParticleBuffer);
}

void FGPUFluidSimulator::ApplyAttachmentUpdates(const TArray<FAttachedParticleUpdate>& Updates)
{
	if (!StreamCompactionManager.IsValid() || !bIsInitialized)
	{
		return;
	}
	StreamCompactionManager->ApplyAttachmentUpdates(Updates, PersistentParticleBuffer);
}
