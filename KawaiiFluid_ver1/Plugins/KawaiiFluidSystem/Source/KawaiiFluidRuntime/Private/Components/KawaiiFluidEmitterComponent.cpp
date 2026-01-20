// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/KawaiiFluidEmitterComponent.h"
#include "Actors/KawaiiFluidEmitter.h"
#include "Actors/KawaiiFluidVolume.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "GPU/GPUFluidParticle.h"
#include "DrawDebugHelpers.h"

UKawaiiFluidEmitterComponent::UKawaiiFluidEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;  // Enable tick in editor for wireframe visualization
}

void UKawaiiFluidEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find volume if not set
	if (!TargetVolume && bAutoFindVolume)
	{
		TargetVolume = FindNearestVolume();
	}

	RegisterToVolume();

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidEmitterComponent [%s]: BeginPlay - TargetVolume=%s"),
		*GetName(), TargetVolume ? *TargetVolume->GetName() : TEXT("None"));

	// Auto spawn for ShapeVolume mode
	if (IsShapeVolumeMode() && bAutoSpawnOnBeginPlay && !bAutoSpawnExecuted)
	{
		ExecuteAutoSpawn();
	}
}

void UKawaiiFluidEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromVolume();
	Super::EndPlay(EndPlayReason);
}

void UKawaiiFluidEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bIsGameWorld = World->IsGameWorld();

	// Wireframe visualization (editor only, or runtime if explicitly enabled)
#if WITH_EDITOR
	if (bShowSpawnVolumeWireframe && !bIsGameWorld)
	{
		DrawSpawnVolumeVisualization();
	}
#endif

	// Process continuous spawning for Emitter mode (game world only)
	if (bIsGameWorld && IsEmitterMode())
	{
		ProcessContinuousSpawn(DeltaTime);
	}
}

AKawaiiFluidEmitter* UKawaiiFluidEmitterComponent::GetOwnerEmitter() const
{
	return Cast<AKawaiiFluidEmitter>(GetOwner());
}

void UKawaiiFluidEmitterComponent::SetTargetVolume(AKawaiiFluidVolume* NewVolume)
{
	if (TargetVolume != NewVolume)
	{
		UnregisterFromVolume();
		TargetVolume = NewVolume;
		RegisterToVolume();
	}
}

float UKawaiiFluidEmitterComponent::GetParticleSpacing() const
{
	if (AKawaiiFluidVolume* Volume = GetTargetVolume())
	{
		return Volume->GetParticleSpacing();
	}
	return 10.0f; // Default fallback
}

void UKawaiiFluidEmitterComponent::ExecuteAutoSpawn()
{
	if (bAutoSpawnExecuted)
	{
		return;
	}

	AKawaiiFluidVolume* Volume = GetTargetVolume();
	if (!Volume)
	{
		UE_LOG(LogTemp, Warning, TEXT("UKawaiiFluidEmitterComponent::ExecuteAutoSpawn - No target Volume available"));
		return;
	}

	bAutoSpawnExecuted = true;

	const FVector SpawnCenter = GetComponentLocation() + SpawnSettings.SpawnOffset;
	const float Spacing = GetParticleSpacing();
	const FVector InitialVelocity = SpawnSettings.InitialVelocity;

	int32 SpawnedCount = 0;

	// Use hexagonal pattern if configured
	const bool bUseHexagonal = (SpawnSettings.GridPattern == ESpawnGridPattern::Hexagonal);

	switch (SpawnSettings.ShapeType)
	{
	case EFluidShapeType::Sphere:
		if (bUseHexagonal)
		{
			SpawnedCount = SpawnParticlesSphereHexagonal(SpawnCenter, SpawnSettings.SphereRadius, Spacing, InitialVelocity);
		}
		else
		{
			const int32 Count = SpawnSettings.bAutoCalculateParticleCount ?
				SpawnSettings.CalculateExpectedParticleCount(Spacing) : SpawnSettings.ParticleCount;
			SpawnParticlesSphereRandom(SpawnCenter, SpawnSettings.SphereRadius, Count, InitialVelocity);
			SpawnedCount = Count;
		}
		break;

	case EFluidShapeType::Box:
		if (bUseHexagonal)
		{
			SpawnedCount = SpawnParticlesBoxHexagonal(SpawnCenter, SpawnSettings.BoxExtent, Spacing, InitialVelocity);
		}
		else
		{
			const int32 Count = SpawnSettings.bAutoCalculateParticleCount ?
				SpawnSettings.CalculateExpectedParticleCount(Spacing) : SpawnSettings.ParticleCount;
			SpawnParticlesBoxRandom(SpawnCenter, SpawnSettings.BoxExtent, Count, InitialVelocity);
			SpawnedCount = Count;
		}
		break;

	case EFluidShapeType::Cylinder:
		if (bUseHexagonal)
		{
			SpawnedCount = SpawnParticlesCylinderHexagonal(SpawnCenter, SpawnSettings.CylinderRadius,
				SpawnSettings.CylinderHalfHeight, Spacing, InitialVelocity);
		}
		else
		{
			const int32 Count = SpawnSettings.bAutoCalculateParticleCount ?
				SpawnSettings.CalculateExpectedParticleCount(Spacing) : SpawnSettings.ParticleCount;
			SpawnParticlesCylinderRandom(SpawnCenter, SpawnSettings.CylinderRadius,
				SpawnSettings.CylinderHalfHeight, Count, InitialVelocity);
			SpawnedCount = Count;
		}
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("UKawaiiFluidEmitterComponent::ExecuteAutoSpawn - Spawned %d particles"), SpawnedCount);
}

