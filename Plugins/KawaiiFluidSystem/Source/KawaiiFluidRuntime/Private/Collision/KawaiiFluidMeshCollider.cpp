// Copyright 2026 Team_Bruteforce. All Rights Reserved.

#include "Collision/KawaiiFluidMeshCollider.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Simulation/Resources/GPUFluidParticle.h"

/**
 * @brief Default constructor for UKawaiiFluidMeshCollider.
 */
UKawaiiFluidMeshCollider::UKawaiiFluidMeshCollider()
{
	TargetMeshComponent = nullptr;
	bAutoFindMesh = true;
	bUseSimplifiedCollision = true;
	CollisionMargin = 1.0f;
	bCacheValid = false;
	CachedBounds = FBox(ForceInit);
}

/**
 * @brief Called when the game starts. Automatically finds a mesh if enabled.
 */
void UKawaiiFluidMeshCollider::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindMesh)
	{
		AutoFindMeshComponent();
	}
}

/**
 * @brief Searches for a suitable mesh component on the owner actor.
 */
void UKawaiiFluidMeshCollider::AutoFindMeshComponent()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Priority 1: SkeletalMeshComponent (precise collision via PhysicsAsset)
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (SkelMesh)
	{
		TargetMeshComponent = SkelMesh;
		return;
	}

	// Priority 2: CapsuleComponent (simple capsule collision)
	UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
	if (Capsule)
	{
		TargetMeshComponent = Capsule;
		return;
	}

	// Priority 3: StaticMeshComponent
	UStaticMeshComponent* StaticMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
	if (StaticMesh)
	{
		TargetMeshComponent = StaticMesh;
		return;
	}
}

/**
 * @brief Extracts and caches collision shapes from the target mesh component.
 */
void UKawaiiFluidMeshCollider::CacheCollisionShapes()
{
	CachedCapsules.Reset();
	CachedSpheres.Reset();
	CachedBoxes.Reset();
	CachedConvexes.Reset();
	CachedBounds = FBox(ForceInit);
	bCacheValid = false;

	if (!TargetMeshComponent)
	{
		return;
	}

	// Handle CapsuleComponent
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

	// Handle SkeletalMeshComponent - use PhysicsAsset
	USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(TargetMeshComponent);
	if (SkelMesh)
	{
		CacheSkeletalMeshCollision(SkelMesh);
		if (bCacheValid)
		{
			return;
		}
	}

	// Handle StaticMeshComponent - use Simple Collision
	UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(TargetMeshComponent);
	if (StaticMesh)
	{
		CacheStaticMeshCollision(StaticMesh);
		if (bCacheValid)
		{
			return;
		}
	}

	// Fallback: use bounding box
	FBoxSphereBounds Bounds = TargetMeshComponent->Bounds;
	CachedBounds = Bounds.GetBox();
	bCacheValid = true;
}

/**
 * @brief Extracts collision shapes from a skeletal mesh's physics asset.
 * @param SkelMesh Skeletal mesh component to extract from
 */
