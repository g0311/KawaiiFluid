// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Collision/MeshFluidCollider.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

UMeshFluidCollider::UMeshFluidCollider()
{
	TargetMeshComponent = nullptr;
	bAutoFindMesh = true;
	bUseSimplifiedCollision = true;
	CollisionMargin = 1.0f;
	bCacheValid = false;
	CachedBounds = FBox(ForceInit);
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

	// 1순위: SkeletalMeshComponent (PhysicsAsset 기반 정밀 충돌)
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (SkelMesh)
	{
		TargetMeshComponent = SkelMesh;

		// 디버그: PhysicsAsset 정보 출력
		UPhysicsAsset* PhysAsset = SkelMesh->GetPhysicsAsset();
		if (PhysAsset)
		{
			int32 TotalCapsules = 0;
			for (USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
			{
				if (BodySetup)
				{
					TotalCapsules += BodySetup->AggGeom.SphylElems.Num();
				}
			}
			//UE_LOG(LogTemp, Warning, TEXT("MeshFluidCollider: Found SkeletalMesh with PhysicsAsset '%s', Bodies: %d, Total Capsules: %d"),*PhysAsset->GetName(), PhysAsset->SkeletalBodySetups.Num(), TotalCapsules);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("MeshFluidCollider: Found SkeletalMesh but NO PhysicsAsset!"));
		}
		return;
	}

	// 2순위: CapsuleComponent (단순 캡슐 충돌)
	UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
	if (Capsule)
	{
		TargetMeshComponent = Capsule;
		//UE_LOG(LogTemp, Warning, TEXT("MeshFluidCollider: Using CapsuleComponent"));
		return;
	}

	// 3순위: StaticMeshComponent
	UStaticMeshComponent* StaticMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
	if (StaticMesh)
	{
		TargetMeshComponent = StaticMesh;
		//UE_LOG(LogTemp, Warning, TEXT("MeshFluidCollider: Using StaticMeshComponent"));
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("MeshFluidCollider: No mesh component found!"));
}

void UMeshFluidCollider::CacheCollisionShapes()
{
	CachedCapsules.Reset();
	CachedSpheres.Reset();
	CachedBounds = FBox(ForceInit);
	bCacheValid = false;

	if (!TargetMeshComponent)
	{
		return;
	}

	// CapsuleComponent인 경우
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(TargetMeshComponent);
	if (Capsule)
	{
		FVector CapsuleCenter = Capsule->GetComponentLocation();
		float CapsuleRadius = Capsule->GetScaledCapsuleRadius() + CollisionMargin;
		float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		FVector CapsuleUp = Capsule->GetUpVector();

		float HalfLength = CapsuleHalfHeight - CapsuleRadius;
		FCachedCapsule CachedCap;
		CachedCap.Start = CapsuleCenter - CapsuleUp * HalfLength;
		CachedCap.End = CapsuleCenter + CapsuleUp * HalfLength;
		CachedCap.Radius = CapsuleRadius;
		CachedCapsules.Add(CachedCap);

		CachedBounds += CachedCap.Start;
		CachedBounds += CachedCap.End;
		CachedBounds = CachedBounds.ExpandBy(CapsuleRadius);
		bCacheValid = true;
		return;
	}

	// SkeletalMeshComponent인 경우 - PhysicsAsset 사용
	USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(TargetMeshComponent);
	if (SkelMesh)
	{
		UPhysicsAsset* PhysAsset = SkelMesh->GetPhysicsAsset();
		if (PhysAsset)
		{
			for (USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
			{
				if (!BodySetup)
				{
					continue;
				}

				int32 BoneIndex = SkelMesh->GetBoneIndex(BodySetup->BoneName);
				if (BoneIndex == INDEX_NONE)
				{
					continue;
				}

				FTransform BoneTransform = SkelMesh->GetBoneTransform(BoneIndex);

				// 캡슐 캐싱
				for (const FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
				{
					FTransform CapsuleLocalTransform = SphylElem.GetTransform();
					FTransform CapsuleWorldTransform = CapsuleLocalTransform * BoneTransform;

					FVector CapsuleCenter = CapsuleWorldTransform.GetLocation();
					float CapsuleRadius = SphylElem.Radius + CollisionMargin;
					float CapsuleLength = SphylElem.Length;
					FVector CapsuleUp = CapsuleWorldTransform.GetRotation().GetUpVector();

					float HalfLength = CapsuleLength * 0.5f;

					FCachedCapsule CachedCap;
					CachedCap.Start = CapsuleCenter - CapsuleUp * HalfLength;
					CachedCap.End = CapsuleCenter + CapsuleUp * HalfLength;
					CachedCap.Radius = CapsuleRadius;
					CachedCap.BoneName = BodySetup->BoneName;
					CachedCap.BoneTransform = BoneTransform;
					CachedCapsules.Add(CachedCap);

					CachedBounds += CachedCap.Start;
					CachedBounds += CachedCap.End;
				}

				// 스피어 캐싱
				for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
				{
					FTransform SphereLocalTransform = SphereElem.GetTransform();
					FTransform SphereWorldTransform = SphereLocalTransform * BoneTransform;

					FCachedSphere CachedSph;
					CachedSph.Center = SphereWorldTransform.GetLocation();
					CachedSph.Radius = SphereElem.Radius + CollisionMargin;
					CachedSph.BoneName = BodySetup->BoneName;
					CachedSph.BoneTransform = BoneTransform;
					CachedSpheres.Add(CachedSph);

					CachedBounds += CachedSph.Center;
				}
			}

			if (CachedCapsules.Num() > 0 || CachedSpheres.Num() > 0)
			{
				// 가장 큰 반경으로 바운딩 박스 확장
				float MaxRadius = CollisionMargin;
				for (const FCachedCapsule& Cap : CachedCapsules)
				{
					MaxRadius = FMath::Max(MaxRadius, Cap.Radius);
				}
				for (const FCachedSphere& Sph : CachedSpheres)
				{
					MaxRadius = FMath::Max(MaxRadius, Sph.Radius);
				}
				CachedBounds = CachedBounds.ExpandBy(MaxRadius);
				bCacheValid = true;
				return;
			}
		}
	}

	// 폴백: 바운딩 박스 사용
	FBoxSphereBounds Bounds = TargetMeshComponent->Bounds;
	CachedBounds = Bounds.GetBox();
	bCacheValid = true;
}

bool UMeshFluidCollider::GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const
{
	if (!bCacheValid)
	{
		return false;
	}

	// AABB 컬링: 바운딩 박스 밖이면 스킵
	const float CullingMargin = 50.0f;  // 충돌 마진 + 여유
	if (!CachedBounds.ExpandBy(CullingMargin).IsInside(Point))
	{
		return false;
	}

	float MinDistance = TNumericLimits<float>::Max();
	bool bFoundAny = false;

	// 캐싱된 캡슐들 순회
	for (const FCachedCapsule& Cap : CachedCapsules)
	{
		// 캡슐 축에서 가장 가까운 점 찾기
		FVector SegmentDir = Cap.End - Cap.Start;
		float SegmentLengthSq = SegmentDir.SizeSquared();

		FVector ClosestOnAxis;
		if (SegmentLengthSq < KINDA_SMALL_NUMBER)
		{
			ClosestOnAxis = Cap.Start;
		}
		else
		{
			float t = FVector::DotProduct(Point - Cap.Start, SegmentDir) / SegmentLengthSq;
			t = FMath::Clamp(t, 0.0f, 1.0f);
			ClosestOnAxis = Cap.Start + SegmentDir * t;
		}

		FVector ToPointVec = Point - ClosestOnAxis;
		float DistToAxis = ToPointVec.Size();

		FVector TempNormal;
		FVector TempClosestPoint;
		float TempDistance;

		if (DistToAxis < KINDA_SMALL_NUMBER)
		{
			TempNormal = FVector::ForwardVector;
			TempClosestPoint = ClosestOnAxis + TempNormal * Cap.Radius;
			TempDistance = -Cap.Radius;
		}
		else
		{
			TempNormal = ToPointVec / DistToAxis;
			TempClosestPoint = ClosestOnAxis + TempNormal * Cap.Radius;
			TempDistance = DistToAxis - Cap.Radius;
		}

		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance;
			OutClosestPoint = TempClosestPoint;
			OutNormal = TempNormal;
			OutDistance = TempDistance;
			bFoundAny = true;
		}
	}

	// 캐싱된 스피어들 순회
	for (const FCachedSphere& Sph : CachedSpheres)
	{
		FVector ToPointVec = Point - Sph.Center;
		float DistToCenter = ToPointVec.Size();

		FVector TempNormal;
		FVector TempClosestPoint;
		float TempDistance;

		if (DistToCenter < KINDA_SMALL_NUMBER)
		{
			TempNormal = FVector::UpVector;
			TempClosestPoint = Sph.Center + TempNormal * Sph.Radius;
			TempDistance = -Sph.Radius;
		}
		else
		{
			TempNormal = ToPointVec / DistToCenter;
			TempClosestPoint = Sph.Center + TempNormal * Sph.Radius;
			TempDistance = DistToCenter - Sph.Radius;
		}

		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance;
			OutClosestPoint = TempClosestPoint;
			OutNormal = TempNormal;
			OutDistance = TempDistance;
			bFoundAny = true;
		}
	}

	// 캐싱된 데이터가 없으면 바운딩 박스 사용
	if (!bFoundAny && TargetMeshComponent)
	{
		FVector BoxCenter = CachedBounds.GetCenter();
		FVector BoxExtent = CachedBounds.GetExtent();

		FVector LocalPoint = Point - BoxCenter;
		FVector ClampedPoint;
		ClampedPoint.X = FMath::Clamp(LocalPoint.X, -BoxExtent.X, BoxExtent.X);
		ClampedPoint.Y = FMath::Clamp(LocalPoint.Y, -BoxExtent.Y, BoxExtent.Y);
		ClampedPoint.Z = FMath::Clamp(LocalPoint.Z, -BoxExtent.Z, BoxExtent.Z);

		OutClosestPoint = BoxCenter + ClampedPoint;
		FVector ToPointVec = Point - OutClosestPoint;
		OutDistance = ToPointVec.Size();
		OutNormal = OutDistance > KINDA_SMALL_NUMBER ? ToPointVec / OutDistance : FVector::UpVector;
		bFoundAny = true;
	}

	return bFoundAny;
}

bool UMeshFluidCollider::GetClosestPointWithBone(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance, FName& OutBoneName, FTransform& OutBoneTransform) const
{
	if (!bCacheValid)
	{
		OutBoneName = NAME_None;
		OutBoneTransform = FTransform::Identity;
		return false;
	}

	// AABB 컬링: 바운딩 박스 밖이면 스킵
	const float CullingMargin = 50.0f;
	if (!CachedBounds.ExpandBy(CullingMargin).IsInside(Point))
	{
		OutBoneName = NAME_None;
		OutBoneTransform = FTransform::Identity;
		return false;
	}

	float MinDistance = TNumericLimits<float>::Max();
	bool bFoundAny = false;

	// 캐싱된 캡슐들 순회
	for (const FCachedCapsule& Cap : CachedCapsules)
	{
		FVector SegmentDir = Cap.End - Cap.Start;
		float SegmentLengthSq = SegmentDir.SizeSquared();

		FVector ClosestOnAxis;
		if (SegmentLengthSq < KINDA_SMALL_NUMBER)
		{
			ClosestOnAxis = Cap.Start;
		}
		else
		{
			float t = FVector::DotProduct(Point - Cap.Start, SegmentDir) / SegmentLengthSq;
			t = FMath::Clamp(t, 0.0f, 1.0f);
			ClosestOnAxis = Cap.Start + SegmentDir * t;
		}

		FVector ToPointVec = Point - ClosestOnAxis;
		float DistToAxis = ToPointVec.Size();

		FVector TempNormal;
		FVector TempClosestPoint;
		float TempDistance;

		if (DistToAxis < KINDA_SMALL_NUMBER)
		{
			TempNormal = FVector::ForwardVector;
			TempClosestPoint = ClosestOnAxis + TempNormal * Cap.Radius;
			TempDistance = -Cap.Radius;
		}
		else
		{
			TempNormal = ToPointVec / DistToAxis;
			TempClosestPoint = ClosestOnAxis + TempNormal * Cap.Radius;
			TempDistance = DistToAxis - Cap.Radius;
		}

		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance;
			OutClosestPoint = TempClosestPoint;
			OutNormal = TempNormal;
			OutDistance = TempDistance;
			OutBoneName = Cap.BoneName;
			OutBoneTransform = Cap.BoneTransform;
			bFoundAny = true;
		}
	}

	// 캐싱된 스피어들 순회
	for (const FCachedSphere& Sph : CachedSpheres)
	{
		FVector ToPointVec = Point - Sph.Center;
		float DistToCenter = ToPointVec.Size();

		FVector TempNormal;
		FVector TempClosestPoint;
		float TempDistance;

		if (DistToCenter < KINDA_SMALL_NUMBER)
		{
			TempNormal = FVector::UpVector;
			TempClosestPoint = Sph.Center + TempNormal * Sph.Radius;
			TempDistance = -Sph.Radius;
		}
		else
		{
			TempNormal = ToPointVec / DistToCenter;
			TempClosestPoint = Sph.Center + TempNormal * Sph.Radius;
			TempDistance = DistToCenter - Sph.Radius;
		}

		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance;
			OutClosestPoint = TempClosestPoint;
			OutNormal = TempNormal;
			OutDistance = TempDistance;
			OutBoneName = Sph.BoneName;
			OutBoneTransform = Sph.BoneTransform;
			bFoundAny = true;
		}
	}

	// 캐싱된 데이터가 없으면 바운딩 박스 사용 (본 정보 없음)
	if (!bFoundAny && TargetMeshComponent)
	{
		FVector BoxCenter = CachedBounds.GetCenter();
		FVector BoxExtent = CachedBounds.GetExtent();

		FVector LocalPoint = Point - BoxCenter;
		FVector ClampedPoint;
		ClampedPoint.X = FMath::Clamp(LocalPoint.X, -BoxExtent.X, BoxExtent.X);
		ClampedPoint.Y = FMath::Clamp(LocalPoint.Y, -BoxExtent.Y, BoxExtent.Y);
		ClampedPoint.Z = FMath::Clamp(LocalPoint.Z, -BoxExtent.Z, BoxExtent.Z);

		OutClosestPoint = BoxCenter + ClampedPoint;
		FVector ToPointVec = Point - OutClosestPoint;
		OutDistance = ToPointVec.Size();
		OutNormal = OutDistance > KINDA_SMALL_NUMBER ? ToPointVec / OutDistance : FVector::UpVector;
		OutBoneName = NAME_None;
		OutBoneTransform = FTransform::Identity;
		bFoundAny = true;
	}

	return bFoundAny;
}

