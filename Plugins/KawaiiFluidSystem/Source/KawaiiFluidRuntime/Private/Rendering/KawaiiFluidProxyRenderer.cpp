// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidProxyRenderer.h"
#include "Core/IKawaiiFluidDataProvider.h"
#include "Core/KawaiiFluidParticle.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Simulation/GPUFluidSimulator.h"
#include "Engine/StaticMesh.h"

UKawaiiFluidProxyRenderer::UKawaiiFluidProxyRenderer()
{
	// No component tick needed - UObject doesn't tick
}

/**
 * @brief Initialize the Proxy renderer with world context, owner, and physical preset.
 * 
 * @param InWorld World context for accessing subsystems.
 * @param InOwnerComponent Parent component for attaching the internal instances.
 * @param InPreset Physical property asset.
 */
void UKawaiiFluidProxyRenderer::Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, UKawaiiFluidPresetDataAsset* InPreset)
{
	CachedWorld = InWorld;
	CachedOwnerComponent = InOwnerComponent;
	CachedPreset = InPreset;

	if (!CachedWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidProxyRenderer::Initialize - No world context provided"));
	}

	if (!CachedOwnerComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidProxyRenderer::Initialize - No owner component provided"));
	}

	if (!CachedPreset)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidProxyRenderer::Initialize - No preset provided"));
	}

	InitializeISM();

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidProxyRenderer: Initialized (Mesh: %s)"),
		ISMComponent && ISMComponent->GetStaticMesh() ? *ISMComponent->GetStaticMesh()->GetName() : TEXT("None"));
}

void UKawaiiFluidProxyRenderer::Cleanup()
{
	if (ISMComponent)
	{
		ISMComponent->ClearInstances();
		ISMComponent->DestroyComponent(); // Unregister and destroy
		ISMComponent = nullptr;
	}

	// Clear cached references
	CachedWorld = nullptr;
	CachedOwnerComponent = nullptr;
	CachedPreset = nullptr;
	// Note: Do NOT reset bEnabled here - it's controlled by Component's bEnableISMDebugView
}

void UKawaiiFluidProxyRenderer::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	// Clear instances when disabled and force render state update
	if (!bEnabled && ISMComponent)
	{
		ISMComponent->ClearInstances();
		ISMComponent->MarkRenderStateDirty();
	}
}

/**
 * @brief Synchronizes Proxy instance transforms with current particle data from the provider.
 * 
 * Handles both CPU and GPU simulation modes by reading back positions and velocities.
 * 
 * @param DataProvider Source of particle simulation data.
 * @param DeltaTime Current frame's time step.
 */
void UKawaiiFluidProxyRenderer::UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime)
{
	static int32 UpdateLogCounter = 0;
	const bool bShouldLog = (UpdateLogCounter++ % 120 == 0);

	if (!bEnabled)
	{
		return;
	}

	if (!ISMComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ProxyRenderer::UpdateRendering - ISMComponent is NULL!"));
		return;
	}

	if (!DataProvider)
	{
		UE_LOG(LogTemp, Error, TEXT("ProxyRenderer::UpdateRendering - DataProvider is NULL!"));
		return;
	}

	// Get simulation data from DataProvider (GPU or CPU)
	TArray<FVector3f> Positions;
	TArray<FVector3f> Velocities;

	if (DataProvider->IsGPUSimulationActive())
	{
		// GPU mode: Use lightweight readback API (Position + Velocity only)
		FGPUFluidSimulator* Simulator = DataProvider->GetGPUSimulator();
		if (Simulator)
		{
			// Enable velocity readback for Proxy rendering
			Simulator->SetFullReadbackEnabled(true);

			if (!Simulator->GetParticlePositionsAndVelocities(Positions, Velocities))
			{
				// Readback not available: clear instances when particle count is confirmed zero
				if (Simulator->GetParticleCount() <= 0 && ISMComponent->GetInstanceCount() > 0)
				{
					ISMComponent->ClearInstances();
				}
				return;
			}
		}
		else
		{
			return;
		}
	}
	else
	{
		// CPU mode: Extract positions and velocities from particle array
		const TArray<FKawaiiFluidParticle>& CPUParticles = DataProvider->GetParticles();
		const int32 Count = CPUParticles.Num();
		Positions.SetNumUninitialized(Count);
		Velocities.SetNumUninitialized(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Positions[i] = FVector3f(CPUParticles[i].Position);
			Velocities[i] = FVector3f(CPUParticles[i].Velocity);
		}
	}

	if (Positions.Num() == 0)
	{
		ISMComponent->ClearInstances();
		return;
	}

	if (bShouldLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== Proxy Debug: Particles=%d, Registered=%d, Visible=%d, Mesh=%s, Material=%s, InstanceCount=%d ==="),
			Positions.Num(),
			ISMComponent->IsRegistered() ? 1 : 0,
			ISMComponent->IsVisible() ? 1 : 0,
			ISMComponent->GetStaticMesh() ? TEXT("OK") : TEXT("NULL"),
			ISMComponent->GetMaterial(0) ? TEXT("OK") : TEXT("NULL"),
			ISMComponent->GetInstanceCount());
	}

	// Use all particles without limit for the debug/proxy view
	const int32 NumInstances = Positions.Num();

	// Clear existing instances and preallocate memory
	ISMComponent->ClearInstances();
	ISMComponent->PreAllocateInstancesMemory(NumInstances);

	// Get ParticleRadius from Preset (simulation radius for accurate debug visualization)
	float ParticleRadius = 5.0f; // Default fallback
	if (CachedPreset)
	{
		ParticleRadius = CachedPreset->ParticleRadius;
	}

	// Scale factor based on ParticleRadius (Default Sphere has 50cm radius)
	float ScaleFactor = ParticleRadius / 50.0f;
	FVector ScaleVec(ScaleFactor, ScaleFactor, ScaleFactor);

	// Check if velocities available
	const bool bHasVelocities = Velocities.Num() == Positions.Num();

	// Add each particle as an instance
	int32 InstanceIndex = 0;
	for (int32 i = 0; i < NumInstances; ++i)
	{
		const FVector3f& Position = Positions[i];

		// Skip NaN/Inf positions (can occur from stale readback after despawn compaction)
		if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z))
		{
			continue;
		}

		const FVector3f Velocity = bHasVelocities ? Velocities[i] : FVector3f::ZeroVector;

		// Create transform
		FTransform InstanceTransform;
		InstanceTransform.SetLocation(FVector(Position));
		InstanceTransform.SetScale3D(ScaleVec);

		// Velocity-based rotation (optional)
		if (bRotateByVelocity && bHasVelocities && !Velocity.IsNearlyZero())
		{
			FRotator Rotation = FVector(Velocity).ToOrientationRotator();
			InstanceTransform.SetRotation(Rotation.Quaternion());
		}

		// Add instance to the ISM component
		ISMComponent->AddInstance(InstanceTransform, false);

		// Velocity-based color (optional)
		if (bColorByVelocity && bHasVelocities)
		{
			float VelocityMagnitude = Velocity.Size();
			float T = FMath::Clamp(VelocityMagnitude / MaxVelocityForColor, 0.0f, 1.0f);
			FLinearColor Color = FMath::Lerp(MinVelocityColor, MaxVelocityColor, T);

			// Pass color as custom data (available in material)
			ISMComponent->SetCustomDataValue(InstanceIndex, 0, Color.R, false);
			ISMComponent->SetCustomDataValue(InstanceIndex, 1, Color.G, false);
			ISMComponent->SetCustomDataValue(InstanceIndex, 2, Color.B, false);
			ISMComponent->SetCustomDataValue(InstanceIndex, 3, Color.A, false);
		}

		++InstanceIndex;
	}

	// Update bounds and render state - essential for Virtual Shadow Maps (VSM) and Cascaded Shadows
	ISMComponent->UpdateBounds();
	ISMComponent->MarkRenderStateDirty();
}