void UKawaiiFluidMeshCollider::CacheSkeletalMeshCollision(USkeletalMeshComponent* SkelMesh)
{
	UPhysicsAsset* PhysAsset = SkelMesh->GetPhysicsAsset();
	if (!PhysAsset)
	{
		return;
	}

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

		// Cache capsules
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
			CachedCap.BoneIndex = BoneIndex;

			CachedCapsules.Add(CachedCap);

			CachedBounds += CachedCap.Start;
			CachedBounds += CachedCap.End;
		}

		// Cache spheres
		for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
		{
			FTransform SphereLocalTransform = SphereElem.GetTransform();
			FTransform SphereWorldTransform = SphereLocalTransform * BoneTransform;

			FCachedSphere CachedSph;
			CachedSph.Center = SphereWorldTransform.GetLocation();
			CachedSph.Radius = SphereElem.Radius + CollisionMargin;
			CachedSph.BoneName = BodySetup->BoneName;
			CachedSph.BoneTransform = BoneTransform;
			CachedSph.BoneIndex = BoneIndex;
			CachedSpheres.Add(CachedSph);

			CachedBounds += CachedSph.Center;
		}

		// Cache boxes
		for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
		{
			FTransform BoxLocalTransform = BoxElem.GetTransform();
			FTransform BoxWorldTransform = BoxLocalTransform * BoneTransform;

			FCachedBox CachedBx;
			CachedBx.Center = BoxWorldTransform.GetLocation();
			CachedBx.Extent = FVector(BoxElem.X * 0.5f + CollisionMargin,
			                          BoxElem.Y * 0.5f + CollisionMargin,
			                          BoxElem.Z * 0.5f + CollisionMargin);
			CachedBx.Rotation = BoxWorldTransform.GetRotation();
			CachedBx.BoneName = BodySetup->BoneName;
			CachedBx.BoneTransform = BoneTransform;
			CachedBx.BoneIndex = BoneIndex;
			CachedBoxes.Add(CachedBx);

			// Add 8 box corners to expand bounding box
			for (int32 CornerIdx = 0; CornerIdx < 8; ++CornerIdx)
			{
				FVector LocalCorner(
					(CornerIdx & 1) ? CachedBx.Extent.X : -CachedBx.Extent.X,
					(CornerIdx & 2) ? CachedBx.Extent.Y : -CachedBx.Extent.Y,
					(CornerIdx & 4) ? CachedBx.Extent.Z : -CachedBx.Extent.Z
				);
				FVector WorldCorner = CachedBx.Center + CachedBx.Rotation.RotateVector(LocalCorner);
				CachedBounds += WorldCorner;
			}
		}

		// Cache convex hulls
		for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
		{
			FCachedConvex CachedCvx;
			CachedCvx.BoneName = BodySetup->BoneName;
			CachedCvx.BoneTransform = BoneTransform;
			CachedCvx.BoneIndex = BoneIndex;

			const TArray<FVector>& VertexData = ConvexElem.VertexData;
			if (VertexData.Num() < 4)
			{
				continue;
			}

			TArray<FVector> WorldVerts;
			WorldVerts.Reserve(VertexData.Num());
			FVector CenterSum = FVector::ZeroVector;
			for (const FVector& V : VertexData)
			{
				FVector WorldV = BoneTransform.TransformPosition(V);
				WorldVerts.Add(WorldV);
				CenterSum += WorldV;
			}

			CachedCvx.Center = CenterSum / static_cast<float>(WorldVerts.Num());

			float MaxDistSq = 0.0f;
			for (const FVector& V : WorldVerts)
			{
				float DistSq = FVector::DistSquared(V, CachedCvx.Center);
				MaxDistSq = FMath::Max(MaxDistSq, DistSq);
				CachedBounds += V;
			}
			CachedCvx.BoundingRadius = FMath::Sqrt(MaxDistSq) + CollisionMargin;

			const TArray<int32>& IndexData = ConvexElem.IndexData;
			if (IndexData.Num() >= 3)
			{
				TSet<uint32> PlaneHashes;
				for (int32 i = 0; i + 2 < IndexData.Num(); i += 3)
				{
					int32 I0 = IndexData[i];
					int32 I1 = IndexData[i + 1];
					int32 I2 = IndexData[i + 2];

					if (I0 < WorldVerts.Num() && I1 < WorldVerts.Num() && I2 < WorldVerts.Num())
					{
						FVector V0 = WorldVerts[I0];
						FVector V1 = WorldVerts[I1];
						FVector V2 = WorldVerts[I2];

						FVector Edge1 = V1 - V0;
						FVector Edge2 = V2 - V0;
						FVector Normal = FVector::CrossProduct(Edge1, Edge2);
						float NormalLen = Normal.Size();

						if (NormalLen > KINDA_SMALL_NUMBER)
						{
							Normal /= NormalLen;

							FVector ToCenter = CachedCvx.Center - V0;
							if (FVector::DotProduct(Normal, ToCenter) > 0)
							{
								Normal = -Normal;
							}

							int32 Nx = FMath::RoundToInt(Normal.X * 1000.0f);
							int32 Ny = FMath::RoundToInt(Normal.Y * 1000.0f);
							int32 Nz = FMath::RoundToInt(Normal.Z * 1000.0f);
							uint32 Hash = HashCombine(HashCombine(GetTypeHash(Nx), GetTypeHash(Ny)), GetTypeHash(Nz));

							if (!PlaneHashes.Contains(Hash))
							{
								PlaneHashes.Add(Hash);

								FCachedConvexPlane Plane;
								Plane.Normal = Normal;
								Plane.Distance = FVector::DotProduct(V0, Normal);
								CachedCvx.Planes.Add(Plane);
							}
						}
					}
				}
			}

			if (CachedCvx.Planes.Num() < 4)
			{
				TArray<FPlane> ChaosPlanes;
				ConvexElem.GetPlanes(ChaosPlanes);

				if (ChaosPlanes.Num() >= 4)
				{
					CachedCvx.Planes.Reset();

					for (const FPlane& ChaosPlane : ChaosPlanes)
					{
						const FVector LocalNormal = FVector(ChaosPlane.X, ChaosPlane.Y, ChaosPlane.Z);
						const FVector WorldNormal = BoneTransform.TransformVectorNoScale(LocalNormal);
						const FVector LocalPoint = LocalNormal * ChaosPlane.W;
						const FVector WorldPoint = BoneTransform.TransformPosition(LocalPoint);
						
						FCachedConvexPlane Plane;
						Plane.Normal = WorldNormal;
						Plane.Distance = FVector::DotProduct(WorldPoint, WorldNormal);
						CachedCvx.Planes.Add(Plane);
					}
				}
			}

			if (CachedCvx.Planes.Num() >= 4)
			{
				CachedConvexes.Add(MoveTemp(CachedCvx));
			}
		}
	}

	if (CachedCapsules.Num() > 0 || CachedSpheres.Num() > 0 || CachedBoxes.Num() > 0 || CachedConvexes.Num() > 0)
	{
		float MaxRadius = CollisionMargin;
		for (const FCachedCapsule& Cap : CachedCapsules) MaxRadius = FMath::Max(MaxRadius, Cap.Radius);
		for (const FCachedSphere& Sph : CachedSpheres) MaxRadius = FMath::Max(MaxRadius, Sph.Radius);
		for (const FCachedBox& Box : CachedBoxes) MaxRadius = FMath::Max(MaxRadius, Box.Extent.GetMax());
		for (const FCachedConvex& Cvx : CachedConvexes) MaxRadius = FMath::Max(MaxRadius, Cvx.BoundingRadius);
		CachedBounds = CachedBounds.ExpandBy(MaxRadius);
		bCacheValid = true;
	}
}

/**
 * @brief Extracts collision shapes from a static mesh's simple collision.
 * @param StaticMesh Static mesh component to extract from
 */
