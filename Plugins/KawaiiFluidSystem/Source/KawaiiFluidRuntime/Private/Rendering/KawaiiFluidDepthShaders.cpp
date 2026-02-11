// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidDepthShaders.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphUtils.h"

// Shader implementation
IMPLEMENT_GLOBAL_SHADER(FKawaiiFluidDepthVS, "/Plugin/KawaiiFluidSystem/Private/FluidDepth.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FKawaiiFluidDepthPS, "/Plugin/KawaiiFluidSystem/Private/FluidDepth.usf", "MainPS", SF_Pixel);
