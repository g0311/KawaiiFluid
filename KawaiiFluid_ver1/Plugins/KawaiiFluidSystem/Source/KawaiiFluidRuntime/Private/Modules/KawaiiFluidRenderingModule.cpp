// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Modules/KawaiiFluidRenderingModule.h"
#include "Rendering/KawaiiFluidISMRenderer.h"
#include "Rendering/KawaiiFluidSSFRRenderer.h"
#include "Core/FluidParticle.h"

UKawaiiFluidRenderingModule::UKawaiiFluidRenderingModule()
{
	// Create renderer instances as default subobjects (Instanced pattern)
	ISMRenderer = CreateDefaultSubobject<UKawaiiFluidISMRenderer>(TEXT("ISMRenderer"));
	SSFRRenderer = CreateDefaultSubobject<UKawaiiFluidSSFRRenderer>(TEXT("SSFRRenderer"));
}

void UKawaiiFluidRenderingModule::Initialize(UWorld* InWorld, USceneComponent* InOwnerComponent, IKawaiiFluidDataProvider* InDataProvider)
{
	CachedWorld = InWorld;
	CachedOwnerComponent = InOwnerComponent;
	DataProviderPtr = InDataProvider;

	// CreateDefaultSubobject only works in CDO context.
	// If created via NewObject (e.g., editor preview), renderers will be nullptr.
	// Create them here if missing.
	if (!ISMRenderer)
	{
		ISMRenderer = NewObject<UKawaiiFluidISMRenderer>(this, TEXT("ISMRenderer"));
		UE_LOG(LogTemp, Log, TEXT("RenderingModule: Created ISMRenderer via NewObject (non-CDO context)"));
	}

	if (!SSFRRenderer)
	{
		SSFRRenderer = NewObject<UKawaiiFluidSSFRRenderer>(this, TEXT("SSFRRenderer"));
		UE_LOG(LogTemp, Log, TEXT("RenderingModule: Created SSFRRenderer via NewObject (non-CDO context)"));
	}

	// Initialize renderers (컴포넌트에 부착)
	if (ISMRenderer)
	{
		ISMRenderer->Initialize(InWorld, InOwnerComponent);
	}

	if (SSFRRenderer)
	{
		SSFRRenderer->Initialize(InWorld, InOwnerComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("RenderingModule: Initialized (ISM: %s, SSFR: %s)"),
		ISMRenderer && ISMRenderer->IsEnabled() ? TEXT("Enabled") : TEXT("Disabled"),
		SSFRRenderer && SSFRRenderer->IsEnabled() ? TEXT("Enabled") : TEXT("Disabled"));
}

void UKawaiiFluidRenderingModule::Cleanup()
{
	if (ISMRenderer)
	{
		ISMRenderer->Cleanup();
	}

	if (SSFRRenderer)
	{
		SSFRRenderer->Cleanup();
	}

	DataProviderPtr = nullptr;
	CachedWorld = nullptr;
	CachedOwnerComponent = nullptr;
}

void UKawaiiFluidRenderingModule::UpdateRenderers()
{
	if (!DataProviderPtr)
	{
		return;
	}

	// Update all enabled renderers (파티클 0개여도 업데이트해서 ISM 클리어)
	if (ISMRenderer && ISMRenderer->IsEnabled())
	{
		ISMRenderer->UpdateRendering(DataProviderPtr, 0.0f);
	}

	if (SSFRRenderer && SSFRRenderer->IsEnabled())
	{
		SSFRRenderer->UpdateRendering(DataProviderPtr, 0.0f);
	}
}

int32 UKawaiiFluidRenderingModule::GetParticleCount() const
{
	if (DataProviderPtr)
	{
		return DataProviderPtr->GetParticleCount();
	}
	return 0;
}
