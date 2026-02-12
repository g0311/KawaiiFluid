// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Editor/KawaiiFluidPresetAssetEditor.h"
#include "Core/KawaiiFluidPresetDataAsset.h"
#include "Preview/KawaiiFluidPreviewScene.h"
#include "Preview/KawaiiFluidPreviewSettings.h"
#include "Viewport/SKawaiiFluidPresetEditorViewport.h"
#include "Widgets/SKawaiiFluidPreviewPlaybackControls.h"
#include "Style/KawaiiFluidEditorStyle.h"
#include "Components/InstancedStaticMeshComponent.h"

#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "AdvancedPreviewScene.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "KawaiiFluidPresetAssetEditor"

const FName FKawaiiFluidPresetAssetEditor::ViewportTabId(TEXT("FluidPresetEditor_Viewport"));
const FName FKawaiiFluidPresetAssetEditor::DetailsTabId(TEXT("FluidPresetEditor_Details"));
const FName FKawaiiFluidPresetAssetEditor::PreviewSettingsTabId(TEXT("FluidPresetEditor_PreviewSettings"));
const FName FKawaiiFluidPresetAssetEditor::AppIdentifier(TEXT("FluidPresetEditorApp"));

/**
 * @brief Default constructor initializing playback state and speed.
 */
FKawaiiFluidPresetAssetEditor::FKawaiiFluidPresetAssetEditor()
	: EditingPreset(nullptr)
	, bIsPlaying(true)
	, SimulationSpeed(1.0f)
{
}

FKawaiiFluidPresetAssetEditor::~FKawaiiFluidPresetAssetEditor()
{
	UnbindEditorDelegates();

	// Cleanup
	ViewportWidget.Reset();
	PreviewScene.Reset();
	DetailsView.Reset();
	PreviewSettingsView.Reset();

	GEditor->UnregisterForUndo(this);
}

/**
 * @brief Configures the editor layout, creates the preview scene, and initializes UI components.
 * @param Mode The toolkit mode (Standalone or WorldCentric)
 * @param InitToolkitHost The host for the toolkit
 * @param InPreset The fluid preset asset to edit
 */
void FKawaiiFluidPresetAssetEditor::InitFluidPresetEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UKawaiiFluidPresetDataAsset* InPreset)
{
	EditingPreset = InPreset;

	// Register for undo
	GEditor->RegisterForUndo(this);

	// Create preview scene
	FPreviewScene::ConstructionValues CVS;
	CVS.bCreatePhysicsScene = false;
	CVS.LightBrightness = 3;
	CVS.SkyBrightness = 1;
	PreviewScene = MakeShared<FKawaiiFluidPreviewScene>(CVS);
	PreviewScene->SetPreset(EditingPreset);

	// Start simulation immediately (bIsPlaying defaults to true)
	PreviewScene->StartSimulation();

	// Bind to property changes
	BindEditorDelegates();

	// Create default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_FluidPresetEditor_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				// Left side - Viewport
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab(ViewportTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				// Right side - Details panels
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.3f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(PreviewSettingsTabId, ETabState::OpenedTab)
				)
			)
		);

	// Initialize the asset editor
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		AppIdentifier,
		StandaloneDefaultLayout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		InPreset);

	// Extend toolbar with playback controls
	ExtendToolbar();

	RegenerateMenusAndToolbars();
}

/**
 * @brief Returns the unique toolkit name for this editor.
 * @return FName of the toolkit
 */
FName FKawaiiFluidPresetAssetEditor::GetToolkitFName() const
{
	return FName("FluidPresetEditor");
}

/**
 * @brief Returns the human-readable name for the toolkit.
 * @return Localized text of the toolkit name
 */
FText FKawaiiFluidPresetAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Fluid Preset Editor");
}

/**
 * @brief Returns the prefix for tabs spawned in world-centric mode.
 * @return Tab prefix string
 */
FString FKawaiiFluidPresetAssetEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "FluidPreset ").ToString();
}

