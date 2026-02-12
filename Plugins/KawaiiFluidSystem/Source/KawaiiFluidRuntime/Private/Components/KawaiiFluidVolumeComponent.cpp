// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Components/KawaiiFluidVolumeComponent.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Actors/KawaiiFluidVolume.h"
#include "Rendering/KawaiiFluidRenderer.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

/**
 * @brief Default constructor for UKawaiiFluidVolumeComponent. Sets up default sizes and preset.
 */
UKawaiiFluidVolumeComponent::UKawaiiFluidVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Enable ticking in editor for debug visualization
	bTickInEditor = true;

	// UBoxComponent configuration for editor visualization and selection
	UBoxComponent::SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UBoxComponent::SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);

	// Wireframe visualization settings
	LineThickness = 2.0f;
	ShapeColor = FColor::Green;
	bHiddenInGame = true;  // Hide wireframe at runtime by default

	// Load default Preset (DA_KF_Water)
	static ConstructorHelpers::FObjectFinder<UKawaiiFluidPresetDataAsset> DefaultPresetFinder(
		TEXT("/KawaiiFluidSystem/Preset/DA_KF_Water.DA_KF_Water"));
	if (DefaultPresetFinder.Succeeded())
	{
		Preset = DefaultPresetFinder.Object;
	}

	// Initialize default volume size based on Medium Z-Order preset and default CellSize (20.0f)
	const float MediumGridResolution = static_cast<float>(GridResolutionPresetHelper::GetGridResolution(EGridResolutionPreset::Medium));
	const float DefaultCellSize = 20.0f;  // Default fallback when no Preset is set
	const float DefaultVolumeSize = MediumGridResolution * DefaultCellSize;
	UniformVolumeSize = DefaultVolumeSize;
	VolumeSize = FVector(DefaultVolumeSize);

	// Initialize BoxExtent directly (don't call SetBoxExtent in constructor)
	BoxExtent = FVector(DefaultVolumeSize * 0.5f);

	// Initialize grid parameters
	CellSize = DefaultCellSize;
	GridResolutionPreset = EGridResolutionPreset::Medium;
	GridAxisBits = GridResolutionPresetHelper::GetAxisBits(GridResolutionPreset);
	GridResolution = GridResolutionPresetHelper::GetGridResolution(GridResolutionPreset);
	MaxCells = GridResolutionPresetHelper::GetMaxCells(GridResolutionPreset);
	BoundsExtent = static_cast<float>(GridResolution) * CellSize;
}

/**
 * @brief Called when the component is registered. Triggers initial bounds calculation.
 */
void UKawaiiFluidVolumeComponent::OnRegister()
{
	Super::OnRegister();
	RecalculateBounds();
}

/**
 * @brief Called when the component is unregistered. Handles subsystem cleanup.
 */
void UKawaiiFluidVolumeComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

/**
 * @brief Called when the game starts. Registers with the fluid subsystem.
 */
void UKawaiiFluidVolumeComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterToSubsystem();
	RecalculateBounds();
}

/**
 * @brief Called when the component is destroyed.
 * @param EndPlayReason Reason for termination
 */
void UKawaiiFluidVolumeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

/**
 * @brief Updates the volume each frame, ensuring bounds are synced.
 * @param DeltaTime Frame time
 * @param TickType Tick type
 * @param ThisTickFunction Function reference
 */
void UKawaiiFluidVolumeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update bounds if component moved
	RecalculateBounds();

	// Update wireframe color based on selection
	if (AActor* Owner = GetOwner())
	{
		ShapeColor = Owner->IsSelected() ? FColor::Yellow : BoundsColor;
	}

	// Draw additional debug visualization (Z-Order space, info text)
	DrawBoundsVisualization();
}

#if WITH_EDITOR
/**
 * @brief Handles property updates in the editor, ensuring size constraints and mode switches.
 * @param PropertyChangedEvent Property change information
 */
void UKawaiiFluidVolumeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ?
		PropertyChangedEvent.Property->GetFName() : NAME_None;

	const FName MemberPropertyName = PropertyChangedEvent.MemberProperty ?
		PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// Sync size values when toggling Uniform Size mode
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUniformSize))
	{
		if (bUniformSize) UniformVolumeSize = FMath::Max3(VolumeSize.X, VolumeSize.Y, VolumeSize.Z);
		else VolumeSize = FVector(UniformVolumeSize);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, UniformVolumeSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUniformSize))
	{
		UniformVolumeSize = FMath::Max(UniformVolumeSize, 10.0f);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, VolumeSize) ||
		MemberPropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, VolumeSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUniformSize))
	{
		VolumeSize.X = FMath::Max(VolumeSize.X, 10.0f);
		VolumeSize.Y = FMath::Max(VolumeSize.Y, 10.0f);
		VolumeSize.Z = FMath::Max(VolumeSize.Z, 10.0f);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUseHybridTiledZOrder))
	{
		if (!bUseHybridTiledZOrder && bUseUnlimitedSize)
		{
			bUseUnlimitedSize = false; bShowBoundsInEditor = true; bShowBoundsAtRuntime = false; SetVisibility(true);
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUseUnlimitedSize))
	{
		if (bUseUnlimitedSize) { bShowBoundsInEditor = false; bShowBoundsAtRuntime = false; SetVisibility(false); }
		else { bShowBoundsInEditor = true; bShowBoundsAtRuntime = false; SetVisibility(true); }
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUniformSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, UniformVolumeSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, VolumeSize) ||
		MemberPropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, VolumeSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, Preset))
	{
		RecalculateBounds();
		for (TWeakObjectPtr<UKawaiiFluidSimulationModule>& WeakModule : RegisteredModules)
		{
			if (UKawaiiFluidSimulationModule* Module = WeakModule.Get()) Module->UpdateVolumeInfoDisplay();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, Preset))
	{
		if (AKawaiiFluidVolume* Volume = Cast<AKawaiiFluidVolume>(GetOwner()))
		{
			if (UKawaiiFluidRenderingModule* RenderingMod = Volume->GetRenderingModule())
			{
				if (UKawaiiFluidRenderer* MR = RenderingMod->GetMetaballRenderer()) MR->SetPreset(Preset);
			}
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, BoundsColor)) ShapeColor = BoundsColor;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, BoundsLineThickness)) SetLineThickness(BoundsLineThickness);
}
#endif

/**
 * @brief Calculates world-space bounds and selects optimal spatial partitioning parameters.
 */
void UKawaiiFluidVolumeComponent::RecalculateBounds()
{
	CellSize = Preset ? FMath::Max(Preset->SmoothingRadius, 1.0f) : 20.0f;

	const FVector OriginalHalfExtent = GetEffectiveVolumeSize() * 0.5f;
	FVector WorkingHalfExtent = OriginalHalfExtent;
	FVector EffectiveHalfExtent = WorkingHalfExtent;
	const FQuat ComponentRotation = GetComponentQuat();

	auto ComputeRotatedAABBHalfExtent = [&ComponentRotation](const FVector& OBBHalfExtent) -> FVector
	{
		if (ComponentRotation.IsIdentity()) return OBBHalfExtent;
		FVector RotatedCorners[8];
		for (int32 i = 0; i < 8; ++i) { FVector Corner((i & 1) ? OBBHalfExtent.X : -OBBHalfExtent.X, (i & 2) ? OBBHalfExtent.Y : -OBBHalfExtent.Y, (i & 4) ? OBBHalfExtent.Z : -OBBHalfExtent.Z); RotatedCorners[i] = ComponentRotation.RotateVector(Corner); }
		FVector AABBMin = RotatedCorners[0], AABBMax = RotatedCorners[0];
		for (int32 i = 1; i < 8; ++i) { AABBMin = AABBMin.ComponentMin(RotatedCorners[i]); AABBMax = AABBMax.ComponentMax(RotatedCorners[i]); }
		return FVector(FMath::Max(FMath::Abs(AABBMin.X), FMath::Abs(AABBMax.X)), FMath::Max(FMath::Abs(AABBMin.Y), FMath::Abs(AABBMax.Y)), FMath::Max(FMath::Abs(AABBMin.Z), FMath::Abs(AABBMax.Z)));
	};

	if (!bUseHybridTiledZOrder)
	{
		const float LargeMaxHalfExtent = GridResolutionPresetHelper::GetMaxExtentForPreset(EGridResolutionPreset::Large, CellSize);
		WorkingHalfExtent = GridResolutionPresetHelper::ClampExtentToMaxSupported(OriginalHalfExtent, CellSize);
		EffectiveHalfExtent = WorkingHalfExtent;
		if (!ComponentRotation.IsIdentity())
		{
			EffectiveHalfExtent = ComputeRotatedAABBHalfExtent(WorkingHalfExtent);
			const float MaxAABBHalfExtent = EffectiveHalfExtent.GetMax();
			if (MaxAABBHalfExtent > LargeMaxHalfExtent) { WorkingHalfExtent *= (LargeMaxHalfExtent / MaxAABBHalfExtent); EffectiveHalfExtent = ComputeRotatedAABBHalfExtent(WorkingHalfExtent); }
		}
		if (!WorkingHalfExtent.Equals(OriginalHalfExtent, 0.01f))
		{
			VolumeSize = WorkingHalfExtent * 2.0f;
			if (bUniformSize) UniformVolumeSize = VolumeSize.GetMax();
		}
	}
	else if (!ComponentRotation.IsIdentity()) EffectiveHalfExtent = ComputeRotatedAABBHalfExtent(WorkingHalfExtent);

	if (!bUseUnlimitedSize) { if (IsRegistered()) SetBoxExtent(WorkingHalfExtent, false); else BoxExtent = WorkingHalfExtent; }
	else BoxExtent = FVector(1.0f, 1.0f, 1.0f);

	GridResolutionPreset = GridResolutionPresetHelper::SelectPresetForExtent(EffectiveHalfExtent, CellSize);
	GridAxisBits = GridResolutionPresetHelper::GetAxisBits(GridResolutionPreset);
	GridResolution = GridResolutionPresetHelper::GetGridResolution(GridResolutionPreset);
	MaxCells = GridResolutionPresetHelper::GetMaxCells(GridResolutionPreset);
	BoundsExtent = static_cast<float>(GridResolution) * CellSize;

	const float ActualHalfExtent = BoundsExtent * 0.5f;
	WorldBoundsMin = GetComponentLocation() - FVector(ActualHalfExtent);
	WorldBoundsMax = GetComponentLocation() + FVector(ActualHalfExtent);
}