void UKawaiiFluidEmitterComponent::BurstSpawn(int32 Count)
{
	if (Count <= 0 || HasReachedParticleLimit())
	{
		return;
	}

	// Clamp to remaining budget
	if (SpawnSettings.MaxParticleCount > 0)
	{
		Count = FMath::Min(Count, SpawnSettings.MaxParticleCount - SpawnedParticleCount);
	}

	const FVector SpawnPos = GetComponentLocation() + SpawnSettings.SpawnOffset;
	const FVector WorldDirection = GetComponentRotation().RotateVector(SpawnSettings.SpawnDirection.GetSafeNormal());

	SpawnParticleDirectional(SpawnPos, WorldDirection, SpawnSettings.SpawnSpeed, Count,
		SpawnSettings.EmitterType == EFluidEmitterType::Spray ? SpawnSettings.ConeAngle : 0.0f);
}

bool UKawaiiFluidEmitterComponent::HasReachedParticleLimit() const
{
	if (SpawnSettings.MaxParticleCount <= 0)
	{
		return false;
	}
	return SpawnedParticleCount >= SpawnSettings.MaxParticleCount;
}

void UKawaiiFluidEmitterComponent::ProcessContinuousSpawn(float DeltaTime)
{
	if (HasReachedParticleLimit() && !bRecycleOldestParticles)
	{
		return;
	}

	switch (SpawnSettings.EmitterType)
	{
	case EFluidEmitterType::Stream:
		ProcessStreamEmitter(DeltaTime);
		break;
	case EFluidEmitterType::HexagonalStream:
		ProcessHexagonalStreamEmitter(DeltaTime);
		break;
	case EFluidEmitterType::Spray:
		ProcessSprayEmitter(DeltaTime);
		break;
	}
}

void UKawaiiFluidEmitterComponent::ProcessStreamEmitter(float DeltaTime)
{
	if (SpawnSettings.ParticlesPerSecond <= 0.0f)
	{
		return;
	}

	SpawnAccumulator += DeltaTime;
	const float SpawnInterval = 1.0f / SpawnSettings.ParticlesPerSecond;
	int32 ParticlesToSpawn = FMath::FloorToInt(SpawnAccumulator / SpawnInterval);

	if (ParticlesToSpawn > 0)
	{
		SpawnAccumulator -= ParticlesToSpawn * SpawnInterval;

		// Recycle if needed
		RecycleOldestParticlesIfNeeded(ParticlesToSpawn);

		// Clamp to particle limit
		if (SpawnSettings.MaxParticleCount > 0 && !bRecycleOldestParticles)
		{
			ParticlesToSpawn = FMath::Min(ParticlesToSpawn, SpawnSettings.MaxParticleCount - SpawnedParticleCount);
		}

		if (ParticlesToSpawn > 0)
		{
			const FVector SpawnPos = GetComponentLocation() + SpawnSettings.SpawnOffset;
			const FVector WorldDirection = GetComponentRotation().RotateVector(SpawnSettings.SpawnDirection.GetSafeNormal());
			SpawnParticleDirectional(SpawnPos, WorldDirection, SpawnSettings.SpawnSpeed, ParticlesToSpawn, 0.0f);
		}
	}
}

void UKawaiiFluidEmitterComponent::ProcessHexagonalStreamEmitter(float DeltaTime)
{
	// Calculate Spacing exactly like KawaiiFluidComponent does:
	// Use StreamParticleSpacing if set, otherwise Preset->SmoothingRadius * 0.5f
	float EffectiveSpacing = SpawnSettings.StreamParticleSpacing;
	if (EffectiveSpacing <= 0.0f)
	{
		if (AKawaiiFluidVolume* Vol = GetTargetVolume())
		{
			if (UKawaiiFluidPresetDataAsset* Pst = Vol->GetPreset())
			{
				EffectiveSpacing = Pst->SmoothingRadius * 0.5f;
			}
		}
	}
	if (EffectiveSpacing <= 0.0f)
	{
		EffectiveSpacing = 10.0f;  // fallback
	}
	const float LayerSpacing = EffectiveSpacing * SpawnSettings.StreamLayerSpacingRatio;

	float LayersToSpawn = 0.0f;

	if (SpawnSettings.StreamLayerMode == EStreamLayerMode::VelocityBased)
	{
		// Calculate layer spawn based on velocity
		const float DistanceThisFrame = SpawnSettings.SpawnSpeed * DeltaTime;
		LayerDistanceAccumulator += DistanceThisFrame;

		if (LayerDistanceAccumulator >= LayerSpacing)
		{
			LayersToSpawn = FMath::FloorToFloat(LayerDistanceAccumulator / LayerSpacing);
			LayerDistanceAccumulator = FMath::Fmod(LayerDistanceAccumulator, LayerSpacing);
		}
	}
	else // FixedRate
	{
		SpawnAccumulator += DeltaTime;
		const float LayerInterval = 1.0f / SpawnSettings.StreamLayersPerSecond;

		if (SpawnAccumulator >= LayerInterval)
		{
			LayersToSpawn = FMath::FloorToFloat(SpawnAccumulator / LayerInterval);
			SpawnAccumulator = FMath::Fmod(SpawnAccumulator, LayerInterval);
		}
	}

	const int32 LayerCount = FMath::FloorToInt(LayersToSpawn);
	if (LayerCount <= 0)
	{
		return;
	}

	const FVector SpawnPos = GetComponentLocation() + SpawnSettings.SpawnOffset;
	const FVector WorldDirection = GetComponentRotation().RotateVector(SpawnSettings.SpawnDirection.GetSafeNormal());

	for (int32 i = 0; i < LayerCount; ++i)
	{
		// Estimate particles per layer for recycling
		const int32 EstimatedParticlesPerLayer = FMath::Max(1, FMath::CeilToInt(
			PI * FMath::Square(SpawnSettings.StreamRadius / EffectiveSpacing)));
		RecycleOldestParticlesIfNeeded(EstimatedParticlesPerLayer);

		SpawnHexagonalLayer(SpawnPos, WorldDirection, SpawnSettings.SpawnSpeed,
			SpawnSettings.StreamRadius, EffectiveSpacing);
	}
}

