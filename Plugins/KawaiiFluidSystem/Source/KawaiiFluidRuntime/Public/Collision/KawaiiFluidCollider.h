// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/KawaiiFluidParticle.h"
#include "KawaiiFluidCollider.generated.h"

/**
 * @brief Base class for fluid colliders.
 * @details Provides the base interface for collision objects that interact with fluid particles.
 * @param bColliderEnabled Whether the collider is currently active
 * @param Friction Friction coefficient (0 = no friction, 1 = maximum friction)
 * @param Restitution Restitution coefficient (0 = no bounce, 1 = full elastic bounce)
 */
UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent))
class KAWAIIFLUIDRUNTIME_API UKawaiiFluidCollider : public UActorComponent
{
	GENERATED_BODY()

public:
	UKawaiiFluidCollider();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider")
	bool bColliderEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Friction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Restitution;

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	bool IsColliderEnabled() const { return bColliderEnabled; }

	virtual void ResolveCollisions(TArray<FKawaiiFluidParticle>& Particles, float SubstepDT);

	virtual void CacheCollisionShapes() {}

	virtual FBox GetCachedBounds() const { return FBox(ForceInit); }

	virtual bool IsCacheValid() const { return false; }

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	virtual bool GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	virtual float GetSignedDistance(const FVector& Point, FVector& OutGradient) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	virtual bool GetClosestPointWithBone(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance, FName& OutBoneName, FTransform& OutBoneTransform) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	virtual bool IsPointInside(const FVector& Point) const;

protected:
	virtual void BeginPlay() override;

	virtual void ResolveParticleCollision(FKawaiiFluidParticle& Particle, float SubstepDT);
};