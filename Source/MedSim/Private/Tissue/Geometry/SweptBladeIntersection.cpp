#include "Tissue/Geometry/SweptBladeIntersection.h"
#include "Tissue/Geometry/TissueIntersection.h"

namespace
{
	struct FTetFace
	{
		int32 A;
		int32 B;
		int32 C;
	};

	struct FTetEdge
	{
		int32 A;
		int32 B;
	};
}

static bool IsPointInTriangle(
	const FVector3f& Point,
	const FVector3f& A,
	const FVector3f& B,
	const FVector3f& C,
	float Epsilon = 0.01f)
{
	const FVector3f Normal = FVector3f::CrossProduct(B - A, C - A);
	const float NormalSizeSquared = Normal.SquaredLength();

	if (NormalSizeSquared <= SMALL_NUMBER)
	{
		return false;
	}

	const FVector3f N = Normal / FMath::Sqrt(NormalSizeSquared);

	// Point has to be on triangle surface.
	const float PlaneDistance = FVector3f::DotProduct(Point - A, N);

	if (FMath::Abs(PlaneDistance) > Epsilon)
	{
		return false;
	}

	const FVector3f V0 = B - A;
	const FVector3f V1 = C - A;
	const FVector3f V2 = Point - A;

	const float Dot00 = FVector3f::DotProduct(V0, V0);
	const float Dot01 = FVector3f::DotProduct(V0, V1);
	const float Dot02 = FVector3f::DotProduct(V0, V2);
	const float Dot11 = FVector3f::DotProduct(V1, V1);
	const float Dot12 = FVector3f::DotProduct(V1, V2);

	const float Denominator = Dot00 * Dot11 - Dot01 * Dot01;

	if (FMath::Abs(Denominator) <= SMALL_NUMBER)
	{
		return false;
	}

	const float InvDenominator = 1.0f / Denominator;

	const float U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenominator;

	const float V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenominator;

	return U >= -Epsilon && V >= -Epsilon && U + V <= 1.0f + Epsilon;
}

static FVector3f ComputeCentroid(
	const TArray<FVector3f>& Points)
{
	FVector3f Sum = FVector3f::ZeroVector;

	for (const FVector3f& Point : Points)
	{
		Sum += Point;
	}

	return Sum / static_cast<float>(Points.Num());
}