/**
 * @brief Returns the color scale for world-centric tabs.
 * @return Tab color scale
 */
FLinearColor FKawaiiFluidPresetAssetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.2f, 0.4f, 0.8f, 0.5f);
}

/**
 * @brief Registers the tab spawners for the preset editor.
 * @param InTabManager The tab manager to register spawners with
 */
void FKawaiiFluidPresetAssetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "Fluid Preset Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	// Viewport tab
	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FKawaiiFluidPresetAssetEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	// Details tab
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FKawaiiFluidPresetAssetEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	// Preview settings tab
	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FKawaiiFluidPresetAssetEditor::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSettingsTab", "Preview Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.WorldSettings"));
}

/**
 * @brief Unregisters the tab spawners for the preset editor.
 * @param InTabManager The tab manager to unregister spawners from
 */
void FKawaiiFluidPresetAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewSettingsTabId);
}

/**
 * @brief Spawns the viewport tab.
 * @param Args The spawn tab arguments
 * @return The spawned dock tab
 */
TSharedRef<SDockTab> FKawaiiFluidPresetAssetEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == ViewportTabId);

	ViewportWidget = SNew(SKawaiiFluidPresetEditorViewport, PreviewScene, SharedThis(this));

	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabLabel", "Viewport"))
		[
			ViewportWidget.ToSharedRef()
		];
}

/**
 * @brief Spawns the details tab for the asset.
 * @param Args The spawn tab arguments
 * @return The spawned dock tab
 */
TSharedRef<SDockTab> FKawaiiFluidPresetAssetEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bShowOptions = true;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(EditingPreset);

	// Bind property change notification
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FKawaiiFluidPresetAssetEditor::OnPresetPropertyChanged);

	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Preset Details"))
		[
			DetailsView.ToSharedRef()
		];
}

/**
 * @brief Spawns the preview settings tab.
 * @param Args The spawn tab arguments
 * @return The spawned dock tab
 */
TSharedRef<SDockTab> FKawaiiFluidPresetAssetEditor::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSettingsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bShowOptions = false;

	PreviewSettingsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	if (PreviewScene.IsValid())
	{
		PreviewSettingsView->SetObject(PreviewScene->GetPreviewSettingsObject());
	}

	// Bind property change notification for preview settings
	PreviewSettingsView->OnFinishedChangingProperties().AddSP(this, &FKawaiiFluidPresetAssetEditor::OnPreviewSettingsPropertyChanged);

	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewSettingsTabLabel", "Preview Settings"))
		[
			PreviewSettingsView.ToSharedRef()
		];
}

/**
 * @brief Extends the toolbar with custom playback controls.
 */
void FKawaiiFluidPresetAssetEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.AddWidget(SNew(SKawaiiFluidPreviewPlaybackControls, SharedThis(this)));
		}));

	AddToolbarExtender(ToolbarExtender);
}

/**
 * @brief Refreshes the preview when an undo operation is performed.
 * @param bSuccess Whether the undo was successful
 */
void FKawaiiFluidPresetAssetEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshPreview();
	}
}

/**
 * @brief Refreshes the preview when a redo operation is performed.
 * @param bSuccess Whether the redo was successful
 */
void FKawaiiFluidPresetAssetEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshPreview();
	}
}

/**
 * @brief Updates the simulation state every frame.
 * @param DeltaTime The time passed since the last frame
 */
void FKawaiiFluidPresetAssetEditor::Tick(float DeltaTime)
{
	UpdateSimulation(DeltaTime);
}

/**
 * @brief Returns the stat ID for performance profiling.
 * @return The stat ID
 */
TStatId FKawaiiFluidPresetAssetEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FKawaiiFluidPresetAssetEditor, STATGROUP_Tickables);
}

/**
 * @brief Resumes simulation playback.
 */
