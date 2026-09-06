#include "Tissue/Geometry/TissueCutSurface.h"
#include "Tissue/Geometry/Utility.h"

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