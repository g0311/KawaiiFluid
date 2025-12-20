// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Collision/BoxFluidCollider.h"

UBoxFluidCollider::UBoxFluidCollider()
{
	BoxExtent = FVector(50.0f, 50.0f, 50.0f);
	LocalOffset = FVector::ZeroVector;
}

bool UBoxFluidCollider::GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	FVector LocalPoint = WorldToLocal(Point);

	FVector ClampedPoint;
	ClampedPoint.X = FMath::Clamp(LocalPoint.X, -BoxExtent.X, BoxExtent.X);
	ClampedPoint.Y = FMath::Clamp(LocalPoint.Y, -BoxExtent.Y, BoxExtent.Y);
	ClampedPoint.Z = FMath::Clamp(LocalPoint.Z, -BoxExtent.Z, BoxExtent.Z);

	if (LocalPoint == ClampedPoint)
	{
		float DistX = BoxExtent.X - FMath::Abs(LocalPoint.X);
		float DistY = BoxExtent.Y - FMath::Abs(LocalPoint.Y);
		float DistZ = BoxExtent.Z - FMath::Abs(LocalPoint.Z);

		if (DistX <= DistY && DistX <= DistZ)
		{
			ClampedPoint.X = (LocalPoint.X > 0) ? BoxExtent.X : -BoxExtent.X;
		}
		else if (DistY <= DistX && DistY <= DistZ)
		{
			ClampedPoint.Y = (LocalPoint.Y > 0) ? BoxExtent.Y : -BoxExtent.Y;
		}
		else
		{
			ClampedPoint.Z = (LocalPoint.Z > 0) ? BoxExtent.Z : -BoxExtent.Z;
		}
	}

	FVector LocalNormal = LocalPoint - ClampedPoint;
	float LocalDistance = LocalNormal.Size();

	if (LocalDistance < KINDA_SMALL_NUMBER)
	{
		if (FMath::Abs(ClampedPoint.X) >= BoxExtent.X - KINDA_SMALL_NUMBER)
		{
			LocalNormal = FVector(FMath::Sign(ClampedPoint.X), 0, 0);
		}
		else if (FMath::Abs(ClampedPoint.Y) >= BoxExtent.Y - KINDA_SMALL_NUMBER)
		{
			LocalNormal = FVector(0, FMath::Sign(ClampedPoint.Y), 0);
		}
		else
		{
			LocalNormal = FVector(0, 0, FMath::Sign(ClampedPoint.Z));
		}
	}
	else
	{
		LocalNormal /= LocalDistance;
	}

	OutClosestPoint = LocalToWorld(ClampedPoint);
	OutNormal = Owner->GetActorRotation().RotateVector(LocalNormal);
	OutDistance = LocalDistance;

	return true;
}

bool UBoxFluidCollider::IsPointInside(const FVector& Point) const
{
	FVector LocalPoint = WorldToLocal(Point);

	return FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
	       FMath::Abs(LocalPoint.Y) <= BoxExtent.Y &&
	       FMath::Abs(LocalPoint.Z) <= BoxExtent.Z;
}

FVector UBoxFluidCollider::WorldToLocal(const FVector& WorldPoint) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return WorldPoint;
	}

	FVector Center = GetBoxCenter();
	FVector RelativePoint = WorldPoint - Center;

	return Owner->GetActorRotation().UnrotateVector(RelativePoint);
}

FVector UBoxFluidCollider::LocalToWorld(const FVector& LocalPoint) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return LocalPoint;
	}

	FVector RotatedPoint = Owner->GetActorRotation().RotateVector(LocalPoint);
	return RotatedPoint + GetBoxCenter();
}

FVector UBoxFluidCollider::GetBoxCenter() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return LocalOffset;
	}

	return Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(LocalOffset);
}