bool SweptBladeIntersection::IntersectTriangleWithTet(
	const FSweptBladeTriangle& BladeTriangle,
	const FVector3f(&V)[4],
	int32 TetId,
	int32 BladeTriangleIndex,
	FTriangleTetIntersection& OutIntersection)
{
	OutIntersection = {};

	OutIntersection.TetId = TetId;
	OutIntersection.BladeTriangleIndex = BladeTriangleIndex;

	TArray<FVector3f> Points;

	Points.Reserve(16);

	auto AddUniquePoint =
		[&Points](const FVector3f& Point)
		{
			constexpr float PositionTolerance = 0.01f;
			const float ToleranceSquared = PositionTolerance * PositionTolerance;

			for (const FVector3f& Existing : Points)
			{
				if ((Existing - Point).SquaredLength() <= ToleranceSquared)
				{
					return;
				}
			}

			Points.Add(Point);
		};

	// --------------------------------------------------------
	// 1. Triangle vertices inside tetrahedron
	// --------------------------------------------------------

	if (TissueIntersection::IsPointInTetrahedron(BladeTriangle.A, V[0], V[1], V[2], V[3]))
	{
		AddUniquePoint(BladeTriangle.A);
	}

	if (TissueIntersection::IsPointInTetrahedron(BladeTriangle.B, V[0], V[1], V[2], V[3]))
	{
		AddUniquePoint(BladeTriangle.B);
	}

	if (TissueIntersection::IsPointInTetrahedron(BladeTriangle.C, V[0], V[1], V[2], V[3]))
	{
		AddUniquePoint(BladeTriangle.C);
	}

	// --------------------------------------------------------
	// 2. Triangle edges vs Tet faces
	// --------------------------------------------------------

	const FTetFace Faces[4] =
	{
		{0, 1, 2},
		{0, 1, 3},
		{0, 2, 3},
		{1, 2, 3}
	};

	const FVector3f TriangleVertices[3] =
	{
		BladeTriangle.A,
		BladeTriangle.B,
		BladeTriangle.C
	};

	for (int32 TriangleEdgeIndex = 0; TriangleEdgeIndex < 3; ++TriangleEdgeIndex)
	{
		const FVector3f& Start = TriangleVertices[TriangleEdgeIndex];
		const FVector3f& End = TriangleVertices[(TriangleEdgeIndex + 1) % 3];

		for (int32 FaceIndex = 0; FaceIndex < 4; ++FaceIndex)
		{
			const FTetFace& Face = Faces[FaceIndex];

			FVector3f Point;
			float SegmentT = 0.0f;
			FVector3f Normal;

			// TODO: coplanar case
			if (TissueIntersection::SegmentIntersectsTriangle(
				Start,
				End,
				V[Face.A],
				V[Face.B],
				V[Face.C],
				Point,
				SegmentT,
				Normal))
			{
				AddUniquePoint(Point);
			}
		}
	}

	// --------------------------------------------------------
	// 3. Tet edges vs swept triangle
	// --------------------------------------------------------

	const FTetEdge Edges[6] =
	{
		{0, 1},
		{0, 2},
		{0, 3},
		{1, 2},
		{1, 3},
		{2, 3}
	};

	for (const FTetEdge& Edge : Edges)
	{
		const FVector3f Start = V[Edge.A];
		const FVector3f End = V[Edge.B];

		FVector3f Point;
		float SegmentT = 0.0f;
		FVector3f Normal;

		// TODO: coplanar case
		if (TissueIntersection::SegmentIntersectsTriangle(
			Start,
			End,
			BladeTriangle.A,
			BladeTriangle.B,
			BladeTriangle.C,
			Point,
			SegmentT,
			Normal))
		{
			if (IsPointInTriangle(
				Point,
				BladeTriangle.A,
				BladeTriangle.B,
				BladeTriangle.C))
			{
				AddUniquePoint(Point);
			}
		}
	}

	if (Points.Num() < 2)
	{
		return false;
	}

	// --------------------------------------------------------
	// 4. Polygon normal
	// --------------------------------------------------------

	const FVector3f Normal = FVector3f::CrossProduct(BladeTriangle.B - BladeTriangle.A, BladeTriangle.C - BladeTriangle.A).GetSafeNormal();

	if (Normal.IsNearlyZero())
	{
		return false;
	}

	OutIntersection.Normal = Normal;

	// --------------------------------------------------------
	// 5. Order polygon points
	// --------------------------------------------------------

	const FVector3f Centroid = ComputeCentroid(Points);

	const FVector3f ReferenceAxis = (FMath::Abs(Normal.Z) < 0.9f) ? FVector3f(0, 0, 1) : FVector3f(0, 1, 0);

	const FVector3f AxisU = FVector3f::CrossProduct(ReferenceAxis, Normal).GetSafeNormal();

	const FVector3f AxisV = FVector3f::CrossProduct(Normal, AxisU).GetSafeNormal();

	Points.Sort(
		[&](const FVector3f& A, const FVector3f& B)
		{
			const FVector3f DA = A - Centroid;
			const FVector3f DB = B - Centroid;

			const float AngleA = FMath::Atan2(FVector3f::DotProduct(DA, AxisV), FVector3f::DotProduct(DA, AxisU));

			const float AngleB = FMath::Atan2(FVector3f::DotProduct(DB, AxisV), FVector3f::DotProduct(DB, AxisU));

			return AngleA < AngleB;
		}
	);

	OutIntersection.Polygon = MoveTemp(Points);

	return true;
}

void SweptBladeIntersection::FindIntersections(const TArray<FSweptBladeTriangle>& SweptTriangles, const TArray<int32>& CandidateTetIds, const FTissueTopologySnapshot& Snapshot, TArray<FTriangleTetIntersection>& OutIntersections)
{
	OutIntersections.Reset();

	for (const int32 TetId : CandidateTetIds)
	{
		if (!Snapshot.Tetrahedra.IsValidIndex(TetId))
		{
			continue;
		}

		const FTissueTet& Tet = Snapshot.Tetrahedra[TetId];

		const int32 I0 = Tet.Vertices.X;
		const int32 I1 = Tet.Vertices.Y;
		const int32 I2 = Tet.Vertices.Z;
		const int32 I3 = Tet.Vertices.W;

		if (!Snapshot.Vertices.IsValidIndex(I0) ||
			!Snapshot.Vertices.IsValidIndex(I1) ||
			!Snapshot.Vertices.IsValidIndex(I2) ||
			!Snapshot.Vertices.IsValidIndex(I3))
		{
			continue;
		}

		const FVector3f TetVertices[4] =
		{
			Snapshot.Vertices[I0].CurrentPosition,
			Snapshot.Vertices[I1].CurrentPosition,
			Snapshot.Vertices[I2].CurrentPosition,
			Snapshot.Vertices[I3].CurrentPosition
		};

		for (int32 TriangleIndex = 0; TriangleIndex < SweptTriangles.Num(); ++TriangleIndex)
		{
			FTriangleTetIntersection Intersection;

			if (IntersectTriangleWithTet(
				SweptTriangles[TriangleIndex],
				TetVertices,
				TetId,
				TriangleIndex,
				Intersection))
			{
				OutIntersections.Add(MoveTemp(Intersection));
			}
		}
	}
}
