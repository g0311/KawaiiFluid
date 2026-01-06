// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/FluidVSMDepthShader.h"
#include "ShaderCore.h"

//=============================================================================
// Vertex Shader Implementation
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FFluidVSMDepthVS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidVSMDepth.usf",
                        "MainVS",
                        SF_Vertex);

/**
 * @brief Check if vertex shader permutation should be compiled.
 * @param Parameters Permutation parameters.
 * @return True if should compile.
 */
bool FFluidVSMDepthVS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

//=============================================================================
// Pixel Shader Implementation
//=============================================================================

IMPLEMENT_GLOBAL_SHADER(FFluidVSMDepthPS,
                        "/Plugin/KawaiiFluidSystem/Private/FluidVSMDepth.usf",
                        "MainPS",
                        SF_Pixel);

/**
 * @brief Check if pixel shader permutation should be compiled.
 * @param Parameters Permutation parameters.
 * @return True if should compile.
 */
bool FFluidVSMDepthPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}
