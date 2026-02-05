# KawaiiFluidSystem

**KawaiiFluidSystem** is a GPU-based PBF (Position Based Fluids) viscous fluid simulation system for Unreal Engine 5.  
It simulates tens of thousands of particles in real-time using high-performance GPU computing, implementing various physical phenomena such as viscosity, adhesion, and surface tension.

## ✨ Key Features

*   **GPU-Based PBF Simulation**: High-performance physics engine capable of processing tens of thousands of particles in real-time.
*   **Advanced Physical Properties**:
    *   **Viscosity**: Realistic representation of thick fluids like honey or lava.
    *   **Adhesion**: Particle-to-surface sticking effects.
    *   **Density Constraints**: Volume preservation and pressure solving.
    *   **Stack Pressure**: Stable pressure simulation in deep fluid layers.
*   **Robust Collision System**:
    *   Support for primitive colliders (Box, Capsule).
    *   Static Mesh and **Skeletal Mesh (BVH-based)** collision support.
*   **High-Quality Rendering**:
    *   **Anisotropy**: Smooth fluid surfaces by rendering particles as ellipsoids.
    *   Support for Metaballs and Instanced Static Mesh (ISM) rendering.
*   **Data-Asset Driven Presets**: Manage various fluid settings (Water, Lava, Honey, Slime, etc.) via reusable data assets.
*   **Editor Workflow**: Particle placement/removal using brush tools and real-time debug visualization.

## 🏗️ System Architecture

1.  **Kawaii Fluid Volume**: The fundamental simulation unit and solver. All physics calculations and particle data are managed within the volume.
2.  **Kawaii Fluid Emitter**: An actor that generates fluid particles and supplies them to a specific volume.
3.  **Kawaii Fluid Collider**: Handles interaction between fluids and objects (static or dynamic).
4.  **Preset System**: Decouples physical properties (viscosity, spacing, mass, etc.) and rendering settings into assets for high reusability.

## 🚀 Getting Started

### Installation
1. Copy the `KawaiiFluidSystem` folder into your project's `Plugins` directory.
2. Right-click your `.uproject` file and select `Generate Visual Studio project files`.
3. Build the project and launch Unreal Engine.

### Basic Usage
1. Place a **Kawaii Fluid Volume** actor in your level.
2. Assign a `Fluid Preset` in the `VolumeComponent` of the volume actor.
3. Place a **Kawaii Fluid Emitter** inside the volume to start generating particles.
4. Add a **Kawaii Fluid Collider** component to any object that needs to interact with the fluid.

## 🛠️ Technical Details

*   **Solver**: Position Based Fluids (PBF)
*   **Neighbor Search**: GPU Spatial Hashing (Z-Order based)
*   **Shaders**: Custom HLSL (USF/USH)
*   **Minimum Specs**: DirectX 12 compatible GPU (Windows 64-bit)

## 📄 License
Copyright 2026 Team_Bruteforce. All Rights Reserved.