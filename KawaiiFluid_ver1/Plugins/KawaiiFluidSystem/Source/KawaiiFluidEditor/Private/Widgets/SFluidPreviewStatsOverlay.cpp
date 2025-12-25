// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SFluidPreviewStatsOverlay.h"
#include "Preview/FluidPreviewScene.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SFluidPreviewStatsOverlay"

void SFluidPreviewStatsOverlay::Construct(const FArguments& InArgs, TSharedPtr<FFluidPreviewScene> InPreviewScene)
{
	PreviewScenePtr = InPreviewScene;
	CachedFPS = 60.0f;
	FPSAccumulator = 0.0f;
	FrameCount = 0;
	CachedParticleCount = 0;
	CachedDensity = 0.0f;

	ChildSlot
	[
		SNew(SBox)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Particle count
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SFluidPreviewStatsOverlay::GetParticleCountText)
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor::Black)
			]

			// Simulation time
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SFluidPreviewStatsOverlay::GetSimulationTimeText)
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor::Black)
			]

			// FPS
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SFluidPreviewStatsOverlay::GetFPSText)
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor::Black)
			]

			// Average density
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SFluidPreviewStatsOverlay::GetAverageDensityText)
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor::Black)
			]
		]
	];
}

void SFluidPreviewStatsOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Update FPS (averaged over multiple frames)
	if (InDeltaTime > 0.0f)
	{
		FPSAccumulator += 1.0f / InDeltaTime;
		FrameCount++;

		if (FrameCount >= 10)
		{
			CachedFPS = FPSAccumulator / FrameCount;
			FPSAccumulator = 0.0f;
			FrameCount = 0;
		}
	}

	// Update cached values from preview scene
	TSharedPtr<FFluidPreviewScene> PreviewScene = PreviewScenePtr.Pin();
	if (PreviewScene.IsValid())
	{
		CachedParticleCount = PreviewScene->GetParticleCount();
		CachedDensity = PreviewScene->GetAverageDensity();
	}
}

FText SFluidPreviewStatsOverlay::GetParticleCountText() const
{
	return FText::Format(LOCTEXT("ParticleCount", "Particles: {0}"), FText::AsNumber(CachedParticleCount));
}

FText SFluidPreviewStatsOverlay::GetSimulationTimeText() const
{
	TSharedPtr<FFluidPreviewScene> PreviewScene = PreviewScenePtr.Pin();
	float SimTime = PreviewScene.IsValid() ? PreviewScene->GetSimulationTime() : 0.0f;
	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 2;
	return FText::Format(LOCTEXT("SimulationTime", "Time: {0}s"), FText::AsNumber(SimTime, &Options));
}

FText SFluidPreviewStatsOverlay::GetFPSText() const
{
	return FText::Format(LOCTEXT("FPS", "FPS: {0}"), FText::AsNumber(FMath::RoundToInt(CachedFPS)));
}

FText SFluidPreviewStatsOverlay::GetAverageDensityText() const
{
	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 1;
	return FText::Format(LOCTEXT("AvgDensity", "Avg Density: {0}"), FText::AsNumber(CachedDensity, &Options));
}

#undef LOCTEXT_NAMESPACE
