// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidISMRenderer.h"
#include "Interfaces/IKawaiiFluidDataProvider.h"
#include "Core/FluidParticle.h"
#include "Data/KawaiiFluidPresetDataAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GPU/GPUFluidSimulator.h"
#include "Engine/StaticMesh.h"

UKawaiiFluidISMRenderer::UKawaiiFluidISMRenderer()
{
	// No component tick needed - UObject doesn't tick
}

void UKawaiiFluidISMRenderer::Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, UKawaiiFluidPresetDataAsset* InPreset)
{
	CachedWorld = InWorld;
	CachedOwnerComponent = InOwnerComponent;
	CachedPreset = InPreset;

	if (!CachedWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMRenderer::Initialize - No world context provided"));
	}

	if (!CachedOwnerComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMRenderer::Initialize - No owner component provided"));
	}

	if (!CachedPreset)
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMRenderer::Initialize - No preset provided"));
	}

	InitializeISM();

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidISMRenderer: Initialized (Mesh: %s)"),
		ISMComponent && ISMComponent->GetStaticMesh() ? *ISMComponent->GetStaticMesh()->GetName() : TEXT("None"));
}

void UKawaiiFluidISMRenderer::Cleanup()
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

void UKawaiiFluidISMRenderer::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	// Clear instances when disabled and force render state update
	if (!bEnabled && ISMComponent)
	{
		ISMComponent->ClearInstances();
		ISMComponent->MarkRenderStateDirty();
	}
}

void UKawaiiFluidISMRenderer::UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime)
{
	static int32 UpdateLogCounter = 0;
	const bool bShouldLog = (UpdateLogCounter++ % 120 == 0);

	if (!bEnabled)
	{
		return;
	}

	if (!ISMComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ISMRenderer::UpdateRendering - ISMComponent is NULL!"));
		return;
	}

	if (!DataProvider)
	{
		UE_LOG(LogTemp, Error, TEXT("ISMRenderer::UpdateRendering - DataProvider is NULL!"));
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
			// Enable velocity readback for ISM rendering
			Simulator->SetFullReadbackEnabled(true);

			if (!Simulator->GetParticlePositionsAndVelocities(Positions, Velocities))
			{
				// No readback data available
				if (bShouldLog)
				{
					UE_LOG(LogTemp, Warning, TEXT("ISMRenderer: GPU readback not available"));
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
		const TArray<FFluidParticle>& CPUParticles = DataProvider->GetParticles();
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
		UE_LOG(LogTemp, Warning, TEXT("=== ISM Debug: Particles=%d, Registered=%d, Visible=%d, Mesh=%s, Material=%s, InstanceCount=%d ==="),
			Positions.Num(),
			ISMComponent->IsRegistered() ? 1 : 0,
			ISMComponent->IsVisible() ? 1 : 0,
			ISMComponent->GetStaticMesh() ? TEXT("OK") : TEXT("NULL"),
			ISMComponent->GetMaterial(0) ? TEXT("OK") : TEXT("NULL"),
			ISMComponent->GetInstanceCount());
	}

	// Use all particles without limit
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

	// Add each particle as instance
	for (int32 i = 0; i < NumInstances; ++i)
	{
		const FVector3f& Position = Positions[i];
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

		// Add instance (use local space - SetAbsolute makes local=world)
		ISMComponent->AddInstance(InstanceTransform, false);

		// Velocity-based color (optional)
		if (bColorByVelocity && bHasVelocities)
		{
			float VelocityMagnitude = Velocity.Size();
			float T = FMath::Clamp(VelocityMagnitude / MaxVelocityForColor, 0.0f, 1.0f);
			FLinearColor Color = FMath::Lerp(MinVelocityColor, MaxVelocityColor, T);

			// Pass color as custom data (available in material)
			ISMComponent->SetCustomDataValue(i, 0, Color.R, false);
			ISMComponent->SetCustomDataValue(i, 1, Color.G, false);
			ISMComponent->SetCustomDataValue(i, 2, Color.B, false);
			ISMComponent->SetCustomDataValue(i, 3, Color.A, false);
		}
	}

	// Update render state - single call is sufficient in UE5
	ISMComponent->MarkRenderStateDirty();
}

void UKawaiiFluidISMRenderer::InitializeISM()
{
	if (!CachedOwnerComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidISMRenderer: No owner component"));
		return;
	}

	// Create ISM component on owner component
	ISMComponent = NewObject<UInstancedStaticMeshComponent>(
		CachedOwnerComponent,
		UInstancedStaticMeshComponent::StaticClass(),
		TEXT("FluidISM_Internal")
	);

	if (!ISMComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidISMRenderer: Failed to create ISM component"));
		return;
	}

	// Component setup - Attach directly to FluidComponent (stable hierarchy)
	ISMComponent->SetupAttachment(CachedOwnerComponent);

	// Use absolute coordinates (same as DummyComponent)
	ISMComponent->SetAbsolute(true, true, true);

	// Mesh setup - MUST be done before RegisterComponent()
	UStaticMesh* DefaultMesh = GetDefaultParticleMesh();
	if (DefaultMesh)
	{
		ISMComponent->SetStaticMesh(DefaultMesh);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidISMRenderer: Failed to load default sphere mesh"));
		return;
	}

	// Material setup - MUST be done before RegisterComponent()
	UMaterialInterface* DefaultMaterial = GetDefaultParticleMaterial();
	if (DefaultMaterial)
	{
		ISMComponent->SetMaterial(0, DefaultMaterial);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluidISMRenderer: Failed to load default material"));
	}

	// Set properties before registration
	ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMComponent->SetCastShadow(bCastShadow);
	ISMComponent->SetCullDistances(0, CullDistance);
	ISMComponent->SetVisibility(true);
	ISMComponent->SetHiddenInGame(false);

	// Register component AFTER all setup is done
	ISMComponent->RegisterComponent();

	if (!ISMComponent->IsRegistered())
	{
		UE_LOG(LogTemp, Error, TEXT("KawaiiFluidISMRenderer: RegisterComponent() failed!"));
		return;
	}

	// Custom data setup (for color variation)
	if (bColorByVelocity)
	{
		ISMComponent->NumCustomDataFloats = 4; // RGBA
	}

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluidISMRenderer: ISM component initialized"));
}

UStaticMesh* UKawaiiFluidISMRenderer::GetDefaultParticleMesh()
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

UMaterialInterface* UKawaiiFluidISMRenderer::GetDefaultParticleMaterial()
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

void UKawaiiFluidISMRenderer::SetFluidColor(FLinearColor Color)
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
