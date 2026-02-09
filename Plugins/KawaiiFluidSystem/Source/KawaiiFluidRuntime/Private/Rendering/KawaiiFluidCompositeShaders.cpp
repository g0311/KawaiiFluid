// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Rendering/KawaiiFluidCompositeShaders.h"

IMPLEMENT_GLOBAL_SHADER(FKawaiiFluidCompositeVS, "/Plugin/KawaiiFluidSystem/Private/FluidComposite.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FKawaiiFluidCompositePS, "/Plugin/KawaiiFluidSystem/Private/FluidComposite.usf", "MainPS", SF_Pixel);