bool UMeshFluidCollider::IsPointInside(const FVector& Point) const
{
	if (!TargetMeshComponent)
	{
		return false;
	}

	// CapsuleComponent인 경우
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

	// SkeletalMeshComponent인 경우 - PhysicsAsset 사용
	USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(TargetMeshComponent);
	if (SkelMesh)
	{
		UPhysicsAsset* PhysAsset = SkelMesh->GetPhysicsAsset();
		if (PhysAsset)
		{
			for (USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
			{
				if (!BodySetup)
				{
					continue;
				}

				int32 BoneIndex = SkelMesh->GetBoneIndex(BodySetup->BoneName);
				if (BoneIndex == INDEX_NONE)
				{
					continue;
				}

				FTransform BoneTransform = SkelMesh->GetBoneTransform(BoneIndex);

				// PhysicsAsset의 Sphyl(캡슐) 요소들 처리
				for (const FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
				{
					FTransform CapsuleLocalTransform = SphylElem.GetTransform();
					FTransform CapsuleWorldTransform = CapsuleLocalTransform * BoneTransform;

					FVector CapsuleCenter = CapsuleWorldTransform.GetLocation();
					float CapsuleRadius = SphylElem.Radius + CollisionMargin;
					float CapsuleLength = SphylElem.Length;
					FVector CapsuleUp = CapsuleWorldTransform.GetRotation().GetUpVector();

					float HalfLength = CapsuleLength * 0.5f;
					FVector CapsuleStart = CapsuleCenter - CapsuleUp * HalfLength;
					FVector CapsuleEnd = CapsuleCenter + CapsuleUp * HalfLength;

					// 캡슐 축에서 가장 가까운 점 찾기
					FVector SegmentDir = CapsuleEnd - CapsuleStart;
					float SegmentLengthSq = SegmentDir.SizeSquared();

					FVector ClosestOnAxis;
					if (SegmentLengthSq < KINDA_SMALL_NUMBER)
					{
						ClosestOnAxis = CapsuleStart;
					}
					else
					{
						float t = FVector::DotProduct(Point - CapsuleStart, SegmentDir) / SegmentLengthSq;
						t = FMath::Clamp(t, 0.0f, 1.0f);
						ClosestOnAxis = CapsuleStart + SegmentDir * t;
					}

					float DistSq = FVector::DistSquared(Point, ClosestOnAxis);
					if (DistSq <= CapsuleRadius * CapsuleRadius)
					{
						return true;
					}
				}

				// PhysicsAsset의 Sphere 요소들 처리
				for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
				{
					FTransform SphereLocalTransform = SphereElem.GetTransform();
					FTransform SphereWorldTransform = SphereLocalTransform * BoneTransform;

					FVector SphereCenter = SphereWorldTransform.GetLocation();
					float SphereRadius = SphereElem.Radius + CollisionMargin;

					float DistSq = FVector::DistSquared(Point, SphereCenter);
					if (DistSq <= SphereRadius * SphereRadius)
					{
						return true;
					}
				}
			}

			return false;
		}
	}

	// 폴백: 바운딩 박스 사용
	FBoxSphereBounds Bounds = TargetMeshComponent->Bounds;
	return Bounds.GetBox().IsInside(Point);
}
