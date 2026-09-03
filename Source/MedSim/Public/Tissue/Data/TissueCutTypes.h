#pragma once

#include "CoreMinimal.h"
#include "TissueCutTypes.generated.h"

USTRUCT()
struct FCutIntersection
{
	GENERATED_BODY()

	// Exact intersection point in Tissue local space
	FVector3f Point = FVector3f::ZeroVector;

	// Index of blade sample trajectory:
	// PreviousBladePoints[BladeSampleIndex]
	// ->
	// CurrentBladePoints[BladeSampleIndex]
	int32 BladeSampleIndex = INDEX_NONE;

	// Position along that trajectory:
	// 0 = PreviousBladePoints[BladeSampleIndex]
	// 1 = CurrentBladePoints[BladeSampleIndex]
	float SegmentT = 0.0f;

	// Which tetrahedron face was intersected
	int32 TetFaceIndex = INDEX_NONE;

	FVector3f Normal = FVector3f::ZeroVector;
};

USTRUCT()
struct FCutTetHit
{
	GENERATED_BODY()

	// Which tetrahedron was hit
	int32 TetId = INDEX_NONE;

	// All intersection events with this tetrahedron
	TArray<FCutIntersection> Intersections;
};

USTRUCT()
struct FCutPathPoint
{
	GENERATED_BODY()

	FVector3f Point = FVector3f::ZeroVector;

	int32 TetId = INDEX_NONE;

	int32 BladeSampleIndex = INDEX_NONE;

	float SegmentT = 0.0f;

	FVector3f Normal = FVector3f::ZeroVector;
};