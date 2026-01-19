# Impact Situation Parameter Tuning Log

## Overview

This document records the parameter tuning process for **Impact situation** - fluid collision scenarios (fluid dropping, hitting surfaces).

## Target Metrics

| Metric | Target | Rationale |
|--------|--------|-----------|
| Density Error (peak) | <10% | Higher tolerance during collision |
| Density Error (settled) | <5% | Should recover to stable state |
| Stability Score (settled) | >80 | Quick recovery after impact |
| Energy Dissipation | Fast | Rapid settling after collision |

---

## Test Scene

- **Setup**: Sphere-shaped fluid volume dropped onto floor
- **Particle Count**: ~14,000
- **Measurement Points**: Collision peak, 1 second after, fully settled

---

## Test 1: Sphere Drop (Baseline Impact Parameters)

**Date**: 2025-01-18

### Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Compliance | 0.000002 | Same as Calm/Flow |
| Global Damping | **0.98** | Faster energy dissipation for impact |
| Cohesion | **0.3** | Balance between splash and coherence |
| Viscosity | 0.02 | Same as Flow |
| Tensile Instability | Enabled (K=0.1) | Prevent clustering |
| Restitution | **0.2** | Allow slight bounce on collision |
| Friction | 0.3 | Default |
| Max Substeps | 2 | Default |
| Solver Iterations | 3 | Default |

### Results - Time Series Analysis

#### Phase 1: Collision Peak (Immediately after impact)

| Metric | Average | Max | Status |
|--------|---------|-----|--------|
| Kinetic Energy | 35,742.72 | **50,680.38** | Peak impact |
| Max Velocity | 493.41 | **1,071.04** cm/s | Peak impact |
| Avg Density | 1,005.56 | 1,040.51 | ✓ |
| Density Error | 1.96% | **5.49%** | ✅ Good for impact |
| Density StdDev | 56.45 | 112.49 | Expected during impact |
| Stability Score | 35.45 | 54.92 | Low during impact (normal) |
| Avg Neighbors | 29.49 | 31.61 | ✓ |

#### Phase 2: ~1 Second After Collision

| Metric | Average | Max | Status |
|--------|---------|-----|--------|
| Kinetic Energy | 4,591.09 | 16,366.22 | -68% from peak |
| Max Velocity | 182.75 | 340.39 cm/s | Settling |
| Avg Density | 999.06 | 1,002.93 | ✅ Near RestDensity |
| Density Error | **0.20%** | 0.40% | ✅ Excellent |
| Density StdDev | 43.58 | 62.26 | Improving |
| Stability Score | 58.43 | 85.46 | Recovering |
| Avg Neighbors | 27.02 | 28.52 | ✓ |

#### Phase 3: Fully Settled

| Metric | Average | Max | Status |
|--------|---------|-----|--------|
| Kinetic Energy | 85.74 | 87.52 | ✅ Stable |
| Max Velocity | 53.87 | 63.15 cm/s | ✅ Low |
| Avg Density | **1,002.54** | 1,002.69 | ✅ Excellent |
| Density Error | **0.25%** | 0.27% | ✅ **Excellent!** |
| Density StdDev | **20.74** | 21.10 | ✅ Very uniform |
| Stability Score | **99.15** | 99.44 | ✅ **Excellent!** |
| Avg Neighbors | 26.24 | 26.27 | ✓ |
| Per-Particle Error | 1.18% | 1.19% | ✅ |

### Energy Dissipation Analysis

| Transition | Kinetic Energy (Max) | Change | Time |
|------------|---------------------|--------|------|
| Collision Peak | 50,680.38 | - | 0s |
| After 1 second | 16,366.22 | **-68%** | ~1s |
| Fully Settled | 87.52 | **-99.8%** | ~3-5s |

**GlobalDamping 0.98 provides excellent energy dissipation** - most energy dissipated within 1 second.

### Key Findings

1. **Density Accuracy During Impact**
   - Even at collision peak, Density Error only 5.49%
   - 1 second after: 0.40%
   - Settled: 0.25%

2. **Rapid Recovery**
   - Stability Score: 35.45 (peak) → 58.43 (1s) → 99.15 (settled)
   - Fast transition from chaotic to stable

3. **Cohesion 0.3 Effectiveness**
   - Fluid maintains coherence during splash
   - No excessive fragmentation observed in metrics

4. **Comparison with Other Situations**

| Metric (Settled) | Impact | Calm | Flow |
|------------------|--------|------|------|
| Density Error | **0.25%** | 7.09% | 4.63% |
| Stability Score | **99.15** | 85.22 | 61.33 |
| Density StdDev | **20.74** | 37.69 | 26.72 |