void UKawaiiFluidEmitterComponent::ProcessSprayEmitter(float DeltaTime)
{
	if (SpawnSettings.ParticlesPerSecond <= 0.0f)
	{
		return;
	}

	SpawnAccumulator += DeltaTime;
	const float SpawnInterval = 1.0f / SpawnSettings.ParticlesPerSecond;
	int32 ParticlesToSpawn = FMath::FloorToInt(SpawnAccumulator / SpawnInterval);

	if (ParticlesToSpawn > 0)
	{
		SpawnAccumulator -= ParticlesToSpawn * SpawnInterval;

		// Recycle if needed
		RecycleOldestParticlesIfNeeded(ParticlesToSpawn);

		// Clamp to particle limit
		if (SpawnSettings.MaxParticleCount > 0 && !bRecycleOldestParticles)
		{
			ParticlesToSpawn = FMath::Min(ParticlesToSpawn, SpawnSettings.MaxParticleCount - SpawnedParticleCount);
		}

		if (ParticlesToSpawn > 0)
		{
			const FVector SpawnPos = GetComponentLocation() + SpawnSettings.SpawnOffset;
			const FVector WorldDirection = GetComponentRotation().RotateVector(SpawnSettings.SpawnDirection.GetSafeNormal());
			SpawnParticleDirectional(SpawnPos, WorldDirection, SpawnSettings.SpawnSpeed, ParticlesToSpawn, SpawnSettings.ConeAngle);
		}
	}
}

