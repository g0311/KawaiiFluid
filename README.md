# Kawaii Fluid System

**Kawaii Fluid** is a high-performance, **2-way interactive viscous fluid system** for Unreal Engine 5.7. It simulates complex liquid dynamics in real-time by leveraging advanced GPU-based physics solvers and optimized data structures.

![Fluid Interaction](docs/media/main-logo.png)

## [Documentation](https://gtlbruteforce.github.io/KawaiiFluid/)

## Project Overview

Kawaii Fluid provides seamless interaction between fluids and dynamic actors. It achieves realistic viscosity and two-way coupling (force feedback) using state-of-the-art simulation techniques.

### Core Highlights:
*   **XPBD & XSPH Hybrid Solver**: Stable pressure and realistic internal friction.
*   **Massive Particle Count**: Tens of thousands of particles at real-time frame rates.
*   **Dynamic Interaction**: Real-time collision with Skeletal Meshes and character attachment.

## System Architecture

### KawaiiFluidRuntime
Responsible for the core physics simulation and rendering pipeline.

*   **Simulation Context**: Pure stateless solver logic for viscosity, surface tension, and density.
*   **GPU Pipeline**: Optimized Compute Shader architecture featuring **SoA (Structure of Arrays)** conversion for maximum bandwidth efficiency.
*   **Physics Solvers**: Hybrid **XPBD** and **XSPH** for stability and realism.
*   **Collision System**: Advanced 2-way dynamic interaction supporting **Bone Colliders** and **Character Attachment**.
*   **Rendering Pipeline**: High-quality Screen Space Fluid Rendering (SSFR).

## Core Technologies

### 1. Performance Optimization
*   **Bandwidth Efficiency**: Reduces memory overhead through SoA and Half-Precision packing.
*   **Z-Order Spatial Sorting**: Maximizes GPU cache hits by aligning spatial data with memory layout.

### 2. Screen Space Fluid Rendering
Features anisotropic smoothing to prevent "jaggies" on fluid surfaces and realistic optical effects like refraction and Jacobian caustics.

### 3. Dynamic 2-Way Interaction
Real-time feedback loop where actors receive impulses and fluid particles respond to skeletal movement using specialized **Boundary Tagging**.

## Technical Specs
*   **Solvers**: XPBD (Macklin 2016), XSPH (Macklin & Müller 2013).
*   **Rendering**: SSFR (Yu & Turk 2013).
*   **Optimization**: SoA, Half-Precision, Z-order Sorting.
*   **Engine**: Unreal Engine 5.7 (DirectX 12).

## License
Copyright 2026 Team_Bruteforce. All Rights Reserved.