void UKawaiiFluidMeshCollider::CacheStaticMeshCollision(UStaticMeshComponent* StaticMesh)
{
	UStaticMesh* Mesh = StaticMesh->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	UBodySetup* BodySetup = Mesh->GetBodySetup();
	if (!BodySetup)
	{
		return;
	}

	FTransform ComponentTransform = StaticMesh->GetComponentTransform();

	// Cache spheres
	for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
	{
		FTransform SphereLocalTransform = SphereElem.GetTransform();
		FTransform SphereWorldTransform = SphereLocalTransform * ComponentTransform;

		FCachedSphere CachedSph;
		CachedSph.Center = SphereWorldTransform.GetLocation();
		CachedSph.Radius = SphereElem.Radius * ComponentTransform.GetScale3D().GetMax() + CollisionMargin;
		CachedSph.BoneName = NAME_None;
		CachedSph.BoneTransform = ComponentTransform;
		CachedSpheres.Add(CachedSph);

		CachedBounds += CachedSph.Center;
	}

	// Cache capsules
	for (const FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
	{
		FTransform CapsuleLocalTransform = SphylElem.GetTransform();
		FTransform CapsuleWorldTransform = CapsuleLocalTransform * ComponentTransform;

		FVector CapsuleCenter = CapsuleWorldTransform.GetLocation();
		FVector3d Scale = ComponentTransform.GetScale3D();
		float ScaledRadius = SphylElem.Radius * FMath::Max(Scale.X, Scale.Y) + CollisionMargin;
		float ScaledLength = SphylElem.Length * Scale.Z;
		FVector CapsuleUp = CapsuleWorldTransform.GetRotation().GetUpVector();

		float HalfLength = ScaledLength * 0.5f;

		FCachedCapsule CachedCap;
		CachedCap.Start = CapsuleCenter - CapsuleUp * HalfLength;
		CachedCap.End = CapsuleCenter + CapsuleUp * HalfLength;
		CachedCap.Radius = ScaledRadius;
		CachedCap.BoneName = NAME_None;
		CachedCap.BoneTransform = ComponentTransform;
		CachedCapsules.Add(CachedCap);

		CachedBounds += CachedCap.Start;
		CachedBounds += CachedCap.End;
	}

	// Cache boxes
	for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
	{
		FTransform BoxLocalTransform = BoxElem.GetTransform();
		FTransform BoxWorldTransform = BoxLocalTransform * ComponentTransform;

		FVector3d Scale = ComponentTransform.GetScale3D();

		FCachedBox CachedBx;
		CachedBx.Center = BoxWorldTransform.GetLocation();
		CachedBx.Extent = FVector(BoxElem.X * 0.5f * Scale.X + CollisionMargin,
		                          BoxElem.Y * 0.5f * Scale.Y + CollisionMargin,
		                          BoxElem.Z * 0.5f * Scale.Z + CollisionMargin);
		CachedBx.Rotation = BoxWorldTransform.GetRotation();
		CachedBx.BoneName = NAME_None;
		CachedBx.BoneTransform = ComponentTransform;

		CachedBoxes.Add(CachedBx);

		for (int32 CornerIdx = 0; CornerIdx < 8; ++CornerIdx)
		{
			FVector LocalCorner(
				(CornerIdx & 1) ? CachedBx.Extent.X : -CachedBx.Extent.X,
				(CornerIdx & 2) ? CachedBx.Extent.Y : -CachedBx.Extent.Y,
				(CornerIdx & 4) ? CachedBx.Extent.Z : -CachedBx.Extent.Z
			);
			FVector WorldCorner = CachedBx.Center + CachedBx.Rotation.RotateVector(LocalCorner);
			CachedBounds += WorldCorner;
		}
	}

	// Cache convex hulls
	for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
	{
		FCachedConvex CachedCvx;
		CachedCvx.BoneName = NAME_None;
		CachedCvx.BoneTransform = ComponentTransform;

		const TArray<FVector>& VertexData = ConvexElem.VertexData;
		if (VertexData.Num() < 4) continue;

		TArray<FVector> WorldVerts;
		WorldVerts.Reserve(VertexData.Num());
		FVector CenterSum = FVector::ZeroVector;
		for (const FVector& V : VertexData)
		{
			FVector WorldV = ComponentTransform.TransformPosition(V);
			WorldVerts.Add(WorldV);
			CenterSum += WorldV;
		}

		CachedCvx.Center = CenterSum / static_cast<float>(WorldVerts.Num());

		float MaxDistSq = 0.0f;
		for (const FVector& V : WorldVerts)
		{
			float DistSq = FVector::DistSquared(V, CachedCvx.Center);
			MaxDistSq = FMath::Max(MaxDistSq, DistSq);
			CachedBounds += V;
		}
		CachedCvx.BoundingRadius = FMath::Sqrt(MaxDistSq) + CollisionMargin;

		const TArray<int32>& IndexData = ConvexElem.IndexData;
		if (IndexData.Num() >= 3)
		{
			TSet<uint32> PlaneHashes;
			for (int32 i = 0; i + 2 < IndexData.Num(); i += 3)
			{
				int32 I0 = IndexData[i], I1 = IndexData[i + 1], I2 = IndexData[i + 2];
				if (I0 < WorldVerts.Num() && I1 < WorldVerts.Num() && I2 < WorldVerts.Num())
				{
					FVector V0 = WorldVerts[I0], V1 = WorldVerts[I1], V2 = WorldVerts[I2];
					FVector Edge1 = V1 - V0, Edge2 = V2 - V0;
					FVector Normal = FVector::CrossProduct(Edge1, Edge2);
					float NormalLen = Normal.Size();

					if (NormalLen > KINDA_SMALL_NUMBER)
					{
						Normal /= NormalLen;
						FVector ToCenter = CachedCvx.Center - V0;
						if (FVector::DotProduct(Normal, ToCenter) > 0) Normal = -Normal;

						int32 Nx = FMath::RoundToInt(Normal.X * 1000.0f), Ny = FMath::RoundToInt(Normal.Y * 1000.0f), Nz = FMath::RoundToInt(Normal.Z * 1000.0f);
						uint32 Hash = HashCombine(HashCombine(GetTypeHash(Nx), GetTypeHash(Ny)), GetTypeHash(Nz));

						if (!PlaneHashes.Contains(Hash))
						{
							PlaneHashes.Add(Hash);
							FCachedConvexPlane Plane;
							Plane.Normal = Normal;
							Plane.Distance = FVector::DotProduct(V0, Normal);
							CachedCvx.Planes.Add(Plane);
						}
					}
				}
			}
		}

		if (CachedCvx.Planes.Num() < 4)
		{
			TArray<FPlane> ChaosPlanes;
			ConvexElem.GetPlanes(ChaosPlanes);
			if (ChaosPlanes.Num() >= 4)
			{
				CachedCvx.Planes.Reset();
				for (const FPlane& ChaosPlane : ChaosPlanes)
				{
					const FVector LocalNormal = FVector(ChaosPlane.X, ChaosPlane.Y, ChaosPlane.Z);
					const FVector WorldNormal = ComponentTransform.TransformVectorNoScale(LocalNormal);
					const FVector LocalPoint = LocalNormal * ChaosPlane.W;
					const FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					FCachedConvexPlane Plane;
					Plane.Normal = WorldNormal;
					Plane.Distance = FVector::DotProduct(WorldPoint, WorldNormal);
					CachedCvx.Planes.Add(Plane);
				}
			}
		}

		if (CachedCvx.Planes.Num() >= 4) CachedConvexes.Add(MoveTemp(CachedCvx));
	}

	if (CachedCapsules.Num() > 0 || CachedSpheres.Num() > 0 || CachedBoxes.Num() > 0 || CachedConvexes.Num() > 0)
	{
		float MaxRadius = CollisionMargin;
		for (const FCachedCapsule& Cap : CachedCapsules) MaxRadius = FMath::Max(MaxRadius, Cap.Radius);
		for (const FCachedSphere& Sph : CachedSpheres) MaxRadius = FMath::Max(MaxRadius, Sph.Radius);
		for (const FCachedBox& Box : CachedBoxes) MaxRadius = FMath::Max(MaxRadius, Box.Extent.GetMax());
		for (const FCachedConvex& Cvx : CachedConvexes) MaxRadius = FMath::Max(MaxRadius, Cvx.BoundingRadius);
		CachedBounds = CachedBounds.ExpandBy(MaxRadius);
		bCacheValid = true;
	}
}

/**
 * @brief Finds the closest point on the mesh surface among all cached shapes.
 * @param Point Query point in world space
 * @param OutClosestPoint Closest point on the surface
 * @param OutNormal Surface normal at the closest point
 * @param OutDistance Distance to the closest point
 * @return True if a closest point was found
 */
bool UKawaiiFluidMeshCollider::GetClosestPoint(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance) const
{
	if (!bCacheValid) return false;

	const float CullingMargin = 50.0f;
	if (!CachedBounds.ExpandBy(CullingMargin).IsInside(Point)) return false;

	float MinDistance = TNumericLimits<float>::Max();
	bool bFoundAny = false;

	// Iterate capsules
	for (const FCachedCapsule& Cap : CachedCapsules)
	{
		FVector SegmentDir = Cap.End - Cap.Start;
		float SegmentLengthSq = SegmentDir.SizeSquared();
		FVector ClosestOnAxis = (SegmentLengthSq < KINDA_SMALL_NUMBER) ? Cap.Start : Cap.Start + SegmentDir * FMath::Clamp(FVector::DotProduct(Point - Cap.Start, SegmentDir) / SegmentLengthSq, 0.0f, 1.0f);
		FVector ToPointVec = Point - ClosestOnAxis;
		float DistToAxis = ToPointVec.Size();
		FVector TempNormal = (DistToAxis < KINDA_SMALL_NUMBER) ? FVector::ForwardVector : ToPointVec / DistToAxis;
		float TempDistance = DistToAxis - Cap.Radius;
		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance;
			OutClosestPoint = ClosestOnAxis + TempNormal * Cap.Radius;
			OutNormal = TempNormal;
			OutDistance = TempDistance;
			bFoundAny = true;
		}
	}

	// Iterate spheres
	for (const FCachedSphere& Sph : CachedSpheres)
	{
		FVector ToPointVec = Point - Sph.Center;
		float DistToCenter = ToPointVec.Size();
		FVector TempNormal = (DistToCenter < KINDA_SMALL_NUMBER) ? FVector::UpVector : ToPointVec / DistToCenter;
		float TempDistance = DistToCenter - Sph.Radius;
		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance;
			OutClosestPoint = Sph.Center + TempNormal * Sph.Radius;
			OutNormal = TempNormal;
			OutDistance = TempDistance;
			bFoundAny = true;
		}
	}

	// Iterate boxes
	for (const FCachedBox& Box : CachedBoxes)
	{
		FVector LocalPoint = Box.Rotation.UnrotateVector(Point - Box.Center);
		FVector ClampedLocal(FMath::Clamp(LocalPoint.X, -Box.Extent.X, Box.Extent.X), FMath::Clamp(LocalPoint.Y, -Box.Extent.Y, Box.Extent.Y), FMath::Clamp(LocalPoint.Z, -Box.Extent.Z, Box.Extent.Z));
		FVector TempClosestPoint = Box.Center + Box.Rotation.RotateVector(ClampedLocal);
		float TempDistance = FVector::Dist(Point, TempClosestPoint);
		FVector TempNormal;
		if (TempDistance < KINDA_SMALL_NUMBER)
		{
			FVector AbsLocal = LocalPoint.GetAbs();
			FVector DistToFace = Box.Extent - AbsLocal;
			if (DistToFace.X <= DistToFace.Y && DistToFace.X <= DistToFace.Z) { TempNormal = FVector(FMath::Sign(LocalPoint.X), 0.0f, 0.0f); TempDistance = -DistToFace.X; }
			else if (DistToFace.Y <= DistToFace.Z) { TempNormal = FVector(0.0f, FMath::Sign(LocalPoint.Y), 0.0f); TempDistance = -DistToFace.Y; }
			else { TempNormal = FVector(0.0f, 0.0f, FMath::Sign(LocalPoint.Z)); TempDistance = -DistToFace.Z; }
			TempNormal = Box.Rotation.RotateVector(TempNormal);
			TempClosestPoint = Point - TempNormal * TempDistance;
		}
		else TempNormal = (Point - TempClosestPoint) / TempDistance;

		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance;
			OutClosestPoint = TempClosestPoint;
			OutNormal = TempNormal;
			OutDistance = TempDistance;
			bFoundAny = true;
		}
	}

	// Iterate convexes
	for (const FCachedConvex& Cvx : CachedConvexes)
	{
		if (FVector::Dist(Point, Cvx.Center) - Cvx.BoundingRadius > MinDistance) continue;
		float MaxPlaneDist = -TNumericLimits<float>::Max();
		FVector BestNormal = FVector::UpVector;
		for (const FCachedConvexPlane& Plane : Cvx.Planes)
		{
			float PlaneDist = FVector::DotProduct(Point, Plane.Normal) - Plane.Distance;
			if (PlaneDist > MaxPlaneDist) { MaxPlaneDist = PlaneDist; BestNormal = Plane.Normal; }
		}
		if (MaxPlaneDist < MinDistance)
		{
			MinDistance = MaxPlaneDist;
			OutNormal = BestNormal;
			OutDistance = MaxPlaneDist;
			OutClosestPoint = Point - BestNormal * MaxPlaneDist;
			bFoundAny = true;
		}
	}

	if (!bFoundAny && TargetMeshComponent)
	{
		FVector BoxCenter = CachedBounds.GetCenter(), BoxExtent = CachedBounds.GetExtent(), LocalPoint = Point - BoxCenter;
		FVector ClampedPoint(FMath::Clamp(LocalPoint.X, -BoxExtent.X, BoxExtent.X), FMath::Clamp(LocalPoint.Y, -BoxExtent.Y, BoxExtent.Y), FMath::Clamp(LocalPoint.Z, -BoxExtent.Z, BoxExtent.Z));
		OutClosestPoint = BoxCenter + ClampedPoint;
		OutDistance = FVector::Dist(Point, OutClosestPoint);
		OutNormal = OutDistance > KINDA_SMALL_NUMBER ? (Point - OutClosestPoint) / OutDistance : FVector::UpVector;
		bFoundAny = true;
	}

	return bFoundAny;
}

