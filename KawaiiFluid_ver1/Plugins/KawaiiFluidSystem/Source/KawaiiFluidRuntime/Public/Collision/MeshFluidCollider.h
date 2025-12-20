// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Collision/FluidCollider.h"
#include "MeshFluidCollider.generated.h"

/**
 * 메시 기반 유체 콜라이더
 * 캐릭터나 복잡한 형태의 오브젝트와 상호작용
 */
UCLASS(ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent))
class KAWAIIFLUIDRUNTIME_API UMeshFluidCollider : public UFluidCollider
{
	GENERATED_BODY()

public:
	UMeshFluidCollider();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider|Mesh")
	UPrimitiveComponent* TargetMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider|Mesh")
	bool bAutoFindMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider|Mesh")
	bool bUseSimplifiedCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Collider|Mesh")
	float CollisionMargin;

	virtual bool GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const override;
	virtual bool IsPointInside(const FVector& Point) const override;

protected:
	virtual void BeginPlay() override;

private:
	void AutoFindMeshComponent();
};
