#pragma once

#include "CoreMinimal.h"
#include "Tissue/Geometry/SweptBladeIntersection.h"

struct FTetBoundaryIntersection
{
	/**
	 * Index of Tet edge:
	 *
	 * 0 = V0-V1
	 * 1 = V0-V2
	 * 2 = V0-V3
	 * 3 = V1-V2
	 * 4 = V1-V3
	 * 5 = V2-V3
	 */
	int32 TetEdgeIndex = INDEX_NONE;

	/**
	 * Intersection point in Tissue Local Space.
	 */
	FVector3f Point = FVector3f::ZeroVector;

	/**
	 * Parametric position along Tet edge.
	 * 0 = edge start
	 * 1 = edge end.
	 */
	float EdgeT = 0.0f;

	/**
	 * Swept triangle that produced this intersection.
	 */
	int32 BladeTriangleIndex = INDEX_NONE;
};

struct FTetCutPatch
{
	int32 BladeTriangleIndex = INDEX_NONE;

	TArray<FVector3f> Polygon;

	FVector3f Normal = FVector3f::ZeroVector;

	float Area = 0.0f;
};

struct FTetCutData
{
	int32 TetId = INDEX_NONE;

	TArray<FTetCutPatch> Patches;

	TArray<FTetBoundaryIntersection> BoundaryIntersections;

	float TotalIntersectionArea = 0.0f;

	bool bNeedsCut = false;
};

namespace TissueCutSurface
{
	float ComputePolygonArea(const TArray<FVector3f>& Polygon);

	void BuildTetCutData(const TArray<FTriangleTetIntersection>& Intersections, TArray<FTetCutData>& OutTetCuts);

	void FindTetEdgeIntersections(
		const TArray<FSweptBladeTriangle>& SweptTriangles,
		const FTissueTopologySnapshot& Snapshot,
		FTetCutData& TetCut);
}