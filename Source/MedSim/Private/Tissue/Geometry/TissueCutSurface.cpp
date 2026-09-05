#include "Tissue/Geometry/TissueCutSurface.h"
#include "Tissue/Geometry/TissueIntersection.h"

float TissueCutSurface::ComputePolygonArea(const TArray<FVector3f>& Polygon)
{
	if (Polygon.Num() < 3)
	{
		return 0.0f;
	}

	const FVector3f& P0 = Polygon[0];

	float Area = 0.0f;

	for (int32 i = 1; i < Polygon.Num() - 1; ++i)
	{
		const FVector3f A = Polygon[i] - P0;
		const FVector3f B = Polygon[i + 1] - P0;

		const float TriangleArea = 0.5f * FVector3f::CrossProduct(A, B).Length();

		Area += TriangleArea;
	}

	return Area;
}

void TissueCutSurface::BuildTetCutData(const TArray<FTriangleTetIntersection>& Intersections, TArray<FTetCutData>& OutTetCuts)
{
	OutTetCuts.Reset();

	TMap<int32, int32> TetIdToCutIndex;

	for (const FTriangleTetIntersection& Intersection : Intersections)
	{
		if (Intersection.TetId == INDEX_NONE)
		{
			continue;
		}

		int32* ExistingIndex = TetIdToCutIndex.Find(Intersection.TetId);
		int32 CutIndex;

		if (ExistingIndex)
		{
			CutIndex = *ExistingIndex;
		}
		else
		{
			CutIndex = OutTetCuts.AddDefaulted();

			OutTetCuts[CutIndex].TetId = Intersection.TetId;

			TetIdToCutIndex.Add(Intersection.TetId, CutIndex);
		}

		FTetCutData& TetCut = OutTetCuts[CutIndex];

		FTetCutPatch Patch;

		Patch.BladeTriangleIndex = Intersection.BladeTriangleIndex;
		Patch.Polygon = Intersection.Polygon;

		Patch.Normal = Intersection.Normal;

		Patch.Area = ComputePolygonArea(Patch.Polygon);

		if (Patch.Area <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// Maybe need for update - connected patch
		TetCut.TotalIntersectionArea += Patch.Area;

		TetCut.Patches.Add(MoveTemp(Patch));
	}

	// Simple criteria - needs to be cut surface
	constexpr float MinimumCutArea = 0.001f;

	for (FTetCutData& TetCut : OutTetCuts)
	{
		TetCut.bNeedsCut = TetCut.TotalIntersectionArea > MinimumCutArea;
	}
}

namespace
{
	struct FTetEdge
	{
		int32 A;
		int32 B;
	};

	static constexpr FTetEdge TetEdges[6] =
	{
		{0, 1},
		{0, 2},
		{0, 3},
		{1, 2},
		{1, 3},
		{2, 3}
	};
}

void TissueCutSurface::FindTetEdgeIntersections(const TArray<FSweptBladeTriangle>& SweptTriangles, const FTissueTopologySnapshot& Snapshot, FTetCutData& TetCut)
{
	TetCut.BoundaryIntersections.Reset();

	if (!TetCut.bNeedsCut)
	{
		return;
	}

	if (!Snapshot.Tetrahedra.IsValidIndex(TetCut.TetId))
	{
		return;
	}

	const FTissueTet& Tet = Snapshot.Tetrahedra[TetCut.TetId];

	const int32 VertexIndices[4] =
	{
		Tet.Vertices.X,
		Tet.Vertices.Y,
		Tet.Vertices.Z,
		Tet.Vertices.W
	};

	FVector3f TetVertices[4];

	for (int32 i = 0; i < 4; ++i)
	{
		if (!Snapshot.Vertices.IsValidIndex(VertexIndices[i]))
		{
			return;
		}

		TetVertices[i] = Snapshot.Vertices[VertexIndices[i]].CurrentPosition;
	}

	/**
	 * Deduplication is done by:
	 *
	 * 1. TetEdgeIndex
	 * 2. spatial position
	 *
	 * This allows multiple distinct cut points
	 * on the same edge to survive, while
	 * duplicate detection of the same physical
	 * point removes repeated hits.
	 */
	auto AddUniqueBoundaryIntersection =
		[&TetCut](
			int32 EdgeIndex,
			const FVector3f& Point,
			float EdgeT,
			int32 BladeTriangleIndex)
		{
			constexpr float PositionTolerance = 0.01f;
			const float ToleranceSquared = PositionTolerance * PositionTolerance;

			for (const FTetBoundaryIntersection& Existing : TetCut.BoundaryIntersections)
			{
				if (Existing.TetEdgeIndex != EdgeIndex)
				{
					continue;
				}

				if ((Existing.Point - Point) .SquaredLength() <= ToleranceSquared)
				{
					return;
				}
			}

			FTetBoundaryIntersection NewIntersection;

			NewIntersection.TetEdgeIndex = EdgeIndex;

			NewIntersection.Point = Point;

			NewIntersection.EdgeT = EdgeT;

			NewIntersection.BladeTriangleIndex = BladeTriangleIndex;

			TetCut.BoundaryIntersections.Add(NewIntersection);
		};

	/**
	 * We don't test every 112 swept triangles.
	 *
	 * We test only triangles that actually
	 * generated a valid area patch inside this Tet.
	 */
	for (const FTetCutPatch& Patch : TetCut.Patches)
	{
		if (!SweptTriangles.IsValidIndex(Patch.BladeTriangleIndex))
		{
			continue;
		}

		const FSweptBladeTriangle& BladeTriangle = SweptTriangles[Patch.BladeTriangleIndex];

		for (int32 EdgeIndex = 0; EdgeIndex < 6; ++EdgeIndex)
		{
			const FTetEdge& Edge = TetEdges[EdgeIndex];

			const FVector3f& EdgeStart = TetVertices[Edge.A];

			const FVector3f& EdgeEnd = TetVertices[Edge.B];

			FVector3f IntersectionPoint;
			float EdgeT = 0.0f;
			FVector3f TriangleNormal;

			if (TissueIntersection::
				SegmentIntersectsTriangle(
					EdgeStart,
					EdgeEnd,
					BladeTriangle.A,
					BladeTriangle.B,
					BladeTriangle.C,
					IntersectionPoint,
					EdgeT,
					TriangleNormal))
			{
				AddUniqueBoundaryIntersection(
					EdgeIndex,
					IntersectionPoint,
					EdgeT,
					Patch.BladeTriangleIndex
				);
			}
		}
	}
}
