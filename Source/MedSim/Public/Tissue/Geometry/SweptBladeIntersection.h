#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueTopology.h"
#include "Tissue/Geometry/SweptBlade.h"

struct FTriangleTetIntersection
{
	int32 TetId = INDEX_NONE;
	int32 BladeTriangleIndex = INDEX_NONE;

	TArray<FVector3f> Polygon;

	FVector3f Normal = FVector3f::ZeroVector;
};

// Precise tria tet intersection
namespace SweptBladeIntersection
{
	bool IntersectTriangleWithTet(
		const FSweptBladeTriangle& BladeTriangle,
		const FVector3f(&TetVertices)[4],
		int32 TetId,
		int32 BladeTriangleIndex,
		FTriangleTetIntersection& OutIntersection
	);

	void FindIntersections(
		const TArray<FSweptBladeTriangle>& SweptTriangles,
		const TArray<int32>& CandidateTetIds,
		const FTissueTopologySnapshot& Snapshot,
		TArray<FTriangleTetIntersection>& OutIntersections
	);
}