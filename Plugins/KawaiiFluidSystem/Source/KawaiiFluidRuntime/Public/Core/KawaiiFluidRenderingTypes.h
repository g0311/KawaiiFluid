// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KawaiiFluidRenderingTypes.generated.h"

/**
 * @enum EFluidShadowMeshQuality
 * @brief Shadow mesh quality levels for ISM (Instanced Static Mesh) shadow casting.
 */
UENUM(BlueprintType)
enum class EFluidShadowMeshQuality : uint8
{
	Low UMETA(DisplayName = "Low (8 tri)", ToolTip = "8 triangles (Octahedron) - Best performance, but results in angular shadows."),
	Medium UMETA(DisplayName = "Medium (20 tri)", ToolTip = "20 triangles (Icosphere) - Balanced performance and quality."),
	High UMETA(DisplayName = "High (80 tri)", ToolTip = "80 triangles (Icosphere subdivided) - High quality, smooth shadows.")
};

/**
 * @enum EKawaiiFluidDebugDrawMode
 * @brief Debug draw mode for particle visualization.
 */
UENUM(BlueprintType)
enum class EKawaiiFluidDebugDrawMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No debug visualization (normal Metaball rendering)."),
	ISM UMETA(DisplayName = "ISM Sphere", ToolTip = "Render particles as instanced spheres with a solid color."),
	Point_ZOrderArrayIndex UMETA(DisplayName = "Z-Order Array Index", ToolTip = "Debug points colored by their array index to verify Z-Order sorting."),
	Point_ZOrderMortonCode UMETA(DisplayName = "Z-Order Morton Code", ToolTip = "Debug points colored by their Morton code to verify Z-Order consistency."),
	Point_PositionX UMETA(DisplayName = "Position X", ToolTip = "Debug points colored based on their X-axis position (Red gradient)."),
	Point_PositionY UMETA(DisplayName = "Position Y", ToolTip = "Debug points colored based on their Y-axis position (Green gradient)."),
	Point_PositionZ UMETA(DisplayName = "Position Z", ToolTip = "Debug points colored based on their Z-axis position (Blue gradient)."),
	Point_Density UMETA(DisplayName = "Density", ToolTip = "Debug points colored based on their calculated density value."),
	Point_IsAttached UMETA(DisplayName = "Is Attached (Boundary)", ToolTip = "Debug points colored based on attachment status for boundary debugging."),
	DebugDraw UMETA(DisplayName = "Debug Point (Legacy)", ToolTip = "Legacy debug draw mode for backwards compatibility.", Hidden)
};

/** 
 * @brief Helper function to check if a debug mode uses point rendering.
 * @param Mode The debug draw mode to check.
 * @return True if it is a point-based debug mode.
 */
FORCEINLINE bool IsPointDebugMode(EKawaiiFluidDebugDrawMode Mode)
{
	return Mode >= EKawaiiFluidDebugDrawMode::Point_ZOrderArrayIndex &&
	       Mode <= EKawaiiFluidDebugDrawMode::Point_IsAttached;
}

/** 
 * @brief Helper function to check if a debug mode requires reading back particle data from the GPU.
 * @param Mode The debug draw mode to check.
 * @return True if GPU readback is required for this mode.
 */
FORCEINLINE bool RequiresGPUReadback(EKawaiiFluidDebugDrawMode Mode)
{
	return Mode == EKawaiiFluidDebugDrawMode::ISM || IsPointDebugMode(Mode);
}

/**
 * @enum ESplashConditionMode
 * @brief Splash VFX condition mode determining when splash or spray effects should be spawned.
 */
UENUM(BlueprintType)
enum class ESplashConditionMode : uint8
{
	VelocityAndIsolation UMETA(DisplayName = "Velocity AND Isolation", ToolTip = "Fast-moving AND isolated particles (most accurate, like FleX diffuse)"),
	VelocityOrIsolation  UMETA(DisplayName = "Velocity OR Isolation", ToolTip = "Fast-moving OR isolated particles (more VFX spawns)"),
	VelocityOnly         UMETA(DisplayName = "Velocity Only", ToolTip = "Only fast-moving particles"),
	IsolationOnly        UMETA(DisplayName = "Isolation Only", ToolTip = "Only isolated particles"),
};
