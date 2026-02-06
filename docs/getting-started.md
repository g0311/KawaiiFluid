# Getting Started

## Installation

1.  Clone this repository into your project's `Plugins/` folder.
2.  Regenerate Visual Studio project files.
3.  Build your project using Unreal Engine 5.7.

## Quick Setup

### 1. Add Simulation Component
Add the `KawaiiFluidSimulationComponent` to your Actor.

### 2. Configure Preset
Assign a `UKawaiiFluidPresetDataAsset` to the component to define fluid properties like viscosity and surface tension.

### 3. Place in World
Ensure a `KawaiiFluidSimulatorSubsystem` is active in your world (it should be automatic as a World Subsystem).