int32 UKawaiiFluidEmitterComponent::SpawnParticlesSphereHexagonal(FVector Center, float Radius, float Spacing, FVector InitialVelocity)
{
	AKawaiiFluidVolume* Volume = GetTargetVolume();
	if (!Volume || Spacing <= 0.0f || Radius <= 0.0f) return 0;

	TArray<FVector> Positions;
	TArray<FVector> Velocities;

	// HCP density compensation (matches KawaiiFluidSimulationModule exactly)
	const float HCPCompensation = 1.122f;
	const float AdjustedSpacing = Spacing * HCPCompensation;

	// Hexagonal close packing offsets
	const float RowSpacingY = AdjustedSpacing * 0.866025f;   // sqrt(3)/2
	const float LayerSpacingZ = AdjustedSpacing * 0.816497f; // sqrt(2/3)
	const float RadiusSq = Radius * Radius;
	const float JitterRange = AdjustedSpacing * SpawnSettings.JitterAmount;

	// Integer-based grid (matches SimulationModule)
	const int32 GridSize = FMath::CeilToInt(Radius / AdjustedSpacing) + 1;
	const int32 GridSizeY = FMath::CeilToInt(Radius / RowSpacingY) + 1;
	const int32 GridSizeZ = FMath::CeilToInt(Radius / LayerSpacingZ) + 1;

	const float EstimatedCount = (4.0f / 3.0f) * PI * Radius * Radius * Radius / (AdjustedSpacing * AdjustedSpacing * AdjustedSpacing);
	Positions.Reserve(FMath::CeilToInt(EstimatedCount));
	Velocities.Reserve(FMath::CeilToInt(EstimatedCount));

	for (int32 z = -GridSizeZ; z <= GridSizeZ; ++z)
	{
		// Z-layer offset pattern (mod 3) - ABCABC stacking for proper HCP
		const int32 ZMod = ((z + GridSizeZ) % 3);
		const float ZLayerOffsetX = (ZMod == 1) ? AdjustedSpacing * 0.5f : ((ZMod == 2) ? AdjustedSpacing * 0.25f : 0.0f);
		const float ZLayerOffsetY = (ZMod == 1) ? RowSpacingY / 3.0f : ((ZMod == 2) ? RowSpacingY * 2.0f / 3.0f : 0.0f);

		for (int32 y = -GridSizeY; y <= GridSizeY; ++y)
		{
			const float RowOffsetX = (((y + GridSizeY) % 2) == 1) ? AdjustedSpacing * 0.5f : 0.0f;

			for (int32 x = -GridSize; x <= GridSize; ++x)
			{
				FVector LocalPos(
					x * AdjustedSpacing + RowOffsetX + ZLayerOffsetX,
					y * RowSpacingY + ZLayerOffsetY,
					z * LayerSpacingZ
				);

				// Check sphere bounds
				if (LocalPos.SizeSquared() > RadiusSq)
				{
					continue;
				}

				FVector WorldPos = Center + LocalPos;

				// Apply jitter
				if (SpawnSettings.bUseJitter && JitterRange > 0.0f)
				{
					WorldPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				Positions.Add(WorldPos);
				Velocities.Add(InitialVelocity);
			}
		}
	}

	QueueSpawnRequest(Positions, Velocities);
	return Positions.Num();
}

int32 UKawaiiFluidEmitterComponent::SpawnParticlesBoxHexagonal(FVector Center, FVector Extent, float Spacing, FVector InitialVelocity)
{
	AKawaiiFluidVolume* Volume = GetTargetVolume();
	if (!Volume || Spacing <= 0.0f) return 0;

	TArray<FVector> Positions;
	TArray<FVector> Velocities;

	// HCP density compensation (matches KawaiiFluidSimulationModule exactly)
	const float HCPCompensation = 1.122f;
	const float AdjustedSpacing = Spacing * HCPCompensation;

	// Hexagonal Close Packing constants
	const float RowSpacingY = AdjustedSpacing * 0.866025f;   // sqrt(3)/2
	const float LayerSpacingZ = AdjustedSpacing * 0.816497f; // sqrt(2/3)
	const float JitterRange = AdjustedSpacing * SpawnSettings.JitterAmount;

	// Calculate grid counts (matches SimulationModule)
	const int32 CountX = FMath::Max(1, FMath::CeilToInt(Extent.X * 2.0f / AdjustedSpacing));
	const int32 CountY = FMath::Max(1, FMath::CeilToInt(Extent.Y * 2.0f / RowSpacingY));
	const int32 CountZ = FMath::Max(1, FMath::CeilToInt(Extent.Z * 2.0f / LayerSpacingZ));

	const int32 EstimatedTotal = CountX * CountY * CountZ;
	Positions.Reserve(EstimatedTotal);
	Velocities.Reserve(EstimatedTotal);

	// Start position (bottom-left-back corner with half-spacing offset)
	const FVector LocalStart(-Extent.X + AdjustedSpacing * 0.5f, -Extent.Y + RowSpacingY * 0.5f, -Extent.Z + LayerSpacingZ * 0.5f);

	for (int32 z = 0; z < CountZ; ++z)
	{
		// Z layer offset for HCP (ABC stacking pattern - mod 3)
		const float ZLayerOffsetX = (z % 3 == 1) ? AdjustedSpacing * 0.5f : ((z % 3 == 2) ? AdjustedSpacing * 0.25f : 0.0f);
		const float ZLayerOffsetY = (z % 3 == 1) ? RowSpacingY / 3.0f : ((z % 3 == 2) ? RowSpacingY * 2.0f / 3.0f : 0.0f);

		for (int32 y = 0; y < CountY; ++y)
		{
			// Row offset for hexagonal pattern in XY plane
			const float RowOffsetX = (y % 2 == 1) ? AdjustedSpacing * 0.5f : 0.0f;

			for (int32 x = 0; x < CountX; ++x)
			{
				FVector LocalPos(
					LocalStart.X + x * AdjustedSpacing + RowOffsetX + ZLayerOffsetX,
					LocalStart.Y + y * RowSpacingY + ZLayerOffsetY,
					LocalStart.Z + z * LayerSpacingZ
				);

				// Check bounds
				if (FMath::Abs(LocalPos.X) > Extent.X ||
				    FMath::Abs(LocalPos.Y) > Extent.Y ||
				    FMath::Abs(LocalPos.Z) > Extent.Z)
				{
					continue;
				}

				FVector WorldPos = Center + LocalPos;

				// Apply jitter
				if (SpawnSettings.bUseJitter && JitterRange > 0.0f)
				{
					WorldPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				Positions.Add(WorldPos);
				Velocities.Add(InitialVelocity);
			}
		}
	}

	QueueSpawnRequest(Positions, Velocities);
	return Positions.Num();
}

int32 UKawaiiFluidEmitterComponent::SpawnParticlesCylinderHexagonal(FVector Center, float Radius, float HalfHeight, float Spacing, FVector InitialVelocity)
{
	AKawaiiFluidVolume* Volume = GetTargetVolume();
	if (!Volume || Spacing <= 0.0f || Radius <= 0.0f || HalfHeight <= 0.0f) return 0;

	TArray<FVector> Positions;
	TArray<FVector> Velocities;

	// HCP density compensation (matches KawaiiFluidSimulationModule exactly)
	const float HCPCompensation = 1.122f;
	const float AdjustedSpacing = Spacing * HCPCompensation;

	// Hexagonal Close Packing constants
	const float RowSpacingY = AdjustedSpacing * 0.866025f;   // sqrt(3)/2
	const float LayerSpacingZ = AdjustedSpacing * 0.816497f; // sqrt(2/3)
	const float JitterRange = AdjustedSpacing * SpawnSettings.JitterAmount;
	const float RadiusSq = Radius * Radius;

	// Integer-based grid (matches SimulationModule)
	const int32 GridSizeXY = FMath::CeilToInt(Radius / AdjustedSpacing) + 1;
	const int32 GridSizeY = FMath::CeilToInt(Radius / RowSpacingY) + 1;
	const int32 GridSizeZ = FMath::CeilToInt(HalfHeight / LayerSpacingZ);

	const float EstimatedCount = PI * Radius * Radius * HalfHeight * 2.0f / (AdjustedSpacing * AdjustedSpacing * AdjustedSpacing);
	Positions.Reserve(FMath::CeilToInt(EstimatedCount));
	Velocities.Reserve(FMath::CeilToInt(EstimatedCount));

	for (int32 z = -GridSizeZ; z <= GridSizeZ; ++z)
	{
		// Z layer offset for HCP (ABC stacking pattern - mod 3)
		const int32 ZMod = ((z + GridSizeZ) % 3);
		const float ZLayerOffsetX = (ZMod == 1) ? AdjustedSpacing * 0.5f : ((ZMod == 2) ? AdjustedSpacing * 0.25f : 0.0f);
		const float ZLayerOffsetY = (ZMod == 1) ? RowSpacingY / 3.0f : ((ZMod == 2) ? RowSpacingY * 2.0f / 3.0f : 0.0f);

		for (int32 y = -GridSizeY; y <= GridSizeY; ++y)
		{
			const float RowOffsetX = (((y + GridSizeY) % 2) == 1) ? AdjustedSpacing * 0.5f : 0.0f;

			for (int32 x = -GridSizeXY; x <= GridSizeXY; ++x)
			{
				FVector LocalPos(
					x * AdjustedSpacing + RowOffsetX + ZLayerOffsetX,
					y * RowSpacingY + ZLayerOffsetY,
					z * LayerSpacingZ
				);

				// Check cylinder bounds (XY plane for radius, Z for height)
				const float XYDistSq = LocalPos.X * LocalPos.X + LocalPos.Y * LocalPos.Y;
				if (XYDistSq > RadiusSq || FMath::Abs(LocalPos.Z) > HalfHeight)
				{
					continue;
				}

				FVector WorldPos = Center + LocalPos;

				// Apply jitter
				if (SpawnSettings.bUseJitter && JitterRange > 0.0f)
				{
					WorldPos += FVector(
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange),
						FMath::FRandRange(-JitterRange, JitterRange)
					);
				}

				Positions.Add(WorldPos);
				Velocities.Add(InitialVelocity);
			}
		}
	}

	QueueSpawnRequest(Positions, Velocities);
	return Positions.Num();
}

void UKawaiiFluidEmitterComponent::SpawnParticlesSphereRandom(FVector Center, float Radius, int32 Count, FVector InitialVelocity)
{
	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	Positions.Reserve(Count);
	Velocities.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FVector RandomPoint;
		do
		{
			RandomPoint = FVector(
				FMath::FRandRange(-1.0f, 1.0f),
				FMath::FRandRange(-1.0f, 1.0f),
				FMath::FRandRange(-1.0f, 1.0f)
			);
		} while (RandomPoint.SizeSquared() > 1.0f);

		Positions.Add(Center + RandomPoint * Radius);
		Velocities.Add(InitialVelocity);
	}

	QueueSpawnRequest(Positions, Velocities);
}

