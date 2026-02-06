# Getting Started

This guide covers everything from initial installation to your first fluid simulation setup.

## Installation

1.  **Plugin Setup:** Clone this repository into your project's `Plugins/KawaiiFluidSystem` folder.
2.  **Generate Files:** Right-click your `.uproject` file and select **Generate Visual Studio project files**.
3.  **Build:** Open your solution in Visual Studio 2022 and **Build** your project.

---

## Quick Start Guide

Follow these five steps to create your first fluid simulation domain and source.

### Step 1: Place a Kawaii Fluid Volume
Drag the **Kawaii Fluid Volume** from the Place Actors panel into your level. This defines the domain where the fluid will be simulated and rendered.

<video controls width="100%">
  <source src="media/volume-emitter-step1.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

### Step 2: Assign a Fluid Preset
Select the Volume and assign a **Preset** in the Details panel under *Fluid Volume > Preset*. This defines physical properties like viscosity and density. The default is `DA_KF_Water`.

<video controls width="100%">
  <source src="media/volume-emitter-step2.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

### Step 3: Place a Kawaii Fluid Emitter
Drag the **Kawaii Fluid Emitter** into the level, ensuring it is positioned **inside** the Volume bounds.

<video controls width="100%">
  <source src="media/volume-emitter-step3.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

### Step 4: Connect Emitter to Volume
Select the Emitter and set the **Target Volume** to your Volume actor. If left blank, it will attempt to find the nearest volume at BeginPlay.

<video controls width="100%">
  <source src="media/volume-emitter-step4.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

### Step 5: Play in Editor
Press **Play** to see the fluid simulation in action.

<video controls width="100%">
  <source src="media/volume-emitter-step5.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

---

## Critical Configuration Rules

To ensure stable simulation and correct interaction, always verify these three rules:

### 1. Volume and Preset Requirement
A Volume without a Preset will not initialize. Always ensure a valid `UKawaiiFluidPresetDataAsset` is assigned to provide the necessary physical parameters to the GPU solver.

### 2. Collision Setup
Static Meshes inside the volume act as obstacles. For optimal performance and reliability, ensure that custom meshes have **Simple Collision** generated in the Static Mesh Editor. The system uses these simple primitives for high-speed GPU collision detection.

### 3. Player Interaction (Auto Collider)
To enable real-time interaction with characters:
*   Add a `UKawaiiFluidSimulationComponent` to your character.
*   Enable **Auto Collider** in the component settings. This automatically synchronizes your character's **Physics Asset** with the GPU simulation.

---

## Quick Verification Checklist
*   [ ] Is there a **Kawaii Fluid Volume** with a **Preset** assigned?
*   [ ] Is the **Emitter** located inside the Volume bounds?
*   [ ] Does your character have the **Simulation Component** with **Auto Collider** enabled?

If all items are checked, your setup is complete.