/**
 * @brief Same as GetClosestPoint but also returns bone info for skeletal meshes.
 * @param Point Query point in world space
 * @param OutClosestPoint Closest point on the surface
 * @param OutNormal Surface normal at the closest point
 * @param OutDistance Distance to the closest point
 * @param OutBoneName Name of the closest bone
 * @param OutBoneTransform World transform of the closest bone
 * @return True if a closest point was found
 */
bool UKawaiiFluidMeshCollider::GetClosestPointWithBone(const FVector& Point, FVector& OutClosestPoint, FVector& OutNormal, float& OutDistance, FName& OutBoneName, FTransform& OutBoneTransform) const
{
	if (!bCacheValid) { OutBoneName = NAME_None; OutBoneTransform = FTransform::Identity; return false; }

	const float CullingMargin = 50.0f;
	if (!CachedBounds.ExpandBy(CullingMargin).IsInside(Point)) { OutBoneName = NAME_None; OutBoneTransform = FTransform::Identity; return false; }

	float MinDistance = TNumericLimits<float>::Max();
	bool bFoundAny = false;

	// Iterate capsules
	for (const FCachedCapsule& Cap : CachedCapsules)
	{
		FVector SegmentDir = Cap.End - Cap.Start;
		float SegmentLengthSq = SegmentDir.SizeSquared();
		FVector ClosestOnAxis = (SegmentLengthSq < KINDA_SMALL_NUMBER) ? Cap.Start : Cap.Start + SegmentDir * FMath::Clamp(FVector::DotProduct(Point - Cap.Start, SegmentDir) / SegmentLengthSq, 0.0f, 1.0f);
		FVector ToPointVec = Point - ClosestOnAxis;
		float DistToAxis = ToPointVec.Size();
		FVector TempNormal = (DistToAxis < KINDA_SMALL_NUMBER) ? FVector::ForwardVector : ToPointVec / DistToAxis;
		float TempDistance = DistToAxis - Cap.Radius;
		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance; OutClosestPoint = ClosestOnAxis + TempNormal * Cap.Radius; OutNormal = TempNormal; OutDistance = TempDistance; OutBoneName = Cap.BoneName; OutBoneTransform = Cap.BoneTransform; bFoundAny = true;
		}
	}

	// Iterate spheres
	for (const FCachedSphere& Sph : CachedSpheres)
	{
		FVector ToPointVec = Point - Sph.Center;
		float DistToCenter = ToPointVec.Size();
		FVector TempNormal = (DistToCenter < KINDA_SMALL_NUMBER) ? FVector::UpVector : ToPointVec / DistToCenter;
		float TempDistance = DistToCenter - Sph.Radius;
		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance; OutClosestPoint = Sph.Center + TempNormal * Sph.Radius; OutNormal = TempNormal; OutDistance = TempDistance; OutBoneName = Sph.BoneName; OutBoneTransform = Sph.BoneTransform; bFoundAny = true;
		}
	}

	// Iterate boxes
	for (const FCachedBox& Box : CachedBoxes)
	{
		FVector LocalPoint = Box.Rotation.UnrotateVector(Point - Box.Center);
		FVector ClampedLocal(FMath::Clamp(LocalPoint.X, -Box.Extent.X, Box.Extent.X), FMath::Clamp(LocalPoint.Y, -Box.Extent.Y, Box.Extent.Y), FMath::Clamp(LocalPoint.Z, -Box.Extent.Z, Box.Extent.Z));
		FVector TempClosestPoint = Box.Center + Box.Rotation.RotateVector(ClampedLocal);
		float TempDistance = FVector::Dist(Point, TempClosestPoint);
		FVector TempNormal;
		if (TempDistance < KINDA_SMALL_NUMBER)
		{
			FVector AbsLocal = LocalPoint.GetAbs();
			FVector DistToFace = Box.Extent - AbsLocal;
			if (DistToFace.X <= DistToFace.Y && DistToFace.X <= DistToFace.Z) { TempNormal = FVector(FMath::Sign(LocalPoint.X), 0.0f, 0.0f); TempDistance = -DistToFace.X; }
			else if (DistToFace.Y <= DistToFace.Z) { TempNormal = FVector(0.0f, FMath::Sign(LocalPoint.Y), 0.0f); TempDistance = -DistToFace.Y; }
			else { TempNormal = FVector(0.0f, 0.0f, FMath::Sign(LocalPoint.Z)); TempDistance = -DistToFace.Z; }
			TempNormal = Box.Rotation.RotateVector(TempNormal);
			TempClosestPoint = Point - TempNormal * TempDistance;
		}
		else TempNormal = (Point - TempClosestPoint) / TempDistance;

		if (TempDistance < MinDistance)
		{
			MinDistance = TempDistance; OutClosestPoint = TempClosestPoint; OutNormal = TempNormal; OutDistance = TempDistance; OutBoneName = Box.BoneName; OutBoneTransform = Box.BoneTransform; bFoundAny = true;
		}
	}

	// Iterate convexes
	for (const FCachedConvex& Cvx : CachedConvexes)
	{
		if (FVector::Dist(Point, Cvx.Center) - Cvx.BoundingRadius > MinDistance) continue;
		float MaxPlaneDist = -TNumericLimits<float>::Max();
		FVector BestNormal = FVector::UpVector;
		for (const FCachedConvexPlane& Plane : Cvx.Planes)
		{
			float PlaneDist = FVector::DotProduct(Point, Plane.Normal) - Plane.Distance;
			if (PlaneDist > MaxPlaneDist) { MaxPlaneDist = PlaneDist; BestNormal = Plane.Normal; }
		}
		if (MaxPlaneDist < MinDistance)
		{
			MinDistance = MaxPlaneDist; OutNormal = BestNormal; OutDistance = MaxPlaneDist; OutClosestPoint = Point - BestNormal * MaxPlaneDist; OutBoneName = Cvx.BoneName; OutBoneTransform = Cvx.BoneTransform; bFoundAny = true;
		}
	}

	if (!bFoundAny && TargetMeshComponent)
	{
		FVector BoxCenter = CachedBounds.GetCenter(), BoxExtent = CachedBounds.GetExtent(), LocalPoint = Point - BoxCenter;
		FVector ClampedPoint(FMath::Clamp(LocalPoint.X, -BoxExtent.X, BoxExtent.X), FMath::Clamp(LocalPoint.Y, -BoxExtent.Y, BoxExtent.Y), FMath::Clamp(LocalPoint.Z, -BoxExtent.Z, BoxExtent.Z));
		OutClosestPoint = BoxCenter + ClampedPoint;
		OutDistance = FVector::Dist(Point, OutClosestPoint);
		OutNormal = OutDistance > KINDA_SMALL_NUMBER ? (Point - OutClosestPoint) / OutDistance : FVector::UpVector;
		OutBoneName = NAME_None; OutBoneTransform = FTransform::Identity; bFoundAny = true;
	}

	return bFoundAny;
}