/**
 * @brief Checks if a world position is within this Volume's bounds.
 * @param WP World position to check
 * @return True if inside
 */
bool UKawaiiFluidVolumeComponent::IsPositionInBounds(const FVector& WP) const
{
	return WP.X >= WorldBoundsMin.X && WP.X <= WorldBoundsMax.X && WP.Y >= WorldBoundsMin.Y && WP.Y <= WorldBoundsMax.Y && WP.Z >= WorldBoundsMin.Z && WP.Z <= WorldBoundsMax.Z;
}

/**
 * @brief Returns the simulation bounds in world space.
 * @param OutMin Minimum corner
 * @param OutMax Maximum corner
 */
void UKawaiiFluidVolumeComponent::GetSimulationBounds(FVector& OutMin, FVector& OutMax) const
{
	OutMin = WorldBoundsMin; OutMax = WorldBoundsMax;
}

/**
 * @brief Registers a fluid module to this volume.
 * @param Module Simulation module pointer
 */
void UKawaiiFluidVolumeComponent::RegisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module && !RegisteredModules.Contains(Module)) RegisteredModules.Add(Module);
}

/**
 * @brief Unregisters a fluid module.
 * @param Module Simulation module pointer
 */
void UKawaiiFluidVolumeComponent::UnregisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module) RegisteredModules.Remove(Module);
}

/**
 * @brief Registers this component with the fluid subsystem.
 */
void UKawaiiFluidVolumeComponent::RegisterToSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>()) Subsystem->RegisterVolumeComponent(this);
	}
}

/**
 * @brief Unregisters this component from the fluid subsystem.
 */
void UKawaiiFluidVolumeComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>()) Subsystem->UnregisterVolumeComponent(this);
	}
}

/**
 * @brief Handles visual debug rendering for the volume boundaries.
 */
void UKawaiiFluidVolumeComponent::DrawBoundsVisualization()
{
	if (bUseUnlimitedSize) return;
	UWorld* World = GetWorld(); if (!World) return;
	if (bShowZOrderSpaceWireframe)
	{
		const FVector C = (WorldBoundsMin + WorldBoundsMax) * 0.5f, E = (WorldBoundsMax - WorldBoundsMin) * 0.5f;
		DrawDebugBox(World, C, E, FQuat::Identity, ZOrderSpaceWireframeColor, false, -1.0f, 0, 1.0f);
	}
#if WITH_EDITOR
	if (!World->IsGameWorld() && bShowBoundsInEditor)
	{
		const FVector ES = GetEffectiveVolumeSize();
		const FString IT = FString::Printf(TEXT("Size: %.0fx%.0fx%.0f cm\nBounce: %.1f, Friction: %.1f"), ES.X, ES.Y, ES.Z, GetWallBounce(), GetWallFriction());
		DrawDebugString(World, GetComponentLocation() + FVector(0, 0, GetVolumeHalfExtent().Z + 50.0f), IT, nullptr, ShapeColor, -1.0f, true);
	}
#endif
}

/**
 * @brief Retrieves the particle spacing from the preset.
 * @return Spacing in cm
 */
float UKawaiiFluidVolumeComponent::GetParticleSpacing() const { return Preset ? Preset->ParticleRadius * 2.0f : 10.0f; }

/**
 * @brief Returns the wall bounce coefficient.
 * @return Bounce factor
 */
float UKawaiiFluidVolumeComponent::GetWallBounce() const { return Preset ? Preset->Bounciness : 0.0f; }

/**
 * @brief Returns the wall friction coefficient.
 * @return Friction factor
 */
float UKawaiiFluidVolumeComponent::GetWallFriction() const { return Preset ? Preset->Friction : 0.5f; }

/**
 * @brief Sets the current debug draw mode.
 * @param Mode Draw mode
 */
void UKawaiiFluidVolumeComponent::SetDebugDrawMode(EKawaiiFluidDebugDrawMode Mode) { DebugDrawMode = Mode; }

/**
 * @brief Disables debug particle drawing.
 */
void UKawaiiFluidVolumeComponent::DisableDebugDraw() { DebugDrawMode = EKawaiiFluidDebugDrawMode::None; }