void UKawaiiFluidEmitterComponent::SpawnParticlesBoxRandom(FVector Center, FVector HalfExtent, int32 Count, FVector InitialVelocity)
{
	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	Positions.Reserve(Count);
	Velocities.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FVector RandomPoint(
			FMath::FRandRange(-HalfExtent.X, HalfExtent.X),
			FMath::FRandRange(-HalfExtent.Y, HalfExtent.Y),
			FMath::FRandRange(-HalfExtent.Z, HalfExtent.Z)
		);
		Positions.Add(Center + RandomPoint);
		Velocities.Add(InitialVelocity);
	}

	QueueSpawnRequest(Positions, Velocities);
}

void UKawaiiFluidEmitterComponent::SpawnParticlesCylinderRandom(FVector Center, float Radius, float HalfHeight, int32 Count, FVector InitialVelocity)
{
	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	Positions.Reserve(Count);
	Velocities.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float R = Radius * FMath::Sqrt(FMath::FRand());
		const float Z = FMath::FRandRange(-HalfHeight, HalfHeight);

		FVector RandomPoint(R * FMath::Cos(Angle), R * FMath::Sin(Angle), Z);
		Positions.Add(Center + RandomPoint);
		Velocities.Add(InitialVelocity);
	}

	QueueSpawnRequest(Positions, Velocities);
}

