// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Components/KawaiiFluidSimulationVolumeComponent.h"
#include "Core/KawaiiFluidSimulatorSubsystem.h"
#include "Modules/KawaiiFluidSimulationModule.h"
#include "GPU/GPUFluidSimulatorShaders.h"
#include "DrawDebugHelpers.h"

UKawaiiFluidSimulationVolumeComponent::UKawaiiFluidSimulationVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Enable ticking in editor for debug visualization
	bTickInEditor = true;

	// Calculate initial bounds (uses GridResolutionPreset default = Medium)
	RecalculateBounds();
}

void UKawaiiFluidSimulationVolumeComponent::OnRegister()
{
	Super::OnRegister();
	RecalculateBounds();
}

void UKawaiiFluidSimulationVolumeComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

void UKawaiiFluidSimulationVolumeComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterToSubsystem();
	RecalculateBounds();
}

void UKawaiiFluidSimulationVolumeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void UKawaiiFluidSimulationVolumeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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
void UKawaiiFluidSimulationVolumeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ?
		PropertyChangedEvent.Property->GetFName() : NAME_None;

	// Handle both CellSize and GridResolutionPreset changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationVolumeComponent, CellSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UKawaiiFluidSimulationVolumeComponent, GridResolutionPreset))
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

void UKawaiiFluidSimulationVolumeComponent::RecalculateBounds()
{
	// Ensure valid CellSize
	CellSize = FMath::Max(CellSize, 1.0f);

	// Update grid parameters from preset
	GridAxisBits = GridResolutionPresetHelper::GetAxisBits(GridResolutionPreset);
	GridResolution = GridResolutionPresetHelper::GetGridResolution(GridResolutionPreset);
	MaxCells = GridResolutionPresetHelper::GetMaxCells(GridResolutionPreset);

	// Calculate bounds extent from grid resolution and cell size
	BoundsExtent = static_cast<float>(GridResolution) * CellSize;

	// Get component world location
	const FVector ComponentLocation = GetComponentLocation();

	// Calculate world bounds (centered on component)
	const float HalfExtent = BoundsExtent * 0.5f;
	WorldBoundsMin = ComponentLocation - FVector(HalfExtent, HalfExtent, HalfExtent);
	WorldBoundsMax = ComponentLocation + FVector(HalfExtent, HalfExtent, HalfExtent);
}

bool UKawaiiFluidSimulationVolumeComponent::IsPositionInBounds(const FVector& WorldPosition) const
{
	return WorldPosition.X >= WorldBoundsMin.X && WorldPosition.X <= WorldBoundsMax.X &&
	       WorldPosition.Y >= WorldBoundsMin.Y && WorldPosition.Y <= WorldBoundsMax.Y &&
	       WorldPosition.Z >= WorldBoundsMin.Z && WorldPosition.Z <= WorldBoundsMax.Z;
}

void UKawaiiFluidSimulationVolumeComponent::GetSimulationBounds(FVector& OutMin, FVector& OutMax) const
{
	OutMin = WorldBoundsMin;
	OutMax = WorldBoundsMax;
}

void UKawaiiFluidSimulationVolumeComponent::RegisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module && !RegisteredModules.Contains(Module))
	{
		RegisteredModules.Add(Module);
	}
}

void UKawaiiFluidSimulationVolumeComponent::UnregisterModule(UKawaiiFluidSimulationModule* Module)
{
	if (Module)
	{
		RegisteredModules.Remove(Module);
	}
}

void UKawaiiFluidSimulationVolumeComponent::RegisterToSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->RegisterVolumeComponent(this);
		}
	}
}

void UKawaiiFluidSimulationVolumeComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UKawaiiFluidSimulatorSubsystem* Subsystem = World->GetSubsystem<UKawaiiFluidSimulatorSubsystem>())
		{
			Subsystem->UnregisterVolumeComponent(this);
		}
	}
}

void UKawaiiFluidSimulationVolumeComponent::DrawBoundsVisualization()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Draw wireframe box
	const FVector Center = (WorldBoundsMin + WorldBoundsMax) * 0.5f;
	const FVector Extent = (WorldBoundsMax - WorldBoundsMin) * 0.5f;

	// Yellow when selected, otherwise use BoundsColor
	AActor* Owner = GetOwner();
	const FColor DrawColor = (Owner && Owner->IsSelected()) ? FColor::Yellow : BoundsColor;

	DrawDebugBox(
		World,
		Center,
		Extent,
		FQuat::Identity,
		DrawColor,
		false,  // bPersistentLines
		-1.0f,  // LifeTime (negative = one frame)
		0,      // DepthPriority
		BoundsLineThickness
	);

	// Draw info text at center
#if WITH_EDITOR
	if (!World->IsGameWorld())
	{
		const FString InfoText = FString::Printf(
			TEXT("Volume: %.1fm x %.1fm x %.1fm\nCellSize: %.1fcm"),
			BoundsExtent / 100.0f, BoundsExtent / 100.0f, BoundsExtent / 100.0f,
			CellSize
		);
		DrawDebugString(World, Center + FVector(0, 0, Extent.Z + 50.0f), InfoText, nullptr, DrawColor, -1.0f, true);
	}
#endif
}
