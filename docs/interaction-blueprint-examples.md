# Fluid Interaction Blueprint Examples

This guide provides examples of how to handle fluid interaction events using Blueprints.

## Bone Collision Event

![Bone Collision](media/interaction-bone-collision.png)

## Resistance

![Resistance 1](media/interaction-resistance-1.png)

![Resistance 2](media/interaction-resistance-2.png)

Resistance is calculated for Actor movement (e.g., movement is slowed in water, and significantly more in Lava due to its higher density).

## Bone Collision Event by Force and Velocity

![Bone Force 1](media/interaction-bone-force-1.png)

![Bone Force 2](media/interaction-bone-force-2.png)

![Bone Force 3](media/interaction-bone-force-3.png)

In this example, the "Monitored Bones" are registered in the Details panel. The system returns the force applied to those specific bones by the fluid particles.

## Bone Collision Event by Specific Bone and Location

![Bone Location 1](media/interaction-bone-location-1.png)

![Bone Location 2](media/interaction-bone-location-2.png)

This is used for triggering events at precise local locations on a specific bone. It returns collision values that can be used to trigger gameplay events.