void FKawaiiFluidPresetAssetEditor::Play()
{
	bIsPlaying = true;
	if (PreviewScene.IsValid())
	{
		PreviewScene->StartSimulation();
	}
}

/**
 * @brief Pauses simulation playback.
 */
void FKawaiiFluidPresetAssetEditor::Pause()
{
	bIsPlaying = false;
	if (PreviewScene.IsValid())
	{
		PreviewScene->StopSimulation();
	}
}

/**
 * @brief Stops simulation and resets particle state.
 */
void FKawaiiFluidPresetAssetEditor::Stop()
{
	bIsPlaying = false;
	if (PreviewScene.IsValid())
	{
		PreviewScene->StopSimulation();
		PreviewScene->ResetSimulation();
	}
}

/**
 * @brief Resets particles to initial state.
 */
void FKawaiiFluidPresetAssetEditor::Reset()
{
	if (PreviewScene.IsValid())
	{
		PreviewScene->ResetSimulation();
		if (bIsPlaying)
		{
			PreviewScene->StartSimulation();
		}
	}
}

/**
 * @brief Sets the simulation speed multiplier.
 * @param Speed The new speed multiplier
 */
void FKawaiiFluidPresetAssetEditor::SetSimulationSpeed(float Speed)
{
	SimulationSpeed = FMath::Clamp(Speed, 0.0f, 4.0f);
}

/**
 * @brief Handler for property changes in the Details panel.
 * @param PropertyChangedEvent Information about the property change
 */
void FKawaiiFluidPresetAssetEditor::OnPresetPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshPreview();
}

/**
 * @brief Handler for property changes in the Preview Settings panel.
 * @param PropertyChangedEvent Information about the property change
 */
void FKawaiiFluidPresetAssetEditor::OnPreviewSettingsPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!PreviewScene.IsValid())
	{
		return;
	}

	// Check if spawn-related property changed (requires simulation reset)
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const bool bNeedsReset =
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, EmitterMode) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, ShapeType) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, SphereRadius) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, CubeHalfSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, CylinderRadius) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, CylinderHalfHeight) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, PreviewSpawnOffset) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FFluidPreviewSettings, JitterAmount);

	if (bNeedsReset)
	{
		PreviewScene->ResetSimulation();
	}

	// Apply preview settings (environment, rendering, etc.)
	PreviewScene->ApplyPreviewSettings();

	if (ViewportWidget.IsValid())
	{
		ViewportWidget->RefreshViewport();
	}
}

/**
 * @brief Synchronizes the preview scene with the current asset data.
 */
void FKawaiiFluidPresetAssetEditor::RefreshPreview()
{
	if (PreviewScene.IsValid())
	{
		PreviewScene->RefreshFromPreset();
	}

	if (ViewportWidget.IsValid())
	{
		ViewportWidget->RefreshViewport();
	}
}

/**
 * @brief Internal tick handler that advances the simulation.
 * @param DeltaTime The time step for the simulation update
 */
void FKawaiiFluidPresetAssetEditor::UpdateSimulation(float DeltaTime)
{
	if (!bIsPlaying || !PreviewScene.IsValid() || !ViewportWidget.IsValid())
	{
		return;
	}

	// Only simulate when this editor's window is focused
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		TSharedPtr<SWindow> MyWindow = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());
		if (!ActiveWindow.IsValid() || !MyWindow.IsValid() || MyWindow != ActiveWindow)
		{
			return;
		}
	}

	float AdjustedDeltaTime = FMath::Clamp(DeltaTime * SimulationSpeed, 0.0f, 0.1f);
	PreviewScene->TickSimulation(AdjustedDeltaTime);
}

void FKawaiiFluidPresetAssetEditor::BindEditorDelegates()
{
	if (EditingPreset)
	{
		// Property changes are handled through DetailsView->OnFinishedChangingProperties()
	}
}

void FKawaiiFluidPresetAssetEditor::UnbindEditorDelegates()
{
	// Unbind any delegates
}

#undef LOCTEXT_NAMESPACE