void UKawaiiFluidEmitterComponent::SpawnParticleDirectional(FVector Position, FVector Direction, float Speed, int32 Count, float ConeAngle)
{
	AKawaiiFluidVolume* Volume = GetTargetVolume();
	if (!Volume) return;

	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	Positions.Reserve(Count);
	Velocities.Reserve(Count);

	// Normalize direction
	FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector(0, 0, -1);  // Default: downward
	}

	const FVector BaseVelocity = Dir * Speed;
	const float SpreadRadians = FMath::DegreesToRadians(ConeAngle);
	const float StreamRadius = SpawnSettings.StreamRadius;

	// Build orthonormal basis perpendicular to direction (same as KawaiiFluidComponent)
	FVector Right, Up;
	Dir.FindBestAxisVectors(Right, Up);

	for (int32 i = 0; i < Count; ++i)
	{
		// Circular distribution within stream radius (perpendicular to direction)
		// This matches KawaiiFluidComponent's SimulationModule::SpawnParticleDirectional
		FVector SpawnPos = Position;
		if (StreamRadius > 0.0f)
		{
			const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
			const float RandomRadius = FMath::FRandRange(0.0f, StreamRadius);
			SpawnPos += Right * FMath::Cos(RandomAngle) * RandomRadius;
			SpawnPos += Up * FMath::Sin(RandomAngle) * RandomRadius;
		}

		FVector Velocity = BaseVelocity;
		if (ConeAngle > 0.0f)
		{
			// Random cone direction for spray (same algorithm as SimulationModule)
			const float RandomPhi = FMath::FRandRange(0.0f, 2.0f * PI);
			const float RandomTheta = FMath::FRandRange(0.0f, SpreadRadians);

			const float SinTheta = FMath::Sin(RandomTheta);
			FVector RandomDir = Dir * FMath::Cos(RandomTheta)
				+ Right * SinTheta * FMath::Cos(RandomPhi)
				+ Up * SinTheta * FMath::Sin(RandomPhi);

			Velocity = RandomDir.GetSafeNormal() * Speed;
		}

		Positions.Add(SpawnPos);
		Velocities.Add(Velocity);
	}

	QueueSpawnRequest(Positions, Velocities);
}

void UKawaiiFluidEmitterComponent::SpawnHexagonalLayer(FVector Position, FVector Direction, float Speed, float Radius, float Spacing)
{
	AKawaiiFluidVolume* Volume = GetTargetVolume();
	if (!Volume || Spacing <= 0.0f || Radius <= 0.0f) return;

	TArray<FVector> Positions;
	TArray<FVector> Velocities;

	// Normalize direction (matches SimulationModule::SpawnParticleDirectionalHexLayerBatch)
	FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector(0, 0, -1);  // Default: downward
	}

	// Create local coordinate system (same method as SimulationModule)
	FVector Right, Up;
	Dir.FindBestAxisVectors(Right, Up);

	// NO HCP compensation for 2D hexagonal layer!
	// This matches SimulationModule::SpawnParticleDirectionalHexLayerBatch exactly
	const float RowSpacing = Spacing * FMath::Sqrt(3.0f) * 0.5f;  // ~0.866 * Spacing
	const float RadiusSq = Radius * Radius;

	// Jitter setup (matches SimulationModule)
	const float Jitter = FMath::Clamp(SpawnSettings.StreamJitter, 0.0f, 0.5f);
	const float MaxJitterOffset = Spacing * Jitter;
	const bool bApplyJitter = Jitter > KINDA_SMALL_NUMBER;

	const FVector SpawnVel = Dir * Speed;

	// Row count calculation (matches SimulationModule)
	const int32 NumRows = FMath::CeilToInt(Radius / RowSpacing) * 2 + 1;
	const int32 HalfRows = NumRows / 2;

	// Estimate and reserve
	const int32 EstimatedCount = FMath::CeilToInt((PI * RadiusSq) / (Spacing * Spacing));
	Positions.Reserve(EstimatedCount);
	Velocities.Reserve(EstimatedCount);

	// Hexagonal grid iteration (matches SimulationModule exactly)
	for (int32 RowIdx = -HalfRows; RowIdx <= HalfRows; ++RowIdx)
	{
		const float LocalY = RowIdx * RowSpacing;
		const float LocalYSq = LocalY * LocalY;

		// Skip rows outside the circle
		if (LocalYSq > RadiusSq)
		{
			continue;
		}

		const float MaxX = FMath::Sqrt(RadiusSq - LocalYSq);

		// Odd rows get X offset (Hexagonal Packing) - uses Abs like SimulationModule
		const float XOffset = (FMath::Abs(RowIdx) % 2 != 0) ? Spacing * 0.5f : 0.0f;

		// Calculate column count
		const int32 NumCols = FMath::FloorToInt(MaxX / Spacing);

		for (int32 ColIdx = -NumCols; ColIdx <= NumCols; ++ColIdx)
		{
			float LocalX = ColIdx * Spacing + XOffset;
			float LocalYFinal = LocalY;

			// Apply jitter
			if (bApplyJitter)
			{
				LocalX += FMath::FRandRange(-MaxJitterOffset, MaxJitterOffset);
				LocalYFinal += FMath::FRandRange(-MaxJitterOffset, MaxJitterOffset);
			}

			// Check inside circle (after jitter)
			if (LocalX * LocalX + LocalYFinal * LocalYFinal <= RadiusSq)
			{
				FVector SpawnPos = Position + Right * LocalX + Up * LocalYFinal;
				Positions.Add(SpawnPos);
				Velocities.Add(SpawnVel);
			}
		}
	}

	QueueSpawnRequest(Positions, Velocities);
}

