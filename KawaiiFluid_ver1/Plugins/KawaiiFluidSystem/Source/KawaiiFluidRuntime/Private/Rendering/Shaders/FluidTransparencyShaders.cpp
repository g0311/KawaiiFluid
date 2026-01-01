// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Shaders/FluidTransparencyShaders.h"

IMPLEMENT_GLOBAL_SHADER(FFluidTransparencyVS,
	"/Plugin/KawaiiFluidSystem/Private/FluidTransparencyPass.usf",
	"MainVS",
	SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FFluidTransparencyPS,
	"/Plugin/KawaiiFluidSystem/Private/FluidTransparencyPass.usf",
	"MainPS",
	SF_Pixel);
