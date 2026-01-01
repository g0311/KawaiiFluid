// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidSceneViewExtension.h"

#include "FluidDepthPass.h"
#include "FluidNormalPass.h"
#include "FluidRendererSubsystem.h"
#include "FluidSmoothingPass.h"
#include "FluidThicknessPass.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "SceneRendering.h"
#include "SceneTextures.h"
#include "SceneTexturesConfig.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "Modules/KawaiiFluidRenderingModule.h"
#include "Rendering/KawaiiFluidSSFRRenderer.h"
#include "Rendering/KawaiiFluidRenderResource.h"
#include "Rendering/Composite/IFluidCompositePass.h"
#include "Rendering/Composite/FluidRayMarchComposite.h"
#include "Rendering/Composite/FluidGBufferComposite.h"
#include "Rendering/Composite/FluidTransparencyComposite.h"
#include "Rendering/SDFVolumeManager.h"

static TRefCountPtr<IPooledRenderTarget> GFluidCompositeDebug_KeepAlive;

// Transparency composite for G-Buffer + Transparency hybrid mode
static FFluidTransparencyComposite GFluidTransparencyComposite;

// ==============================================================================
// Class Implementation
// ==============================================================================

FFluidSceneViewExtension::FFluidSceneViewExtension(const FAutoRegister& AutoRegister,
                                                   UFluidRendererSubsystem* InSubsystem)
	: FSceneViewExtensionBase(AutoRegister), Subsystem(InSubsystem)
{
}

FFluidSceneViewExtension::~FFluidSceneViewExtension()
{
}

bool FFluidSceneViewExtension::IsViewFromOurWorld(const FSceneView& InView) const
{
	UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();
	if (!SubsystemPtr)
	{
		return false;
	}

	UWorld* OurWorld = SubsystemPtr->GetWorld();
	if (!OurWorld)
	{
		return false;
	}

	// Get World from View's Family Scene
	if (InView.Family && InView.Family->Scene)
	{
		UWorld* ViewWorld = InView.Family->Scene->GetWorld();
		return ViewWorld == OurWorld;
	}

	return false;
}

