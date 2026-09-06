#pragma once

#include "CoreMinimal.h"
#include "Tissue/Geometry/SweptBladeIntersection.h"

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

	float TotalIntersectionArea = 0.0f;

	bool bNeedsCut = false;
};

struct FTetCutSurface
{
	int32 TetId;

	TArray<FVector3f> Vertices;
	TArray<FIntVector3> Triangles;
};

// Result of intersections
namespace TissueCutSurface
{
	float ComputePolygonArea(const TArray<FVector3f>& Polygon);

	void BuildTetCutData(const TArray<FTriangleTetIntersection>& Intersections, TArray<FTetCutData>& OutTetCuts);
}