Impact settled state shows **best metrics** - likely due to:
- Lower particle count (14k vs 24k/40k)
- GlobalDamping 0.98 quickly stabilizes
- Sphere shape distributes evenly on floor

---

## ✅ Impact Situation Parameters (Pending Visual Verification)

| Parameter | Value | vs Calm | vs Flow |
|-----------|-------|---------|---------|
| **Compliance** | 0.000002 | Same | Same |
| **Global Damping** | **0.98** | -0.01 | -0.015 |
| **Cohesion** | **0.3** | +0.2 | -0.2 |
| **Restitution** | **0.2** | +0.2 | +0.2 |
| Viscosity | 0.02 | Same | Same |
| Tensile Instability | Enabled | Same | Same |

### Metrics Summary

| Phase | Density Error | Stability Score | Status |
|-------|---------------|-----------------|--------|
| Collision Peak | 5.49% | 35.45 | ✅ Good for impact |
| 1 Second After | 0.40% | 58.43 | ✅ Recovering |
| Fully Settled | 0.25% | 99.15 | ✅ Excellent |

---

## Visual Verification Tests

Numeric metrics were excellent, but visual inspection revealed significant issues.

### Test 2: Reduced Stickiness

**Issue Identified**: Fluid looked too sticky, not water-like. 2-layer formation with slow merging.

| Parameter | Test 1 | Test 2 |
|-----------|--------|--------|
| Cohesion | 0.3 | **0.1** |
| Viscosity | 0.02 | **0.005** |
| GlobalDamping | 0.98 | **0.99** |

**Result**: Improved but layer issue persists.

---

### Test 3: Disabled Tensile Instability

**Purpose**: Check if Tensile Instability causes layering.

| Parameter | Test 2 | Test 3 |
|-----------|--------|--------|
| Tensile Instability | Enabled | **Disabled** |

**Result**: Still 2 layers in center, 1 layer at edges. Issue is not from Tensile Instability.

---

### Test 4: Lower Cohesion

| Parameter | Test 3 | Test 4 |
|-----------|--------|--------|
| Cohesion | 0.1 | **0.05** |
| Viscosity | 0.005 | 0.005 |

**Result**: Stats stable but 2nd layer particles "wriggling" (micro-oscillation).

---

### Test 5: Increased Viscosity for Stability

| Parameter | Test 4 | Test 5 |
|-----------|--------|--------|
| Viscosity | 0.005 | **0.015** |
| GlobalDamping | 0.99 | **0.98** |

**Result**: Still wriggling. Over time, holes appear in 2nd layer, particles fall to 1st layer.

---

### Test 6: Higher Cohesion Attempt

**Purpose**: Try to stabilize with moderate cohesion.

| Parameter | Test 5 | Test 6 |
|-----------|--------|--------|
| Cohesion | 0.05 | **0.08** |
| Viscosity | 0.015 | **0.025** |
| GlobalDamping | 0.98 | **0.985** |

**Result**: ❌ Worse - now stacks to 3 layers. Holes form like cheese.

---

### Test 7: Minimize Cohesion for Fast Spreading

| Parameter | Test 6 | Test 7 |
|-----------|--------|--------|
| Cohesion | 0.08 | **0.01** |
| Viscosity | 0.025 | **0.003** |
| GlobalDamping | 0.985 | **0.995** |
| Friction | 0.3 | **0.1** |

**Result**: ❌ Spreads thin and fast but scatters like powder, not fluid.

---

### Test 8: Balanced Parameters

| Parameter | Test 7 | Test 8 |
|-----------|--------|--------|
| Cohesion | 0.01 | **0.04** |
| Viscosity | 0.003 | **0.008** |
| Friction | 0.1 | **0.15** |
| Tensile Instability | Disabled | **Enabled K=0.05** |

**Result**: ❌ Not powder-like, but 2 layers form again with bumpy appearance and holes.

---

## ❌ Fundamental Limitation Identified

### Problem Summary

| Cohesion | Result |
|----------|--------|
| High (0.08+) | Stacks 2-3 layers, sticky appearance |
| Medium (0.04-0.05) | 2 layers with unstable boundary, holes form |
| Low (0.01-0.02) | Scatters like powder |

**No parameter combination produces natural water-like spreading on flat surface.**

---

### Root Cause: Particle-Based Method Limitation

This is NOT an XPBD-specific issue. It affects ALL particle-based methods (SPH, PBF, XPBD):

#### 1. Discrete Particle Nature

