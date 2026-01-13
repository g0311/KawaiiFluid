// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRDGBuilder;
class FSceneView;
class FRDGTexture;
class UKawaiiFluidMetaballRenderer;
typedef FRDGTexture* FRDGTextureRef;

/**
 * Fluid Depth 렌더링 패스 (Batched path)
 * 지정된 렌더러 리스트만 렌더링 (배치 최적화용)
 * @param OutLinearDepthTexture 출력: 선형 깊이 텍스처 (R32F)
 * @param OutVelocityTexture 출력: 스크린 공간 속도 텍스처 (RG16F) - Flow 텍스처용
 */
void RenderFluidDepthPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const TArray<UKawaiiFluidMetaballRenderer*>& Renderers,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef& OutLinearDepthTexture,
	FRDGTextureRef& OutVelocityTexture);
