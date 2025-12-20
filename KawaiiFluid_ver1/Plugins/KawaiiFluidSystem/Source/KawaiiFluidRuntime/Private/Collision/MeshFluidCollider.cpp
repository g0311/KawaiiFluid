// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Collision/MeshFluidCollider.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"

UMeshFluidCollider::UMeshFluidCollider()
{
	TargetMeshComponent = nullptr;
	bAutoFindMesh = true;
	bUseSimplifiedCollision = true;
	CollisionMargin = 1.0f;
}

void UMeshFluidCollider::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindMesh)
	{
		AutoFindMeshComponent();
	}
}

void UMeshFluidCollider::AutoFindMeshComponent()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
	if (Capsule)
	{
		TargetMeshComponent = Capsule;
		return;
	}

	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (SkelMesh)
	{
		TargetMeshComponent = SkelMesh;
		return;
	}

	UStaticMeshComponent* StaticMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
	if (StaticMesh)
	{
		TargetMeshComponent = StaticMesh;
		return;
	}
}

bool UMeshFluidCollider::GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const
{
	if (!TargetMeshComponent)
	{
		return false;
	}

	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(TargetMeshComponent);
	if (Capsule)
	{
		FVector CapsuleCenter = Capsule->GetComponentLocation();
		float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
		float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		FVector CapsuleUp = Capsule->GetUpVector();

		FVector ToPoint = Point - CapsuleCenter;
		float AxisProjection = FVector::DotProduct(ToPoint, CapsuleUp);

		float ClampedProjection = FMath::Clamp(AxisProjection, -CapsuleHalfHeight + CapsuleRadius, CapsuleHalfHeight - CapsuleRadius);
		FVector ClosestOnAxis = CapsuleCenter + CapsuleUp * ClampedProjection;

		FVector RadialVector = Point - ClosestOnAxis;
		float RadialDistance = RadialVector.Size();

		if (RadialDistance < KINDA_SMALL_NUMBER)
		{
			OutNormal = FVector::ForwardVector;
			OutClosestPoint = ClosestOnAxis + OutNormal * CapsuleRadius;
			OutDistance = CapsuleRadius;
		}
		else
		{
			OutNormal = RadialVector / RadialDistance;
			OutClosestPoint = ClosestOnAxis + OutNormal * CapsuleRadius;
			OutDistance = RadialDistance - CapsuleRadius;
		}

		return true;
	}

	FBoxSphereBounds Bounds = TargetMeshComponent->Bounds;
	FVector BoxCenter = Bounds.Origin;
	FVector BoxExtent = Bounds.BoxExtent;

	FVector LocalPoint = Point - BoxCenter;
	FVector ClampedPoint;
	ClampedPoint.X = FMath::Clamp(LocalPoint.X, -BoxExtent.X, BoxExtent.X);
	ClampedPoint.Y = FMath::Clamp(LocalPoint.Y, -BoxExtent.Y, BoxExtent.Y);
	ClampedPoint.Z = FMath::Clamp(LocalPoint.Z, -BoxExtent.Z, BoxExtent.Z);

	OutClosestPoint = BoxCenter + ClampedPoint;
	FVector ToPointVec = Point - OutClosestPoint;
	OutDistance = ToPointVec.Size();
	OutNormal = OutDistance > KINDA_SMALL_NUMBER ? ToPointVec / OutDistance : FVector::UpVector;

	return true;
}

bool UMeshFluidCollider::IsPointInside(const FVector& Point) const
{
	if (!TargetMeshComponent)
	{
		return false;
	}

	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(TargetMeshComponent);
	if (Capsule)
	{
		FVector CapsuleCenter = Capsule->GetComponentLocation();
		float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
		float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		FVector CapsuleUp = Capsule->GetUpVector();

		FVector ToPoint = Point - CapsuleCenter;
		float AxisProjection = FVector::DotProduct(ToPoint, CapsuleUp);

		if (FMath::Abs(AxisProjection) > CapsuleHalfHeight)
		{
			FVector SphereCenter = CapsuleCenter + CapsuleUp * FMath::Sign(AxisProjection) * (CapsuleHalfHeight - CapsuleRadius);
			return FVector::DistSquared(Point, SphereCenter) <= CapsuleRadius * CapsuleRadius;
		}
		else
		{
			FVector ClosestOnAxis = CapsuleCenter + CapsuleUp * AxisProjection;
			return FVector::DistSquared(Point, ClosestOnAxis) <= CapsuleRadius * CapsuleRadius;
		}
	}

	FBoxSphereBounds Bounds = TargetMeshComponent->Bounds;
	return Bounds.GetBox().IsInside(Point);
}