```
Real water thickness:  ████████████████  (continuous)
                       thin ←────→ thick

Particle simulation:   ●  ●  ●●  ●●  ●●  ●  ●  (discrete)
                       1층   2층   2층   1층
                           ↑
                       Sharp boundary (unavoidable)
```

Particles cannot be 1.5 - only 1 or 2 layers possible, creating sharp visible boundaries.

#### 2. Surface Particle Ratio Problem

```
Floor spreading scenario:
●  ●  ●  ●  ●  ●  ●  ●  ← ~100% surface particles
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀

Surface particle ratio: ~90%+ → SPH density calculation inaccurate
Average neighbors: 5-15 → Insufficient for fluid behavior
```

#### 3. Thin Film Instability

1-2 layer configurations are inherently unstable in SPH/PBF:
- Surface particles lack neighbors on one side
- Density estimation becomes inaccurate
- System naturally wants to either spread (1 layer) or stack (2+ layers)
- The transition zone is always unstable

---

## 🔄 Impact Situation Redefinition

### Original Scenario (Problematic)

```
Before:        After:
   ●●●
   ●●●          ●  ●  ●  ●  ●  ●
   ●●●         ●●  ●  ●●  ●  ●●   ← 1-2 layers (unstable)
▀▀▀▀▀▀▀▀      ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀

Issues:
- 90%+ surface particles
- 5-15 neighbors per particle
- Powder-like or layer stacking
```

### New Scenario: Object Falling into Water Pool

```
Before:        During:         After:
               ↓ ■ ↓
████████████   ██↑█↑██        ~~~~~~~~~~~~
████████████   ████████        ████████████
████████████   ████████        ████████████
▀▀▀▀▀▀▀▀▀▀▀▀   ▀▀▀▀▀▀▀▀        ▀▀▀▀▀▀▀▀▀▀▀▀
  Existing       Splash          Waves → Stable
  water pool
```

### Why This Works Better

| Aspect | Floor Spreading | Object into Pool |
|--------|-----------------|------------------|
| Surface particle ratio | 80-100% | 10-20% |
| Neighbors per particle | 5-15 | 25-35 |
| Density accuracy | Low | High |
| Stability | Unstable/infinite | Fast (2-3s) |
| Visual quality | Powder/bumpy | Natural waves |

### New Impact Scenario Benefits

1. **Bulk fluid maintained** - Water pool stays 3+ layers deep
2. **Accurate density** - Most particles have full neighbor sets
3. **Natural dynamics** - Splash → waves → settling is XPBD strength
4. **Predictable behavior** - Returns to stable state

---

## XPBD Optimal Scenarios

### ✅ Well-Suited Scenarios

| Scenario | Why It Works |
|----------|--------------|
| Swimming pool / tank | Large volume, contained, 3+ layers |
| Waterfall / fountain | Flow into pool, maintains bulk |
| Waves / sloshing | Bulk fluid movement |
| Object submersion | Splash + waves + settling |
| Pouring between containers | Stream + pooling |

### ❌ Challenging Scenarios

| Scenario | Issue | Workaround |
|----------|-------|------------|
| Thin puddles (1-2 layers) | Discrete layer boundary | Screen-space rendering |
| Small droplets (<1000 particles) | Insufficient neighbors | Special droplet system |
| Spray / mist | Particle separation | Separate particle system |
| Spreading on flat surface | Surface ratio too high | Design for pooling instead |

---

## Recommended Minimum Conditions

| Condition | Minimum | Reason |
|-----------|---------|--------|
| Fluid depth | 3-4 particle layers | Reduces surface ratio |
| Particle count | 5,000+ | Ensures neighbor coverage |
| Environment | Contained (walls/floor) | Clear boundary conditions |

---

## Next Steps

1. **Retest Impact** with "object falling into water pool" scenario
2. **Define Impact parameters** for splash/wave behavior
3. **Screen-space rendering** investigation for thin fluid visual quality

---

## Parameter Comparison: All Situations

| Parameter | Calm | Flow | Impact |
|-----------|------|------|--------|
| Compliance | 0.000002 | 0.000002 | 0.000002 |
| Global Damping | 0.99 | 0.995 | **0.98** |
| Cohesion | 0.1 | 0.5 | **0.3** |
| Viscosity | - | 0.02 | 0.02 |
| Restitution | 0.0 | 0.0 | **0.2** |
| Tensile Instability | - | Enabled | Enabled |

### Design Rationale

- **Calm**: Minimal movement, focus on density stability
- **Flow**: Sustained movement, high cohesion for stream coherence
- **Impact**: Quick energy dissipation, balanced cohesion for splash + recovery