void UKawaiiFluidEmitterComponent::QueueSpawnRequest(const TArray<FVector>& Positions, const TArray<FVector>& Velocities)
{
	AKawaiiFluidVolume* Volume = GetTargetVolume();
	if (!Volume || Positions.Num() == 0)
	{
		return;
	}

	// Get SourceID from owner emitter
	int32 SourceID = -1;
	if (AKawaiiFluidEmitter* Emitter = GetOwnerEmitter())
	{
		SourceID = Emitter->GetUniqueID();
	}

	// Queue spawn requests to Volume's batch queue
	Volume->QueueSpawnRequests(Positions, Velocities, SourceID);

	SpawnedParticleCount += Positions.Num();
}

UKawaiiFluidSimulationModule* UKawaiiFluidEmitterComponent::GetSimulationModule() const
{
	if (AKawaiiFluidVolume* Volume = GetTargetVolume())
	{
		return Volume->GetSimulationModule();
	}
	return nullptr;
}

void UKawaiiFluidEmitterComponent::RecycleOldestParticlesIfNeeded(int32 NewParticleCount)
{
	if (!bRecycleOldestParticles || SpawnSettings.MaxParticleCount <= 0)
	{
		return;
	}

	UKawaiiFluidSimulationModule* Module = GetSimulationModule();
	if (!Module)
	{
		return;
	}

	const int32 CurrentCount = Module->GetParticleCount();
	const int32 ExcessCount = (CurrentCount + NewParticleCount) - SpawnSettings.MaxParticleCount;

	if (ExcessCount > 0)
	{
		Module->RemoveOldestParticles(ExcessCount);
	}
}

FVector UKawaiiFluidEmitterComponent::ApplyJitter(FVector Position, float Spacing) const
{
	const float JitterRange = Spacing * SpawnSettings.JitterAmount;
	return Position + FVector(
		FMath::FRandRange(-JitterRange, JitterRange),
		FMath::FRandRange(-JitterRange, JitterRange),
		FMath::FRandRange(-JitterRange, JitterRange)
	);
}

//========================================
// Volume Registration
//========================================

void UKawaiiFluidEmitterComponent::RegisterToVolume()
{
	if (TargetVolume)
	{
		if (AKawaiiFluidEmitter* Emitter = GetOwnerEmitter())
		{
			TargetVolume->RegisterEmitter(Emitter);
		}
	}
}

void UKawaiiFluidEmitterComponent::UnregisterFromVolume()
{
	if (TargetVolume)
	{
		if (AKawaiiFluidEmitter* Emitter = GetOwnerEmitter())
		{
			TargetVolume->UnregisterEmitter(Emitter);
		}
	}
}

AKawaiiFluidVolume* UKawaiiFluidEmitterComponent::FindNearestVolume() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Get subsystem to find registered volumes
	UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>();
	if (!Subsystem)
	{
		return nullptr;
	}

	const FVector EmitterLocation = GetComponentLocation();
	AKawaiiFluidVolume* NearestVolume = nullptr;
	float NearestDistSq = FLT_MAX;

	for (const TObjectPtr<AKawaiiFluidVolume>& Volume : Subsystem->GetAllVolumes())
	{
		if (Volume)
		{
			const float DistSq = FVector::DistSquared(EmitterLocation, Volume->GetActorLocation());
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				NearestVolume = Volume;
			}
		}
	}

	return NearestVolume;
}

#if WITH_EDITOR
void UKawaiiFluidEmitterComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ?
		PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidEmitterComponent, TargetVolume))
	{
		// Re-register when target volume changes
		if (HasBegunPlay())
		{
			UnregisterFromVolume();
			RegisterToVolume();
		}
	}
}

