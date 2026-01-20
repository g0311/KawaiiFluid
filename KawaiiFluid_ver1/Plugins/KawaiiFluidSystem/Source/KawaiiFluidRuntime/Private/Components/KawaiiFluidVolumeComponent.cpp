// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/KawaiiFluidVolumeComponent.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "DrawDebugHelpers.h"

UKawaiiFluidVolumeComponent::UKawaiiFluidVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Enable ticking in editor for debug visualization
	bTickInEditor = true;

	// Initialize default volume size based on Medium Z-Order preset and CellSize
	// Formula: GridResolution(Medium) * CellSize = 128 * CellSize
	const float MediumGridResolution = static_cast<float>(GridResolutionPresetHelper::GetGridResolution(EGridResolutionPreset::Medium));
	const float DefaultVolumeSize = MediumGridResolution * CellSize;
	UniformVolumeSize = DefaultVolumeSize;
	VolumeSize = FVector(DefaultVolumeSize);

	// Calculate initial bounds
	RecalculateBounds();
}

void UKawaiiFluidVolumeComponent::OnRegister()
{
	Super::OnRegister();
	RecalculateBounds();
}

void UKawaiiFluidVolumeComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

void UKawaiiFluidVolumeComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterToSubsystem();
	RecalculateBounds();
}

void UKawaiiFluidVolumeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void UKawaiiFluidVolumeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update bounds if component moved
	RecalculateBounds();

	// Draw debug visualization
	const bool bShouldDraw = GetWorld() &&
		((bShowBoundsInEditor && !GetWorld()->IsGameWorld()) ||
		 (bShowBoundsAtRuntime && GetWorld()->IsGameWorld()));

	if (bShouldDraw)
	{
		DrawBoundsVisualization();
	}
}

#if WITH_EDITOR
void UKawaiiFluidVolumeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ?
		PropertyChangedEvent.Property->GetFName() : NAME_None;

	// Sync size values when toggling Uniform Size mode
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUniformSize))
	{
		if (bUniformSize)
		{
			// Switching to uniform mode: use max of VolumeSize components
			UniformVolumeSize = FMath::Max3(VolumeSize.X, VolumeSize.Y, VolumeSize.Z);
		}
		else
		{
			// Switching to non-uniform mode: copy UniformVolumeSize to all axes
			VolumeSize = FVector(UniformVolumeSize);
		}
	}

	// When CellSize changes, recalculate volume size to maintain Medium preset
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, CellSize))
	{
		const float MediumGridResolution = static_cast<float>(GridResolutionPresetHelper::GetGridResolution(EGridResolutionPreset::Medium));
		const float NewDefaultSize = MediumGridResolution * CellSize;
		UniformVolumeSize = NewDefaultSize;
		VolumeSize = FVector(NewDefaultSize);
	}

	// Handle size-related property changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, bUniformSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, UniformVolumeSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, VolumeSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidVolumeComponent, CellSize))
	{
		RecalculateBounds();

		// Notify all registered modules to update their volume info display
		for (TWeakObjectPtr<UKawaiiFluidSimulationModule>& WeakModule : RegisteredModules)
		{
			if (UKawaiiFluidSimulationModule* Module = WeakModule.Get())
			{
				Module->UpdateVolumeInfoDisplay();
			}
		}
	}
}
#endif

void UKawaiiFluidVolumeComponent::RecalculateBounds()
{
	// Ensure valid CellSize
	CellSize = FMath::Max(CellSize, 1.0f);

	// Get user-defined volume size (full size)
	const FVector EffectiveSize = GetEffectiveVolumeSize();
	const FVector HalfExtent = EffectiveSize * 0.5f;

	// Auto-select optimal GridResolutionPreset based on volume size
	GridResolutionPreset = GridResolutionPresetHelper::SelectPresetForExtent(HalfExtent, CellSize);

	// Update grid parameters from auto-selected preset
	GridAxisBits = GridResolutionPresetHelper::GetAxisBits(GridResolutionPreset);
	GridResolution = GridResolutionPresetHelper::GetGridResolution(GridResolutionPreset);
	MaxCells = GridResolutionPresetHelper::GetMaxCells(GridResolutionPreset);

	// Calculate actual bounds extent from the selected preset
	// This may be larger than requested to fit Z-Order grid constraints
	BoundsExtent = static_cast<float>(GridResolution) * CellSize;

	// Get component world location
	const FVector ComponentLocation = GetComponentLocation();

	// Calculate world bounds (centered on component)
	// Use the actual BoundsExtent from grid for consistency with Z-Order space
	const float ActualHalfExtent = BoundsExtent * 0.5f;
	WorldBoundsMin = ComponentLocation - FVector(ActualHalfExtent, ActualHalfExtent, ActualHalfExtent);
	WorldBoundsMax = ComponentLocation + FVector(ActualHalfExtent, ActualHalfExtent, ActualHalfExtent);
}