void UKawaiiFluidProxyRenderer::InitializeISM()
{
	if (!CachedOwnerComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidProxyRenderer: No owner component"));
		return;
	}

	// Create internal ISM component on the owner component
	ISMComponent = NewObject<UInstancedStaticMeshComponent>(
		CachedOwnerComponent,
		UInstancedStaticMeshComponent::StaticClass(),
		TEXT("FluidProxyISM_Internal")
	);

	if (!ISMComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidProxyRenderer: Failed to create ISM component"));
		return;
	}

	// Component setup - Attach to FluidComponent for stable hierarchy
	ISMComponent->SetupAttachment(CachedOwnerComponent);

	// Use absolute coordinates for simpler world-space mapping
	ISMComponent->SetAbsolute(true, true, true);

	// Mesh setup - Default sphere for particle visualization
	UStaticMesh* DefaultMesh = GetDefaultParticleMesh();
	if (DefaultMesh)
	{
		ISMComponent->SetStaticMesh(DefaultMesh);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidProxyRenderer: Failed to load default sphere mesh"));
		return;
	}

	// Material setup
	UMaterialInterface* DefaultMaterial = GetDefaultParticleMaterial();
	if (DefaultMaterial)
	{
		ISMComponent->SetMaterial(0, DefaultMaterial);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidProxyRenderer: Failed to load default material"));
	}

	// Performance and visibility properties
	ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMComponent->SetCastShadow(bCastShadow);
	ISMComponent->bCastShadowAsTwoSided = true; // Improve shadow stability for small particles
	ISMComponent->SetCullDistances(0, CullDistance);
	ISMComponent->SetVisibility(true);
	ISMComponent->SetHiddenInGame(false);

	// Register component with the world
	ISMComponent->RegisterComponent();

	if (!ISMComponent->IsRegistered())
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidProxyRenderer: RegisterComponent() failed!"));
		return;
	}

	// Custom data setup (e.g., for color variation based on velocity)
	if (bColorByVelocity)
	{
		ISMComponent->NumCustomDataFloats = 4; // RGBA
	}

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidProxyRenderer: Internal ISM component initialized"));
}

UStaticMesh* UKawaiiFluidProxyRenderer::GetDefaultParticleMesh()
{
	// Load engine default sphere mesh
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere")
	);

	if (!SphereMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMRenderer: Failed to load default sphere mesh"));
	}

	return SphereMesh;
}

UMaterialInterface* UKawaiiFluidProxyRenderer::GetDefaultParticleMaterial()
{
	// Load engine default material
	UMaterialInterface* DefaultMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
	);

	if (!DefaultMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMRenderer: Failed to load default material"));
	}

	return DefaultMaterial;
}

void UKawaiiFluidProxyRenderer::SetFluidColor(FLinearColor Color)
{
	if (!ISMComponent)
	{
		return;
	}

	// Get or create dynamic material instance
	UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(ISMComponent->GetMaterial(0));

	if (!DynMaterial)
	{
		// Create dynamic material from current material
		UMaterialInterface* BaseMaterial = ISMComponent->GetMaterial(0);
		if (!BaseMaterial)
		{
			BaseMaterial = GetDefaultParticleMaterial();
		}

		if (BaseMaterial)
		{
			DynMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, ISMComponent);
			ISMComponent->SetMaterial(0, DynMaterial);
		}
	}

	if (DynMaterial)
	{
		// BasicShapeMaterial uses "Color" parameter
		DynMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	// Also update velocity colors for consistency
	MinVelocityColor = Color;
	MaxVelocityColor = Color;
}