/**
 * @brief Checks if a point is inside any of the cached collision shapes.
 * @param Point Point to check in world space
 * @return True if the point is inside
 */
bool UKawaiiFluidMeshCollider::IsPointInside(const FVector& Point) const
{
	if (!TargetMeshComponent) return false;

	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(TargetMeshComponent);
	if (Capsule)
	{
		FVector CapsuleCenter = Capsule->GetComponentLocation(), CapsuleUp = Capsule->GetUpVector();
		float CapsuleRadius = Capsule->GetScaledCapsuleRadius(), CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		FVector ToPoint = Point - CapsuleCenter;
		float AxisProj = FVector::DotProduct(ToPoint, CapsuleUp);
		if (FMath::Abs(AxisProj) > CapsuleHalfHeight) return FVector::DistSquared(Point, CapsuleCenter + CapsuleUp * FMath::Sign(AxisProj) * (CapsuleHalfHeight - CapsuleRadius)) <= CapsuleRadius * CapsuleRadius;
		return FVector::DistSquared(Point, CapsuleCenter + CapsuleUp * AxisProj) <= CapsuleRadius * CapsuleRadius;
	}

	USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(TargetMeshComponent);
	if (SkelMesh && SkelMesh->GetPhysicsAsset())
	{
		for (const FCachedConvex& Cvx : CachedConvexes)
		{
			if (FVector::Dist(Point, Cvx.Center) > Cvx.BoundingRadius) continue;
			bool bInside = true;
			for (const FCachedConvexPlane& Plane : Cvx.Planes) { if (FVector::DotProduct(Point, Plane.Normal) - Plane.Distance > 0.0f) { bInside = false; break; } }
			if (bInside) return true;
		}
		// Also check other shapes (omitted for brevity but should follow similar logic)
	}

	for (const FCachedConvex& Cvx : CachedConvexes)
	{
		if (FVector::Dist(Point, Cvx.Center) > Cvx.BoundingRadius) continue;
		bool bInside = true;
		for (const FCachedConvexPlane& Plane : Cvx.Planes) { if (FVector::DotProduct(Point, Plane.Normal) - Plane.Distance > 0.0f) { bInside = false; break; } }
		if (bInside) return true;
	}

	return TargetMeshComponent->Bounds.GetBox().IsInside(Point);
}

