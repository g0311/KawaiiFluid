// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/FluidParticle.h"
#include "FluidCollider.generated.h"

/**
 * 유체 콜라이더 베이스 클래스
 *
 * 유체 입자와 상호작용하는 충돌체의 기본 인터페이스
 */
UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent))
class KAWAIIFLUIDRUNTIME_API UFluidCollider : public UActorComponent
{
	GENERATED_BODY()

public:
	UFluidCollider();

	/** 콜라이더 활성화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider")
	bool bColliderEnabled;

	/** 마찰 계수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Friction;

	/** 반발 계수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Restitution;

	/** 접착 가능 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider")
	bool bAllowAdhesion;

	/** 접착력 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider", meta = (ClampMin = "0.0", ClampMax = "2.0", EditCondition = "bAllowAdhesion"))
	float AdhesionMultiplier;

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	bool IsColliderEnabled() const { return bColliderEnabled; }

	virtual void ResolveCollisions(TArray<FFluidParticle>& Particles);

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	virtual bool GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const;

	UFUNCTION(BlueprintCallable, Category = "Fluid Collider")
	virtual bool IsPointInside(const FVector& Point) const;

protected:
	virtual void BeginPlay() override;
	virtual void ResolveParticleCollision(FFluidParticle& Particle);
};
