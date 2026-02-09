// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidThicknessShaders.h"

IMPLEMENT_GLOBAL_SHADER(FKawaiiFluidThicknessVS, "/Plugin/KawaiiFluidSystem/Private/FluidThickness.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FKawaiiFluidThicknessPS, "/Plugin/KawaiiFluidSystem/Private/FluidThickness.usf", "MainPS", SF_Pixel);
