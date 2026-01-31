// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KawaiiFluidRenderingTypes.generated.h"

/**
 * Shadow mesh quality levels for ISM shadow casting.
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
 * Debug draw mode for particle visualization.
 * Controls both rendering method and coloring scheme.
 * All debug modes use simulation ParticleRadius for accurate size representation.
 */
UENUM(BlueprintType)
enum class EKawaiiFluidDebugDrawMode : uint8
{
	/** No debug visualization (normal Metaball rendering) */
	None UMETA(DisplayName = "None"),

	//--- ISM (Instanced Static Mesh) ---

	/** Render particles as instanced spheres with solid color */
	ISM UMETA(DisplayName = "ISM Sphere"),

	//--- Debug Point (DrawDebugPoint) ---

	/** Debug points colored by array index (Z-Order verification) */
	Point_ZOrderArrayIndex UMETA(DisplayName = "Z-Order Array Index"),
	/** Debug points colored by Morton code (Z-Order verification) */
	Point_ZOrderMortonCode UMETA(DisplayName = "Z-Order Morton Code"),
	/** Debug points colored by X position (Red gradient) */
	Point_PositionX UMETA(DisplayName = "Position X"),
	/** Debug points colored by Y position (Green gradient) */
	Point_PositionY UMETA(DisplayName = "Position Y"),
	/** Debug points colored by Z position (Blue gradient) */
	Point_PositionZ UMETA(DisplayName = "Position Z"),
	/** Debug points colored by density value */
	Point_Density UMETA(DisplayName = "Density"),
	/** Debug points colored by attachment status (boundary debug) */
	Point_IsAttached UMETA(DisplayName = "Is Attached (Boundary)"),

	//--- Legacy (for backwards compatibility) ---
	DebugDraw UMETA(DisplayName = "Debug Point (Legacy)", Hidden),
};

/** Helper to check if mode is a Point debug mode */
FORCEINLINE bool IsPointDebugMode(EKawaiiFluidDebugDrawMode Mode)
{
	return Mode >= EKawaiiFluidDebugDrawMode::Point_ZOrderArrayIndex &&
	       Mode <= EKawaiiFluidDebugDrawMode::Point_IsAttached;
}

/** Helper to check if mode requires GPU readback */
FORCEINLINE bool RequiresGPUReadback(EKawaiiFluidDebugDrawMode Mode)
{
	return Mode == EKawaiiFluidDebugDrawMode::ISM || IsPointDebugMode(Mode);
}

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