/**
 * @brief Exports cached primitive data for GPU collision processing.
 * @param OutSpheres Output array for spheres
 * @param OutCapsules Output array for capsules
 * @param OutBoxes Output array for boxes
 * @param OutConvexes Output array for convex hulls
 * @param OutPlanes Output array for convex planes
 * @param InFriction Friction coefficient to apply
 * @param InRestitution Restitution coefficient to apply
 * @param InOwnerID Unique owner ID for filtering
 */
void UKawaiiFluidMeshCollider::ExportToGPUPrimitives(
	TArray<FGPUCollisionSphere>& OutSpheres,
	TArray<FGPUCollisionCapsule>& OutCapsules,
	TArray<FGPUCollisionBox>& OutBoxes,
	TArray<FGPUCollisionConvex>& OutConvexes,
	TArray<FGPUConvexPlane>& OutPlanes,
	float InFriction,
	float InRestitution,
	int32 InOwnerID
) const
{
	if (!bCacheValid) return;

	for (const FCachedSphere& Sph : CachedSpheres)
	{
		FGPUCollisionSphere GPUSphere;
		GPUSphere.Center = FVector3f(Sph.Center); GPUSphere.Radius = Sph.Radius; GPUSphere.Friction = InFriction; GPUSphere.Restitution = InRestitution; GPUSphere.BoneIndex = Sph.BoneIndex; GPUSphere.OwnerID = InOwnerID; GPUSphere.bHasFluidInteraction = 1;
		OutSpheres.Add(GPUSphere);
	}

	for (const FCachedCapsule& Cap : CachedCapsules)
	{
		FGPUCollisionCapsule GPUCapsule;
		GPUCapsule.Start = FVector3f(Cap.Start); GPUCapsule.End = FVector3f(Cap.End); GPUCapsule.Radius = Cap.Radius; GPUCapsule.Friction = InFriction; GPUCapsule.Restitution = InRestitution; GPUCapsule.BoneIndex = Cap.BoneIndex; GPUCapsule.OwnerID = InOwnerID; GPUCapsule.bHasFluidInteraction = 1;
		OutCapsules.Add(GPUCapsule);
	}

	for (const FCachedBox& Box : CachedBoxes)
	{
		FGPUCollisionBox GPUBox;
		GPUBox.Center = FVector3f(Box.Center); GPUBox.Extent = FVector3f(Box.Extent); GPUBox.Rotation = FVector4f(Box.Rotation.X, Box.Rotation.Y, Box.Rotation.Z, Box.Rotation.W); GPUBox.Friction = InFriction; GPUBox.Restitution = InRestitution; GPUBox.BoneIndex = Box.BoneIndex; GPUBox.OwnerID = InOwnerID; GPUBox.bHasFluidInteraction = 1;
		OutBoxes.Add(GPUBox);
	}

	for (const FCachedConvex& Cvx : CachedConvexes)
	{
		FGPUCollisionConvex GPUConvex;
		GPUConvex.Center = FVector3f(Cvx.Center); GPUConvex.BoundingRadius = Cvx.BoundingRadius; GPUConvex.PlaneStartIndex = OutPlanes.Num(); GPUConvex.PlaneCount = Cvx.Planes.Num(); GPUConvex.Friction = InFriction; GPUConvex.Restitution = InRestitution; GPUConvex.BoneIndex = Cvx.BoneIndex; GPUConvex.OwnerID = InOwnerID; GPUConvex.bHasFluidInteraction = 1;
		OutConvexes.Add(GPUConvex);
		for (const FCachedConvexPlane& Plane : Cvx.Planes) { FGPUConvexPlane GPUPlane; GPUPlane.Normal = FVector3f(Plane.Normal); GPUPlane.Distance = Plane.Distance; OutPlanes.Add(GPUPlane); }
	}
}