void FFluidSceneViewExtension::PostRenderBasePassDeferred_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneView& InView,
	const FRenderTargetBindingSlots& RenderTargets,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	// Only render for views from our World
	if (!IsViewFromOurWorld(InView))
	{
		return;
	}

	UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();
	if (!SubsystemPtr || !SubsystemPtr->RenderingParameters.bEnableRendering)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "KawaiiFluidGBuffer_PostBasePass");

	// Collect GBuffer mode renderers only
	TMap<FFluidRenderingParameters, TArray<UKawaiiFluidSSFRRenderer*>> GBufferBatches;

	const TArray<UKawaiiFluidRenderingModule*>& Modules = SubsystemPtr->GetAllRenderingModules();
	for (UKawaiiFluidRenderingModule* Module : Modules)
	{
		if (!Module) continue;

		UKawaiiFluidSSFRRenderer* SSFRRenderer = Module->GetSSFRRenderer();
		if (SSFRRenderer && SSFRRenderer->IsRenderingActive())
		{
			const FFluidRenderingParameters& Params = SSFRRenderer->GetLocalParameters();
			if (Params.SSFRMode == ESSFRRenderingMode::GBuffer)
			{
				GBufferBatches.FindOrAdd(Params).Add(SSFRRenderer);
			}
		}
	}

	if (GBufferBatches.Num() == 0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: PostRenderBasePassDeferred - Processing %d GBuffer batches"), GBufferBatches.Num());

	// Get SceneDepth from RenderTargets
	FRDGTextureRef SceneDepthTexture = RenderTargets.DepthStencil.GetTexture();
	UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: PostBasePass SceneDepthTexture = %p"), SceneDepthTexture);

	// SDF Volume Manager for baking
	static FSDFVolumeManager GBufferSDFVolumeManager;

	// Process GBuffer batches using Ray Marching SDF
	for (auto& Batch : GBufferBatches)
	{
		const FFluidRenderingParameters& BatchParams = Batch.Key;
		const TArray<UKawaiiFluidSSFRRenderer*>& Renderers = Batch.Value;

		RDG_EVENT_SCOPE(GraphBuilder, "FluidGBufferRayMarchBatch");

		// Collect all particle positions from batch
		TArray<FVector3f> AllParticlePositions;
		float AverageParticleRadius = 10.0f;
		float TotalRadius = 0.0f;
		int32 ValidCount = 0;

		for (UKawaiiFluidSSFRRenderer* Renderer : Renderers)
		{
			FKawaiiFluidRenderResource* RenderResource = Renderer->GetFluidRenderResource();
			if (RenderResource && RenderResource->IsValid())
			{
				const TArray<FKawaiiRenderParticle>& CachedParticles = RenderResource->GetCachedParticles();
				for (const FKawaiiRenderParticle& Particle : CachedParticles)
				{
					AllParticlePositions.Add(Particle.Position);
				}
			}
			TotalRadius += Renderer->GetCachedParticleRadius();
			ValidCount++;
		}

		if (ValidCount > 0)
		{
			AverageParticleRadius = TotalRadius / ValidCount;
		}

		if (AllParticlePositions.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("KawaiiFluid: No particles for GBuffer Ray Marching - skipping"));
			continue;
		}

		// Create RDG buffer for particle positions
		const uint32 BufferSize = AllParticlePositions.Num() * sizeof(FVector3f);
		FRDGBufferRef ParticleBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), AllParticlePositions.Num()),
			TEXT("GBufferRayMarchParticlePositions"));

		GraphBuilder.QueueBufferUpload(
			ParticleBuffer,
			AllParticlePositions.GetData(),
			BufferSize,
			ERDGInitialDataFlags::None);

		FRDGBufferSRVRef ParticleBufferSRV = GraphBuilder.CreateSRV(ParticleBuffer);

		// SDF Volume Baking (if optimization enabled)
		FRDGTextureSRVRef SDFVolumeSRV = nullptr;
		FVector3f VolumeMin = FVector3f::ZeroVector;
		FVector3f VolumeMax = FVector3f::ZeroVector;
		FIntVector VolumeResolution = FIntVector(64, 64, 64);

		const bool bUseSDFVolume = BatchParams.bUseSDFVolumeOptimization;

		if (bUseSDFVolume)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "SDFVolumeBake_GBuffer");

			int32 Resolution = FMath::Clamp(BatchParams.SDFVolumeResolution, 32, 256);
			GBufferSDFVolumeManager.SetVolumeResolution(FIntVector(Resolution, Resolution, Resolution));

			float Margin = AverageParticleRadius * 2.0f;
			CalculateParticleBoundingBox(AllParticlePositions, AverageParticleRadius, Margin, VolumeMin, VolumeMax);

			SDFVolumeSRV = GBufferSDFVolumeManager.BakeSDFVolume(
				GraphBuilder,
				ParticleBufferSRV,
				AllParticlePositions.Num(),
				AverageParticleRadius,
				BatchParams.SDFSmoothness,
				VolumeMin,
				VolumeMax);

			VolumeResolution = GBufferSDFVolumeManager.GetVolumeResolution();

			UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: GBuffer SDF Volume baked - Min:(%.1f,%.1f,%.1f) Max:(%.1f,%.1f,%.1f) Res:%d"),
				VolumeMin.X, VolumeMin.Y, VolumeMin.Z,
				VolumeMax.X, VolumeMax.Y, VolumeMax.Z,
				Resolution);
		}

		// Get GBuffer textures
		const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);
		const FSceneTextures& SceneTexturesRef = ViewInfo.GetSceneTextures();

		FFluidIntermediateTextures GBufferTextures;
		GBufferTextures.GBufferATexture = SceneTexturesRef.GBufferA;
		GBufferTextures.GBufferBTexture = SceneTexturesRef.GBufferB;
		GBufferTextures.GBufferCTexture = SceneTexturesRef.GBufferC;
		GBufferTextures.GBufferDTexture = SceneTexturesRef.GBufferD;

		// DEBUG: Compare GBuffer texture size vs ViewRect
		FIntVector2 GBufferASize = GBufferTextures.GBufferATexture ?
			FIntVector2(GBufferTextures.GBufferATexture->Desc.Extent.X, GBufferTextures.GBufferATexture->Desc.Extent.Y) :
			FIntVector2(0, 0);
		UE_LOG(LogTemp, Warning, TEXT("KawaiiFluid: PostBasePass - ViewInfo.ViewRect=(%d,%d)-(%d,%d) [%dx%d], GBufferA TexSize=(%d,%d)"),
			ViewInfo.ViewRect.Min.X, ViewInfo.ViewRect.Min.Y, ViewInfo.ViewRect.Max.X, ViewInfo.ViewRect.Max.Y,
			ViewInfo.ViewRect.Width(), ViewInfo.ViewRect.Height(),
			GBufferASize.X, GBufferASize.Y);

		// Check if ViewRect exceeds GBuffer texture size
		if (ViewInfo.ViewRect.Width() > GBufferASize.X || ViewInfo.ViewRect.Height() > GBufferASize.Y)
		{
			UE_LOG(LogTemp, Error, TEXT("KawaiiFluid: WARNING! ViewRect exceeds GBuffer texture size! This will cause rendering issues."));
		}

		UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: GBuffer textures - A:%s B:%s C:%s D:%s"),
			GBufferTextures.GBufferATexture ? TEXT("OK") : TEXT("NULL"),
			GBufferTextures.GBufferBTexture ? TEXT("OK") : TEXT("NULL"),
			GBufferTextures.GBufferCTexture ? TEXT("OK") : TEXT("NULL"),
			GBufferTextures.GBufferDTexture ? TEXT("OK") : TEXT("NULL"));

		// Get composite pass and call RenderRayMarchToGBuffer
		if (Renderers.Num() > 0 && Renderers[0]->GetCompositePass())
		{
			FFluidGBufferComposite* GBufferComposite =
				static_cast<FFluidGBufferComposite*>(Renderers[0]->GetCompositePass().Get());

			GBufferComposite->RenderRayMarchToGBuffer(
				GraphBuilder,
				InView,
				BatchParams,
				ParticleBufferSRV,
				AllParticlePositions.Num(),
				AverageParticleRadius,
				SDFVolumeSRV,
				VolumeMin,
				VolumeMax,
				VolumeResolution,
				SceneDepthTexture,
				GBufferTextures);

			UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: GBuffer Ray Marching complete (Particles: %d, UseSDFVolume: %s)"),
				AllParticlePositions.Num(), bUseSDFVolume ? TEXT("true") : TEXT("false"));
		}
	}
}

void FFluidSceneViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass,
	const FSceneView& InView,
	FPostProcessingPassDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	// Custom mode: Tonemap pass (post-lighting)
	if (Pass == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateLambda(
			[this](FRDGBuilder& GraphBuilder, const FSceneView& View,
			       const FPostProcessMaterialInputs& InInputs)
			{
				// Only render for views from our World
				if (!IsViewFromOurWorld(View))
				{
					return InInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
				}

				UFluidRendererSubsystem* SubsystemPtr = Subsystem.Get();

				// 유효성 검사
				bool bHasAnyModules = SubsystemPtr && SubsystemPtr->GetAllRenderingModules().Num() > 0;

				if (!SubsystemPtr || !SubsystemPtr->RenderingParameters.bEnableRendering || !bHasAnyModules)
				{
					return InInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
				}

				RDG_EVENT_SCOPE(GraphBuilder, "KawaiiFluidRendering");

				// ============================================
				// Batch renderers by LocalParameters
				// Separate batches by rendering mode
				// ============================================
				TMap<FFluidRenderingParameters, TArray<UKawaiiFluidSSFRRenderer*>> CustomBatches;
				TMap<FFluidRenderingParameters, TArray<UKawaiiFluidSSFRRenderer*>> GBufferBatches;
				TMap<FFluidRenderingParameters, TArray<UKawaiiFluidSSFRRenderer*>> RayMarchingBatches;

				const TArray<UKawaiiFluidRenderingModule*>& Modules = SubsystemPtr->GetAllRenderingModules();
				for (UKawaiiFluidRenderingModule* Module : Modules)
				{
					if (!Module) continue;

					UKawaiiFluidSSFRRenderer* SSFRRenderer = Module->GetSSFRRenderer();
					if (SSFRRenderer && SSFRRenderer->IsRenderingActive())
					{
						const FFluidRenderingParameters& Params = SSFRRenderer->GetLocalParameters();

						// Route to appropriate batch based on rendering mode
						if (Params.SSFRMode == ESSFRRenderingMode::Custom)
						{
							CustomBatches.FindOrAdd(Params).Add(SSFRRenderer);
						}
						else if (Params.SSFRMode == ESSFRRenderingMode::GBuffer)
						{
							GBufferBatches.FindOrAdd(Params).Add(SSFRRenderer);
						}
						else if (Params.SSFRMode == ESSFRRenderingMode::RayMarching)
						{
							RayMarchingBatches.FindOrAdd(Params).Add(SSFRRenderer);
						}
					}
				}

				// Check if we have any renderers
				if (CustomBatches.Num() == 0 && GBufferBatches.Num() == 0 && RayMarchingBatches.Num() == 0)
				{
					return InInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
				}

				// Scene Depth 가져오기
				FRDGTextureRef SceneDepthTexture = nullptr;
				if (InInputs.SceneTextures.SceneTextures)
				{
					SceneDepthTexture = InInputs.SceneTextures.SceneTextures->GetContents()->SceneDepthTexture;
				}

				// Composite Setup (공통)
				FScreenPassTexture SceneColorInput = FScreenPassTexture(
					InInputs.GetInput(EPostProcessMaterialInput::SceneColor));
				if (!SceneColorInput.IsValid())
				{
					return InInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
				}

				// Output Target 결정
				FScreenPassRenderTarget Output = InInputs.OverrideOutput;
				if (!Output.IsValid())
				{
					Output = FScreenPassRenderTarget::CreateFromInput(
						GraphBuilder, SceneColorInput, View.GetOverwriteLoadAction(),
						TEXT("FluidCompositeOutput"));
				}

				// SceneColor 복사
				if (SceneColorInput.Texture != Output.Texture)
				{
					AddDrawTexturePass(GraphBuilder, View, SceneColorInput, Output);
				}

				// ============================================
				// Custom Mode Batched Rendering
				// ============================================
				for (auto& Batch : CustomBatches)
				{
					const FFluidRenderingParameters& BatchParams = Batch.Key;
					const TArray<UKawaiiFluidSSFRRenderer*>& Renderers = Batch.Value;

					RDG_EVENT_SCOPE(GraphBuilder, "FluidBatch");

					// Calculate average particle radius for this batch
					float AverageParticleRadius = 10.0f;
					float TotalRadius = 0.0f;
					int ValidCount = 0;

					for (UKawaiiFluidSSFRRenderer* Renderer : Renderers)
					{
						TotalRadius += Renderer->GetCachedParticleRadius();
						ValidCount++;
					}

					if (ValidCount > 0)
					{
						AverageParticleRadius = TotalRadius / ValidCount;
					}

					// Use BatchParams for rendering parameters
					float BlurRadius = static_cast<float>(BatchParams.BilateralFilterRadius);
					float DepthFalloff = AverageParticleRadius * 0.7f;  // Dynamic calculation
					int32 NumIterations = 3;  // Hardcoded

					// Depth Pass (batched - only render particles from this batch)
					FRDGTextureRef BatchDepthTexture = nullptr;
					RenderFluidDepthPass(GraphBuilder, View, Renderers, SceneDepthTexture, BatchDepthTexture);

					if (BatchDepthTexture)
					{
						// Smoothing Pass
						FRDGTextureRef BatchSmoothedDepthTexture = nullptr;
						RenderFluidSmoothingPass(GraphBuilder, View, BatchDepthTexture, BatchSmoothedDepthTexture,
						                         BlurRadius, DepthFalloff, NumIterations);

						if (BatchSmoothedDepthTexture)
						{
							// Normal Pass
							FRDGTextureRef BatchNormalTexture = nullptr;
							RenderFluidNormalPass(GraphBuilder, View, BatchSmoothedDepthTexture, BatchNormalTexture);

							// Thickness Pass
							FRDGTextureRef BatchThicknessTexture = nullptr;
							RenderFluidThicknessPass(GraphBuilder, View, Renderers, SceneDepthTexture, BatchThicknessTexture);

							if (BatchNormalTexture && BatchThicknessTexture)
							{
								// Composite Pass - Use appropriate composite pass implementation
								// Get composite pass from first renderer (all renderers in batch share same params/mode)
								if (Renderers.Num() > 0 && Renderers[0]->GetCompositePass())
								{
									FFluidIntermediateTextures IntermediateTextures;
									IntermediateTextures.SmoothedDepthTexture = BatchSmoothedDepthTexture;
									IntermediateTextures.NormalTexture = BatchNormalTexture;
									IntermediateTextures.ThicknessTexture = BatchThicknessTexture;

									Renderers[0]->GetCompositePass()->RenderComposite(
										GraphBuilder,
										View,
										BatchParams,
										IntermediateTextures,
										SceneDepthTexture,
										SceneColorInput.Texture,
										Output
									);
								}
							}
						}
					}
				}

				// ============================================
				// Ray Marching SDF Mode Rendering
				// ============================================
				UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: RayMarchingBatches count = %d"), RayMarchingBatches.Num());

				// SDF Volume Manager for optimized ray marching
				static FSDFVolumeManager SDFVolumeManager;

				for (auto& Batch : RayMarchingBatches)
				{
					const FFluidRenderingParameters& BatchParams = Batch.Key;
					const TArray<UKawaiiFluidSSFRRenderer*>& Renderers = Batch.Value;

					RDG_EVENT_SCOPE(GraphBuilder, "FluidRayMarchBatch");

					UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: RayMarching batch - Renderers count = %d"), Renderers.Num());

					// Collect all particle positions from batch
					TArray<FVector3f> AllParticlePositions;
					float AverageParticleRadius = 10.0f;
					float TotalRadius = 0.0f;
					int32 ValidCount = 0;

					for (UKawaiiFluidSSFRRenderer* Renderer : Renderers)
					{
						FKawaiiFluidRenderResource* RenderResource = Renderer->GetFluidRenderResource();
						UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: RenderResource = %p, IsValid = %s"),
							RenderResource,
							RenderResource ? (RenderResource->IsValid() ? TEXT("true") : TEXT("false")) : TEXT("N/A"));

						if (RenderResource && RenderResource->IsValid())
						{
							const TArray<FKawaiiRenderParticle>& CachedParticles = RenderResource->GetCachedParticles();
							UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: CachedParticles count = %d"), CachedParticles.Num());
							for (const FKawaiiRenderParticle& Particle : CachedParticles)
							{
								AllParticlePositions.Add(Particle.Position);
							}
						}
						TotalRadius += Renderer->GetCachedParticleRadius();
						ValidCount++;
					}

					if (ValidCount > 0)
					{
						AverageParticleRadius = TotalRadius / ValidCount;
					}

					UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: AllParticlePositions = %d, AverageRadius = %.2f"),
						AllParticlePositions.Num(), AverageParticleRadius);

					if (AllParticlePositions.Num() == 0)
					{
						UE_LOG(LogTemp, Warning, TEXT("KawaiiFluid: No particles for RayMarching - skipping"));
						continue;
					}

					// Create RDG buffer for particle positions
					const uint32 BufferSize = AllParticlePositions.Num() * sizeof(FVector3f);
					FRDGBufferRef ParticleBuffer = GraphBuilder.CreateBuffer(
						FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), AllParticlePositions.Num()),
						TEXT("RayMarchParticlePositions"));

					GraphBuilder.QueueBufferUpload(
						ParticleBuffer,
						AllParticlePositions.GetData(),
						BufferSize,
						ERDGInitialDataFlags::None);  // Copy data to prevent flickering

					FRDGBufferSRVRef ParticleBufferSRV = GraphBuilder.CreateSRV(ParticleBuffer);

					// Get RayMarch composite pass from first renderer
					if (Renderers.Num() > 0 && Renderers[0]->GetCompositePass())
					{
						FFluidRayMarchComposite* RayMarchComposite =
							static_cast<FFluidRayMarchComposite*>(Renderers[0]->GetCompositePass().Get());

						// Check if SDF Volume optimization is enabled
						const bool bUseSDFVolume = BatchParams.bUseSDFVolumeOptimization;

						if (bUseSDFVolume)
						{
							// ============================================
							// Optimized Path: Bake SDF to 3D Volume Texture
							// ============================================
							RDG_EVENT_SCOPE(GraphBuilder, "SDFVolumeBake");

							// Set volume resolution from parameters
							int32 Resolution = FMath::Clamp(BatchParams.SDFVolumeResolution, 32, 256);
							SDFVolumeManager.SetVolumeResolution(FIntVector(Resolution, Resolution, Resolution));

							// Calculate bounding box for volume
							FVector3f VolumeMin, VolumeMax;
							float Margin = AverageParticleRadius * 2.0f;  // Extra margin around particles
							CalculateParticleBoundingBox(AllParticlePositions, AverageParticleRadius, Margin, VolumeMin, VolumeMax);

							UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: SDF Volume Bake - Min:(%.1f,%.1f,%.1f) Max:(%.1f,%.1f,%.1f)"),
								VolumeMin.X, VolumeMin.Y, VolumeMin.Z,
								VolumeMax.X, VolumeMax.Y, VolumeMax.Z);

							// Bake SDF volume using compute shader
							FRDGTextureSRVRef SDFVolumeSRV = SDFVolumeManager.BakeSDFVolume(
								GraphBuilder,
								ParticleBufferSRV,
								AllParticlePositions.Num(),
								AverageParticleRadius,
								BatchParams.SDFSmoothness,
								VolumeMin,
								VolumeMax);

							// Set volume data for ray marching
							RayMarchComposite->SetUseSDFVolume(true);
							RayMarchComposite->SetSDFVolumeData(
								SDFVolumeSRV,
								VolumeMin,
								VolumeMax,
								SDFVolumeManager.GetVolumeResolution());

							// Still need particle data for radius
							RayMarchComposite->SetParticleData(
								ParticleBufferSRV,
								AllParticlePositions.Num(),
								AverageParticleRadius);

							UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: Using SDF Volume optimization (%dx%dx%d)"),
								SDFVolumeManager.GetVolumeResolution().X,
								SDFVolumeManager.GetVolumeResolution().Y,
								SDFVolumeManager.GetVolumeResolution().Z);
						}
						else
						{
							// ============================================
							// Legacy Path: Direct particle iteration
							// ============================================
							RayMarchComposite->SetUseSDFVolume(false);
							RayMarchComposite->SetParticleData(
								ParticleBufferSRV,
								AllParticlePositions.Num(),
								AverageParticleRadius);

							UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: Using direct particle iteration (legacy)"));
						}

						// Render (no intermediate textures needed for ray marching)
						FFluidIntermediateTextures DummyIntermediateTextures;

						RayMarchComposite->RenderComposite(
							GraphBuilder,
							View,
							BatchParams,
							DummyIntermediateTextures,
							SceneDepthTexture,
							SceneColorInput.Texture,
							Output);

						UE_LOG(LogTemp, Verbose, TEXT("KawaiiFluid: Ray Marching rendered %d particles"), AllParticlePositions.Num());
					}
				}

				// ============================================
				// G-Buffer Mode: Transparency Pass (After Lighting)
				// ============================================
				if (GBufferBatches.Num() > 0)
				{
					RDG_EVENT_SCOPE(GraphBuilder, "FluidGBufferTransparency");

					UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: Processing %d G-Buffer batches for transparency"), GBufferBatches.Num());
					UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: Tonemap SceneDepthTexture = %p"), SceneDepthTexture);

					// Get GBuffer textures from SceneTextures (contains slime surface data)
					const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
					const FSceneTextures& SceneTexturesRef = ViewInfo.GetSceneTextures();
					FRDGTextureRef GBufferATexture = SceneTexturesRef.GBufferA;
					FRDGTextureRef GBufferDTexture = SceneTexturesRef.GBufferD;

					// Create a copy of SceneColor for reading (avoid read-write hazard)
					FRDGTextureRef LitSceneColorCopy = GraphBuilder.CreateTexture(
						SceneColorInput.Texture->Desc,
						TEXT("LitSceneColorCopy"));
					AddCopyTexturePass(GraphBuilder, SceneColorInput.Texture, LitSceneColorCopy);

					for (auto& Batch : GBufferBatches)
					{
						const FFluidRenderingParameters& BatchParams = Batch.Key;

						RDG_EVENT_SCOPE(GraphBuilder, "GBufferTransparencyBatch");

						// Apply transparency pass using stencil-masked regions
						// Use the copy for reading to avoid read-write hazard
						GFluidTransparencyComposite.RenderTransparency(
							GraphBuilder,
							View,
							BatchParams,
							SceneDepthTexture,
							LitSceneColorCopy,        // Copy of lit scene color (for reading)
							GBufferATexture,          // Normals for refraction
							GBufferDTexture,          // Thickness in R channel
							Output);

						UE_LOG(LogTemp, Log, TEXT("KawaiiFluid: G-Buffer transparency pass complete for batch"));
					}
				}

				// Debug Keep Alive
				GraphBuilder.QueueTextureExtraction(Output.Texture, &GFluidCompositeDebug_KeepAlive);

				return FScreenPassTexture(Output);
			}
		));
	}
}
