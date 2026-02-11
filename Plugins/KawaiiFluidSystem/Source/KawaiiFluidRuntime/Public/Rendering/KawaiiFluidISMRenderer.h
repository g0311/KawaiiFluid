// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "KawaiiFluidISMRenderer.generated.h"

class IKawaiiFluidDataProvider;

/**
 * @class UKawaiiFluidISMRenderer
 * @brief Renderer that represents fluid particles as instances of a static mesh for high performance.
 * 
 * Each particle is rendered as a mesh instance using GPU instancing, supporting velocity-based 
 * colors and rotations.
 * 
 * @param bEnabled Toggle for the ISM debug renderer.
 * @param CullDistance Distance at which instances are no longer rendered (cm).
 * @param bCastShadow Whether the particle instances should cast shadows.
 * @param bRotateByVelocity Orient instances toward their velocity vector.
 * @param bColorByVelocity Apply a color gradient based on particle speed.
 * @param MinVelocityColor Color used for stationary or slow particles.
 * @param MaxVelocityColor Color used for particles at maximum speed.
 * @param MaxVelocityForColor Normalization value for the velocity-to-color mapping.
 * @param ISMComponent The internal component managing the static mesh instances.
 * @param CachedWorld Cached pointer to the world context.
 * @param CachedOwnerComponent Component to which the ISM is attached.
 * @param CachedPreset The data asset containing fluid physical properties.
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidISMRenderer : public UObject
{
	GENERATED_BODY()

public:
	UKawaiiFluidISMRenderer();

	void Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, class UKawaiiFluidPresetDataAsset* InPreset);

	void Cleanup();

	void UpdateRendering(const IKawaiiFluidDataProvider* DataProvider, float DeltaTime);

	bool IsEnabled() const { return bEnabled; }

	void SetEnabled(bool bInEnabled);

	void SetFluidColor(FLinearColor Color);

	//========================================
	// Debug Visualization Settings
	//========================================

	bool bEnabled = false;

	//========================================
	// Performance Options
	//========================================

	float CullDistance = 10000.0f;

	bool bCastShadow = false;

	//========================================
	// Visual Effects
	//========================================

	bool bRotateByVelocity = false;

	bool bColorByVelocity = false;

	FLinearColor MinVelocityColor = FLinearColor::Blue;

	FLinearColor MaxVelocityColor = FLinearColor::Red;

	float MaxVelocityForColor = 1000.0f;

	//========================================
	// Component Access
	//========================================

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ISMComponent;

protected:
	//========================================
	// Common State
	//========================================

	UPROPERTY()
	TObjectPtr<UWorld> CachedWorld;

	UPROPERTY()
	TObjectPtr<USceneComponent> CachedOwnerComponent;

	UPROPERTY()
	TObjectPtr<class UKawaiiFluidPresetDataAsset> CachedPreset;

	//========================================
	// ISM-specific Internals
	//========================================

	void InitializeISM();

	UStaticMesh* GetDefaultParticleMesh();

	UMaterialInterface* GetDefaultParticleMaterial();
};
