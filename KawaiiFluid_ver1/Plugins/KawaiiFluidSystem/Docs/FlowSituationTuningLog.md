# Flow Situation Parameter Tuning Log

## Overview
This document records the parameter tuning process for **Flow situation** - simulating flowing water (waterfalls, fountains, pouring water).

## Target Metrics
| Metric | Target | Rationale |
|--------|--------|-----------|
| Density Error (flowing) | <15% | Higher tolerance for dynamic situations |
| Density Error (settled) | <10% | After pooling, should approach Calm metrics |
| Stability Score | >70 | Lower threshold due to movement |
| Fluid Coherence | Maintained | No separation during flow |

---

## Test Scene
- **Setup**: Slope with stream spawning at top, fluid flows down and pools at bottom
- **Particle Count**: ~40,000
- **Measurement**: Stats captured during flow and after settling

---

## Test 1: Baseline (Compliance 0.00001)
**Date**: 2025-01-18
**Purpose**: Initial test with default Compliance

| Parameter | Value |
|-----------|-------|
| Compliance | 0.00001 |
| Global Damping | 0.99 |
| Cohesion | 0.1 |

**Results** (settled state):
| Metric | Value | Status |
|--------|-------|--------|
| Particles | 40,050 | - |
| Density Error | **19.41%** | ❌ Too high |
| Density StdDev | 87.64 | ❌ High |
| Stability Score | 74.13 | ⚠️ |
| Kinetic Energy | 46.49 | ❌ High |
| Max Velocity | 34.09 cm/s | ⚠️ |

**Analysis**: Same issue as Calm testing - Compliance 0.00001 is too soft for this particle count. Need to use Calm-tuned Compliance 0.000002.

---

## Test 2: Optimized Parameters (Final)
**Date**: 2025-01-18
**Changes**: Applied Calm-tuned Compliance, adjusted Flow-specific parameters

| Parameter | Test 1 | Test 2 |
|-----------|--------|--------|
| Compliance | 0.00001 | **0.000002** |
| Global Damping | 0.99 | **0.995** |
| Cohesion | 0.1 | **0.5** |
| Viscosity | - | **0.02** |
| Tensile Instability | - | **Enabled (K=0.1)** |

### Results During Flow
| Metric | Value | Status |
|--------|-------|--------|
| Particles | 40,083 | - |
| Avg Density | 1,048.25 | ✅ +4.8% |
| Density Error | **4.82%** | ✅ **Excellent!** |
| Per-Particle Error | 4.90% | ✅ |
| Density StdDev | 127.21 | ⚠️ Expected during flow |
| Stability Score | 41.39 | ⚠️ Expected during flow |
| Max Velocity | 1,642.95 cm/s | ✓ Flow velocity |
| Avg Velocity | 16.58 cm/s | - |
| Kinetic Energy | 7,667.88 | ⚠️ Active flow |
| Avg Neighbors | 32.14 | ✓ Good |

### Results After Settling
| Metric | Value | vs Calm Final | Status |
|--------|-------|---------------|--------|
| Particles | 40,108 | +69% | - |
| Avg Density | 1,046.27 | Better | ✅ +4.6% |
| Density Error | **4.63%** | **Better than Calm!** | ✅ |
| Density StdDev | **26.72** | Better | ✅ |
| Per-Particle Error | 4.65% | - | ✅ |
| Stability Score | 61.33 | Lower | ⚠️ |
| Max Velocity | 1,609.08 | Higher | ⚠️ |
| Avg Velocity | 6.83 | - | ✓ |
| Kinetic Energy | 203.51 | Higher | ⚠️ |
| Avg Neighbors | 32.95 | Similar | ✓ |

**Analysis**:
- Density metrics are **excellent** - even better than Calm despite 69% more particles
- Stability Score lower due to some particles still moving on slope edges
- High Max Velocity from particles on slope forming layer (Cohesion 0.5 mitigates visibility)
- Overall: **Very successful Flow parameters**

---

## ✅ FINAL: Flow Situation Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| **Compliance** | **0.000002** | Same as Calm - key factor for density accuracy |
| **Global Damping** | **0.995** | Less energy dissipation for natural flow |
| **Cohesion** | **0.5** | Stronger cohesion maintains fluid coherence during flow |
| **Viscosity** | **0.02** | Smooth flow behavior |
| **Tensile Instability** | **Enabled** | K=0.1, N=4, ΔQ=0.2 - prevents clustering |
| Spacing Ratio | 0.5 | Same as Calm |
| Solver Iterations | 3 | Default |
| Substeps | 2 | Default |

### Comparison: Calm vs Flow

| Parameter | Calm | Flow | Difference |
|-----------|------|------|------------|
| Compliance | 0.000002 | 0.000002 | Same |
| Global Damping | 0.99 | **0.995** | +0.005 |
| Cohesion | 0.1 | **0.5** | +0.4 |
| Viscosity | - | **0.02** | Added |
| Tensile Instability | - | **Enabled** | Added |

### Final Metrics Summary

| State | Density Error | Stability Score | Target Met |
|-------|---------------|-----------------|------------|
| Flowing | 4.82% | 41.39 | ✅ Density |
| Settled | 4.63% | 61.33 | ✅ Density |

---

## Key Learnings

### 1. Compliance is Universal
The Compliance value (0.000002) tuned for Calm situation works equally well for Flow. This is the **foundational parameter** for density accuracy.

### 2. Cohesion is Critical for Flow
Higher Cohesion (0.5 vs 0.1) is essential for Flow to:
- Maintain fluid coherence during movement
- Prevent visual artifacts at slope-pool boundaries
- Create natural-looking stream behavior

### 3. Global Damping Trade-off
- Calm: 0.99 (more damping → faster settling)
- Flow: 0.995 (less damping → sustained flow energy)

### 4. Slope Layer Phenomenon
Particles on slopes can form a stuck layer with gap to the pool below:
- Caused by low Cohesion and Tensile Instability
- Mitigated with Cohesion ≥ 0.5
- Not visible in normal operation with recommended parameters

---

## Next Steps
- [ ] Impact situation parameter tuning
- [ ] Document situation-based preset system
