// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class UKawaiiFluidRendererSubsystem;
struct FPostProcessingInputs;

/**
 * @class FKawaiiFluidSceneViewExtension
 * @brief Scene View Extension for injecting the SSFR rendering pipeline into the Unreal renderer.
 * 
 * Handles GPU data extraction and executes fluid rendering passes at the PrePostProcess stage.
 * 
 * @param Subsystem Weak reference to the parent fluid renderer subsystem.
 */
class KAWAIIFLUIDRUNTIME_API FKawaiiFluidSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FKawaiiFluidSceneViewExtension(const FAutoRegister& AutoRegister, UKawaiiFluidRendererSubsystem* InSubsystem);
	virtual ~FKawaiiFluidSceneViewExtension() override;

	// ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	virtual void SubscribeToPostProcessingPass(
		EPostProcessingPass Pass,
		const FSceneView& InView,
		FPostProcessingPassDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;

	virtual void PrePostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessingInputs& Inputs) override;

	// End of ISceneViewExtension interface

private:
	bool IsViewFromOurWorld(const FSceneView& InView) const;

	TWeakObjectPtr<UKawaiiFluidRendererSubsystem> Subsystem;
};