bool UKawaiiFluidVolumeComponent::IsPositionInBounds(const FVector& WorldPosition) const
{
	return WorldPosition.X >= WorldBoundsMin.X && WorldPosition.X <= WorldBoundsMax.X &&
	       WorldPosition.Y >= WorldBoundsMin.Y && WorldPosition.Y <= WorldBoundsMax.Y &&
	       WorldPosition.Z >= WorldBoundsMin.Z && WorldPosition.Z <= WorldBoundsMax.Z;
}

void UKawaiiFluidVolumeComponent::GetSimulationBounds(FVector& OutMin, FVector& OutMax) const
{
	OutMin = WorldBoundsMin;
	OutMax = WorldBoundsMax;
}

void UKawaiiFluidVolumeComponent::RegisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module && !RegisteredModules.Contains(Module))
	{
		RegisteredModules.Add(Module);
	}
}

void UKawaiiFluidVolumeComponent::UnregisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module)
	{
		RegisteredModules.Remove(Module);
	}
}

void UKawaiiFluidVolumeComponent::RegisterToSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->RegisterVolumeComponent(this);
		}
	}
}

void UKawaiiFluidVolumeComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->UnregisterVolumeComponent(this);
		}
	}
}

void UKawaiiFluidVolumeComponent::DrawBoundsVisualization()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AActor* Owner = GetOwner();
	const FVector ComponentLocation = GetComponentLocation();

	// Draw user-defined volume size (main wireframe)
	const FVector UserExtent = GetVolumeHalfExtent();
	const FColor DrawColor = (Owner && Owner->IsSelected()) ? FColor::Yellow : BoundsColor;

	DrawDebugBox(
		World,
		ComponentLocation,
		UserExtent,
		FQuat::Identity,
		DrawColor,
		false,  // bPersistentLines
		-1.0f,  // LifeTime (negative = one frame)
		0,      // DepthPriority
		BoundsLineThickness
	);

	// Optionally draw internal Z-Order space (advanced debug)
	if (bShowZOrderSpaceWireframe)
	{
		const FVector ZOrderCenter = (WorldBoundsMin + WorldBoundsMax) * 0.5f;
		const FVector ZOrderExtent = (WorldBoundsMax - WorldBoundsMin) * 0.5f;

		DrawDebugBox(
			World,
			ZOrderCenter,
			ZOrderExtent,
			FQuat::Identity,
			ZOrderSpaceWireframeColor,
			false,
			-1.0f,
			0,
			1.0f  // Thinner line for internal grid
		);
	}

	// Draw info text at center
#if WITH_EDITOR
	if (!World->IsGameWorld())
	{
		const FVector EffectiveSize = GetEffectiveVolumeSize();
		const FString InfoText = FString::Printf(
			TEXT("Size: %.0fx%.0fx%.0f cm\nBounce: %.1f, Friction: %.1f"),
			EffectiveSize.X, EffectiveSize.Y, EffectiveSize.Z,
			WallBounce, WallFriction
		);
		DrawDebugString(World, ComponentLocation + FVector(0, 0, UserExtent.Z + 50.0f), InfoText, nullptr, DrawColor, -1.0f, true);
	}
#endif
}

//========================================
// Preset & Simulation
//========================================

void UKawaiiFluidVolumeComponent::SetFluidType(EFluidType InFluidType)
{
	FluidType = InFluidType;

	// Forward to all registered modules
	for (TWeakObjectPtr<UKawaiiFluidSimulationModule>& WeakModule : RegisteredModules)
	{
		if (UKawaiiFluidSimulationModule* Module = WeakModule.Get())
		{
			Module->SetFluidType(InFluidType);
		}
	}
}

float UKawaiiFluidVolumeComponent::GetParticleSpacing() const
{
	if (Preset)
	{
		return Preset->ParticleRadius * 2.0f;
	}
	return 10.0f;  // Default fallback
}

//========================================
// Debug Methods
//========================================

void UKawaiiFluidVolumeComponent::SetDebugVisualization(EFluidDebugVisualization Mode)
{
	DebugDrawMode = Mode;
	bEnableDebugDraw = (Mode != EFluidDebugVisualization::None);
}

void UKawaiiFluidVolumeComponent::EnableDebugDraw(EFluidDebugVisualization Mode, float PointSize)
{
	bEnableDebugDraw = true;
	DebugDrawMode = Mode;
	DebugPointSize = PointSize;
}

void UKawaiiFluidVolumeComponent::DisableDebugDraw()
{
	bEnableDebugDraw = false;
}
