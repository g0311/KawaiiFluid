// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SFluidPreviewPlaybackControls.h"
#include "Editor/KawaiiFluidPresetAssetEditor.h"
#include "Style/FluidEditorStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SFluidPreviewPlaybackControls"

void SFluidPreviewPlaybackControls::Construct(const FArguments& InArgs, TSharedPtr<FKawaiiFluidPresetAssetEditor> InEditor)
{
	EditorPtr = InEditor;

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Play/Pause button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SButton)
			.OnClicked(this, &SFluidPreviewPlaybackControls::OnPlayPauseClicked)
			.ToolTipText(this, &SFluidPreviewPlaybackControls::GetPlayPauseTooltip)
			[
				SNew(STextBlock)
				.Text(this, &SFluidPreviewPlaybackControls::GetPlayPauseButtonText)
			]
		]

		// Stop button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SButton)
			.OnClicked(this, &SFluidPreviewPlaybackControls::OnStopClicked)
			.ToolTipText(LOCTEXT("StopTooltip", "Stop and Reset Simulation"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StopButton", "Stop"))
			]
		]

		// Reset button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SButton)
			.OnClicked(this, &SFluidPreviewPlaybackControls::OnResetClicked)
			.ToolTipText(LOCTEXT("ResetTooltip", "Reset Particles (keep playing)"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ResetButton", "Reset"))
			]
		]

		// Separator
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 2.0f)
		[
			SNew(SSeparator)
			.Orientation(Orient_Vertical)
		]

		// Speed label
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SpeedLabel", "Speed:"))
		]

		// Speed spinbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(80.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(4.0f)
				.MinSliderValue(0.0f)
				.MaxSliderValue(2.0f)
				.Delta(0.1f)
				.Value(this, &SFluidPreviewPlaybackControls::GetCurrentSpeed)
				.OnValueChanged(this, &SFluidPreviewPlaybackControls::OnSpeedChanged)
				.ToolTipText(LOCTEXT("SpeedTooltip", "Simulation Speed Multiplier"))
			]
		]

		// Speed text
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SFluidPreviewPlaybackControls::GetSpeedText)
		]
	];
}

FReply SFluidPreviewPlaybackControls::OnPlayPauseClicked()
{
	TSharedPtr<FKawaiiFluidPresetAssetEditor> Editor = EditorPtr.Pin();
	if (Editor.IsValid())
	{
		if (Editor->IsPlaying())
		{
			Editor->Pause();
		}
		else
		{
			Editor->Play();
		}
	}
	return FReply::Handled();
}

FReply SFluidPreviewPlaybackControls::OnStopClicked()
{
	TSharedPtr<FKawaiiFluidPresetAssetEditor> Editor = EditorPtr.Pin();
	if (Editor.IsValid())
	{
		Editor->Stop();
	}
	return FReply::Handled();
}

FReply SFluidPreviewPlaybackControls::OnResetClicked()
{
	TSharedPtr<FKawaiiFluidPresetAssetEditor> Editor = EditorPtr.Pin();
	if (Editor.IsValid())
	{
		Editor->Reset();
	}
	return FReply::Handled();
}

bool SFluidPreviewPlaybackControls::IsPlaying() const
{
	TSharedPtr<FKawaiiFluidPresetAssetEditor> Editor = EditorPtr.Pin();
	return Editor.IsValid() && Editor->IsPlaying();
}

bool SFluidPreviewPlaybackControls::IsPaused() const
{
	return !IsPlaying();
}

bool SFluidPreviewPlaybackControls::CanPlay() const
{
	TSharedPtr<FKawaiiFluidPresetAssetEditor> Editor = EditorPtr.Pin();
	return Editor.IsValid() && Editor->GetEditingPreset() != nullptr;
}

FText SFluidPreviewPlaybackControls::GetPlayPauseButtonText() const
{
	if (IsPlaying())
	{
		return LOCTEXT("PauseButton", "Pause");
	}
	else
	{
		return LOCTEXT("PlayButton", "Play");
	}
}

FText SFluidPreviewPlaybackControls::GetPlayPauseTooltip() const
{
	if (IsPlaying())
	{
		return LOCTEXT("PauseTooltip", "Pause Simulation");
	}
	else
	{
		return LOCTEXT("PlayTooltip", "Play Simulation");
	}
}

void SFluidPreviewPlaybackControls::OnSpeedChanged(float NewValue)
{
	TSharedPtr<FKawaiiFluidPresetAssetEditor> Editor = EditorPtr.Pin();
	if (Editor.IsValid())
	{
		Editor->SetSimulationSpeed(NewValue);
	}
}

float SFluidPreviewPlaybackControls::GetCurrentSpeed() const
{
	TSharedPtr<FKawaiiFluidPresetAssetEditor> Editor = EditorPtr.Pin();
	if (Editor.IsValid())
	{
		return Editor->GetSimulationSpeed();
	}
	return 1.0f;
}

TOptional<float> SFluidPreviewPlaybackControls::GetSpeedAsOptional() const
{
	return GetCurrentSpeed();
}

FText SFluidPreviewPlaybackControls::GetSpeedText() const
{
	return FText::Format(LOCTEXT("SpeedFormat", "x"), FText::AsNumber(GetCurrentSpeed()));
}

#undef LOCTEXT_NAMESPACE
