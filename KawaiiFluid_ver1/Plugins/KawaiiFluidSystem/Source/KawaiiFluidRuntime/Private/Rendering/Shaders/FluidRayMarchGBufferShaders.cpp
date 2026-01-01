// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Rendering/Shaders/FluidRayMarchGBufferShaders.h"

IMPLEMENT_GLOBAL_SHADER(FFluidRayMarchGBufferVS,
	"/Plugin/KawaiiFluidSystem/Private/FluidRayMarchGBuffer.usf",
	"MainVS",
	SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FFluidRayMarchGBufferPS,
	"/Plugin/KawaiiFluidSystem/Private/FluidRayMarchGBuffer.usf",
	"MainPS",
	SF_Pixel);