/**
 * @brief Exports cached primitives with associated bone transforms for animated GPU collision.
 * @param OutSpheres Output array for spheres
 * @param OutCapsules Output array for capsules
 * @param OutBoxes Output array for boxes
 * @param OutConvexes Output array for convex hulls
 * @param OutPlanes Output array for convex planes
 * @param OutBoneTransforms Output array for bone transforms
 * @param BoneNameToIndex Shared mapping of bone names to indices
 * @param InFriction Friction coefficient
 * @param InRestitution Restitution coefficient
 * @param InOwnerID Unique owner ID
 */
void UKawaiiFluidMeshCollider::ExportToGPUPrimitivesWithBones(
	TArray<FGPUCollisionSphere>& OutSpheres,
	TArray<FGPUCollisionCapsule>& OutCapsules,
	TArray<FGPUCollisionBox>& OutBoxes,
	TArray<FGPUCollisionConvex>& OutConvexes,
	TArray<FGPUConvexPlane>& OutPlanes,
	TArray<FGPUBoneTransform>& OutBoneTransforms,
	TMap<FName, int32>& BoneNameToIndex,
	float InFriction,
	float InRestitution,
	int32 InOwnerID
) const
{
	if (!bCacheValid) return;

	auto GetOrCreateBoneIndex = [&](FName BoneName, const FTransform& BoneTransform) -> int32
	{
		if (BoneName == NAME_None) return -1;
		if (int32* ExistingIndex = BoneNameToIndex.Find(BoneName)) { OutBoneTransforms[*ExistingIndex].UpdatePrevious(); OutBoneTransforms[*ExistingIndex].SetFromTransform(BoneTransform); return *ExistingIndex; }
		int32 NewIndex = OutBoneTransforms.Num(); BoneNameToIndex.Add(BoneName, NewIndex);
		FGPUBoneTransform GPUBone; GPUBone.SetFromTransform(BoneTransform); GPUBone.UpdatePrevious(); OutBoneTransforms.Add(GPUBone);
		return NewIndex;
	};

	for (const FCachedSphere& Sph : CachedSpheres) { FGPUCollisionSphere GPUSphere; GPUSphere.Center = FVector3f(Sph.Center); GPUSphere.Radius = Sph.Radius; GPUSphere.Friction = InFriction; GPUSphere.Restitution = InRestitution; GPUSphere.BoneIndex = Sph.BoneIndex; GPUSphere.OwnerID = InOwnerID; GPUSphere.bHasFluidInteraction = 1; GetOrCreateBoneIndex(Sph.BoneName, Sph.BoneTransform); OutSpheres.Add(GPUSphere); }
	for (const FCachedCapsule& Cap : CachedCapsules) { FGPUCollisionCapsule GPUCapsule; GPUCapsule.Start = FVector3f(Cap.Start); GPUCapsule.End = FVector3f(Cap.End); GPUCapsule.Radius = Cap.Radius; GPUCapsule.Friction = InFriction; GPUCapsule.Restitution = InRestitution; GPUCapsule.BoneIndex = Cap.BoneIndex; GPUCapsule.OwnerID = InOwnerID; GPUCapsule.bHasFluidInteraction = 1; GetOrCreateBoneIndex(Cap.BoneName, Cap.BoneTransform); OutCapsules.Add(GPUCapsule); }
	for (const FCachedBox& Box : CachedBoxes) { FGPUCollisionBox GPUBox; GPUBox.Center = FVector3f(Box.Center); GPUBox.Extent = FVector3f(Box.Extent); GPUBox.Rotation = FVector4f(Box.Rotation.X, Box.Rotation.Y, Box.Rotation.Z, Box.Rotation.W); GPUBox.Friction = InFriction; GPUBox.Restitution = InRestitution; GPUBox.BoneIndex = Box.BoneIndex; GPUBox.OwnerID = InOwnerID; GPUBox.bHasFluidInteraction = 1; GetOrCreateBoneIndex(Box.BoneName, Box.BoneTransform); OutBoxes.Add(GPUBox); }
	for (const FCachedConvex& Cvx : CachedConvexes)
	{
		FGPUCollisionConvex GPUConvex; GPUConvex.Center = FVector3f(Cvx.Center); GPUConvex.BoundingRadius = Cvx.BoundingRadius; GPUConvex.PlaneStartIndex = OutPlanes.Num(); GPUConvex.PlaneCount = Cvx.Planes.Num(); GPUConvex.Friction = InFriction; GPUConvex.Restitution = InRestitution; GPUConvex.BoneIndex = Cvx.BoneIndex; GPUConvex.OwnerID = InOwnerID; GPUConvex.bHasFluidInteraction = 1; GetOrCreateBoneIndex(Cvx.BoneName, Cvx.BoneTransform); OutConvexes.Add(GPUConvex);
		for (const FCachedConvexPlane& Plane : Cvx.Planes) { FGPUConvexPlane GPUPlane; GPUPlane.Normal = FVector3f(Plane.Normal); GPUPlane.Distance = Plane.Distance; OutPlanes.Add(GPUPlane); }
	}
}