void UKawaiiFluidEmitterComponent::DrawSpawnVolumeVisualization()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Location = GetComponentLocation() + SpawnSettings.SpawnOffset;
	const FQuat Rotation = GetComponentRotation().Quaternion();
	const FColor SpawnColor = SpawnVolumeWireframeColor;
	const float Duration = -1.0f;  // Redraw each frame
	const uint8 DepthPriority = 0;
	const float Thickness = WireframeThickness;

	if (IsShapeVolumeMode())
	{
		// Shape Volume visualization
		switch (SpawnSettings.ShapeType)
		{
		case EFluidShapeType::Sphere:
			// Sphere is rotation-independent
			DrawDebugSphere(World, Location, SpawnSettings.SphereRadius, 24, SpawnColor, false, Duration, DepthPriority, Thickness);
			break;

		case EFluidShapeType::Box:
			// Box with rotation
			DrawDebugBox(World, Location, SpawnSettings.BoxExtent, Rotation, SpawnColor, false, Duration, DepthPriority, Thickness);
			break;

		case EFluidShapeType::Cylinder:
			{
				const float Radius = SpawnSettings.CylinderRadius;
				const float HalfHeight = SpawnSettings.CylinderHalfHeight;

				// Local coordinate cylinder vertices, then apply rotation
				const FVector LocalTopCenter = FVector(0, 0, HalfHeight);
				const FVector LocalBottomCenter = FVector(0, 0, -HalfHeight);

				const int32 NumSegments = 24;
				for (int32 i = 0; i < NumSegments; ++i)
				{
					const float Angle1 = (float)i / NumSegments * 2.0f * PI;
					const float Angle2 = (float)(i + 1) / NumSegments * 2.0f * PI;

					// Calculate local positions
					const FVector LocalTopP1 = LocalTopCenter + FVector(FMath::Cos(Angle1), FMath::Sin(Angle1), 0) * Radius;
					const FVector LocalTopP2 = LocalTopCenter + FVector(FMath::Cos(Angle2), FMath::Sin(Angle2), 0) * Radius;
					const FVector LocalBottomP1 = LocalBottomCenter + FVector(FMath::Cos(Angle1), FMath::Sin(Angle1), 0) * Radius;
					const FVector LocalBottomP2 = LocalBottomCenter + FVector(FMath::Cos(Angle2), FMath::Sin(Angle2), 0) * Radius;

					// Apply rotation and transform to world position
					const FVector TopP1 = Location + Rotation.RotateVector(LocalTopP1);
					const FVector TopP2 = Location + Rotation.RotateVector(LocalTopP2);
					const FVector BottomP1 = Location + Rotation.RotateVector(LocalBottomP1);
					const FVector BottomP2 = Location + Rotation.RotateVector(LocalBottomP2);

					DrawDebugLine(World, TopP1, TopP2, SpawnColor, false, Duration, DepthPriority, Thickness);
					DrawDebugLine(World, BottomP1, BottomP2, SpawnColor, false, Duration, DepthPriority, Thickness);
				}

				// Vertical lines (4 lines connecting top and bottom)
				for (int32 i = 0; i < 4; ++i)
				{
					const float Angle = (float)i / 4 * 2.0f * PI;
					const FVector LocalTopP = LocalTopCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius;
					const FVector LocalBottomP = LocalBottomCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius;

					const FVector TopP = Location + Rotation.RotateVector(LocalTopP);
					const FVector BottomP = Location + Rotation.RotateVector(LocalBottomP);
					DrawDebugLine(World, TopP, BottomP, SpawnColor, false, Duration, DepthPriority, Thickness);
				}
			}
			break;
		}
	}
	else // Emitter mode
	{
		// Direction arrow (apply component rotation)
		const FVector WorldDir = Rotation.RotateVector(SpawnSettings.SpawnDirection.GetSafeNormal());
		const float ArrowLength = 100.0f;
		const FVector EndPoint = Location + WorldDir * ArrowLength;

		DrawDebugDirectionalArrow(World, Location, EndPoint, 20.0f, SpawnColor, false, Duration, DepthPriority, Thickness);

		// Stream radius circle
		if (SpawnSettings.StreamRadius > 0.0f)
		{
			FVector Right, Up;
			WorldDir.FindBestAxisVectors(Right, Up);

			const int32 NumSegments = 24;
			for (int32 i = 0; i < NumSegments; ++i)
			{
				const float Angle1 = (float)i / NumSegments * 2.0f * PI;
				const float Angle2 = (float)(i + 1) / NumSegments * 2.0f * PI;

				const FVector P1 = Location + (Right * FMath::Cos(Angle1) + Up * FMath::Sin(Angle1)) * SpawnSettings.StreamRadius;
				const FVector P2 = Location + (Right * FMath::Cos(Angle2) + Up * FMath::Sin(Angle2)) * SpawnSettings.StreamRadius;

				DrawDebugLine(World, P1, P2, SpawnColor, false, Duration, DepthPriority, Thickness);
			}
		}

		// Spray emitter: show cone
		if (SpawnSettings.EmitterType == EFluidEmitterType::Spray && SpawnSettings.ConeAngle > 0.0f)
		{
			const float ConeLength = 80.0f;
			const float HalfAngleRad = FMath::DegreesToRadians(SpawnSettings.ConeAngle * 0.5f);
			const float ConeRadius = ConeLength * FMath::Tan(HalfAngleRad);

			FVector ConeRight, ConeUp;
			WorldDir.FindBestAxisVectors(ConeRight, ConeUp);

			// Cone lines from apex to base
			const int32 NumLines = 8;
			const FVector ConeCenter = Location + WorldDir * ConeLength;

			for (int32 i = 0; i < NumLines; ++i)
			{
				const float Angle = (float)i / NumLines * 2.0f * PI;
				const FVector ConePoint = ConeCenter + (ConeRight * FMath::Cos(Angle) + ConeUp * FMath::Sin(Angle)) * ConeRadius;
				DrawDebugLine(World, Location, ConePoint, FColor::Orange, false, Duration, DepthPriority, Thickness * 0.5f);
			}

			// Cone base circle
			for (int32 i = 0; i < NumLines; ++i)
			{
				const float Angle1 = (float)i / NumLines * 2.0f * PI;
				const float Angle2 = (float)(i + 1) / NumLines * 2.0f * PI;

				const FVector P1 = ConeCenter + (ConeRight * FMath::Cos(Angle1) + ConeUp * FMath::Sin(Angle1)) * ConeRadius;
				const FVector P2 = ConeCenter + (ConeRight * FMath::Cos(Angle2) + ConeUp * FMath::Sin(Angle2)) * ConeRadius;

				DrawDebugLine(World, P1, P2, FColor::Orange, false, Duration, DepthPriority, Thickness * 0.5f);
			}
		}
	}
}
#endif
