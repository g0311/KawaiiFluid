// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/IKawaiiFluidDataProvider.h"
#include "Core/KawaiiFluidRenderParticle.h"
#include "KawaiiFluidRenderingModule.generated.h"

class UKawaiiFluidProxyRenderer;
class UKawaiiFluidRenderer;
class UKawaiiFluidPresetDataAsset;

/**
 * @class UKawaiiFluidRenderingModule
 * @brief Orchestrates fluid rendering by managing internal renderer instances and simulation data distribution.
 * 
 * Receives simulation data from an IKawaiiFluidDataProvider and distributes it to enabled renderers (ISM, Metaball).
 * 
 * @param CachedWorld Cached world reference for rendering contexts.
 * @param CachedOwnerComponent Cached scene component used for renderer attachment and transform tracking.
 * @param DataProviderPtr Pointer to the simulation data source.
 * @param ISMRenderer Instance of the Instanced Static Mesh renderer for particle visualization.
 * @param MetaballRenderer Instance of the Metaball/SSFR renderer for fluid surface visualization.
 * @param RenderParticlesCache Temporary buffer used for particle data conversion and caching.
 */
UCLASS()
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidRenderingModule : public UObject
{
	GENERATED_BODY()

public:
	UKawaiiFluidRenderingModule();

	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	void Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, IKawaiiFluidDataProvider* InDataProvider, UKawaiiFluidPresetDataAsset* InPreset);

	void Cleanup();

	void UpdateRenderers();

	UFUNCTION(BlueprintPure, Category = "Rendering")
	int32 GetParticleCount() const;

	UFUNCTION(BlueprintPure, Category = "Rendering")
	bool IsInitialized() const { return CachedWorld != nullptr && DataProviderPtr != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Rendering")
	UKawaiiFluidProxyRenderer* GetISMRenderer() const { return ISMRenderer; }

	UFUNCTION(BlueprintPure, Category = "Rendering")
	UKawaiiFluidRenderer* GetMetaballRenderer() const { return MetaballRenderer; }

	UKawaiiFluidRenderer* GetSSFRRenderer() const { return MetaballRenderer; }

protected:
	UPROPERTY()
	TObjectPtr<UWorld> CachedWorld;

	UPROPERTY()
	TObjectPtr<USceneComponent> CachedOwnerComponent;

	IKawaiiFluidDataProvider* DataProviderPtr;

	UPROPERTY()
	TObjectPtr<UKawaiiFluidProxyRenderer> ISMRenderer;

	UPROPERTY()
	TObjectPtr<UKawaiiFluidRenderer> MetaballRenderer;

	TArray<FKawaiiFluidRenderParticle> RenderParticlesCache;
};