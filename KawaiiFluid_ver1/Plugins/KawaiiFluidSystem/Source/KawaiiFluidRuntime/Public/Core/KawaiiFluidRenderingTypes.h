// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KawaiiFluidRenderingTypes.generated.h"

/**
 * Shadow mesh quality levels for HISM shadow casting.
 * Controls the polygon count of instanced spheres used for fluid shadows.
 */
UENUM(BlueprintType)
enum class EFluidShadowMeshQuality : uint8
{
	/** 8 triangles (Octahedron) - Best performance, angular shadows */
	Low UMETA(DisplayName = "Low (8 tri)"),

	/** 20 triangles (Icosphere) - Balanced performance and quality */
	Medium UMETA(DisplayName = "Medium (20 tri)"),

	/** 80 triangles (Icosphere subdivided) - Smooth shadows */
	High UMETA(DisplayName = "High (80 tri)")
};

/**
 * Debug draw mode for particle visualization
 * Controls the rendering method for debug visualization.
 */
UENUM(BlueprintType)
enum class EKawaiiFluidDebugDrawMode : uint8
{
	None        UMETA(DisplayName = "None", ToolTip = "No debug visualization"),
	ISM         UMETA(DisplayName = "ISM Sphere", ToolTip = "Render particles as instanced spheres (disables Metaball)"),
	DebugDraw   UMETA(DisplayName = "Debug Point", ToolTip = "Draw particles as debug points (no GPU cost)"),
};

/**
 * Debug visualization modes for fluid particles
 * Determines the coloring scheme for debug particles.
 */
UENUM(BlueprintType)
enum class EFluidDebugVisualization : uint8
{
	/** Normal rendering (no debug) */
	None			UMETA(DisplayName = "None"),

	//--- Z-Order Sorting Verification ---
	// Use these modes to verify Z-Order (Morton code) sorting is working correctly.
	// If sorting works, spatially close particles will have similar array indices.

	/** [Z-Order] Color by array index - spatially close particles should have similar colors if sorted */
	ZOrderArrayIndex	UMETA(DisplayName = "Z-Order: Array Index"),
	/** [Z-Order] Color by Morton code computed from position */
	ZOrderMortonCode	UMETA(DisplayName = "Z-Order: Morton Code"),

	//--- General Debug Visualization ---

	/** Color by X position (Red gradient) */
	PositionX		UMETA(DisplayName = "Position X"),
	/** Color by Y position (Green gradient) */
	PositionY		UMETA(DisplayName = "Position Y"),
	/** Color by Z position (Blue gradient) */
	PositionZ		UMETA(DisplayName = "Position Z"),
	/** Color by density value */
	Density			UMETA(DisplayName = "Density"),

	//--- Legacy (deprecated, use ZOrderArrayIndex/ZOrderMortonCode instead) ---
	ArrayIndex		UMETA(DisplayName = "Array Index (Legacy)", Hidden),
	MortonCode		UMETA(DisplayName = "Morton Code (Legacy)", Hidden),
};

/**
 * Splash VFX condition mode
 * Determines when splash/spray VFX are spawned.
 */
UENUM(BlueprintType)
enum class ESplashConditionMode : uint8
{
	VelocityAndIsolation UMETA(DisplayName = "Velocity AND Isolation", ToolTip = "Fast-moving AND isolated particles (most accurate, like FleX diffuse)"),
	VelocityOrIsolation  UMETA(DisplayName = "Velocity OR Isolation", ToolTip = "Fast-moving OR isolated particles (more VFX spawns)"),
	VelocityOnly         UMETA(DisplayName = "Velocity Only", ToolTip = "Only fast-moving particles"),
	IsolationOnly        UMETA(DisplayName = "Isolation Only", ToolTip = "Only isolated particles"),
};
