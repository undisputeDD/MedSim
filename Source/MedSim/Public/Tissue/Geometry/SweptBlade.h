#pragma once

#include "CoreMinimal.h"

struct FSweptBladeTriangle
{
	FVector3f A;
	FVector3f B;
	FVector3f C;

	/**
	 * Indexes of two blade samples,
	 * which create this swept surface.
	 *
	 * For the same quad will be:
	 * BladeSampleA = i
	 * BladeSampleB = i + 1
	 */
	int32 BladeSampleA = INDEX_NONE;
	int32 BladeSampleB = INDEX_NONE;
};

// Building swept geometry
namespace SweptBlade
{
	/**
	 * Builds swept surface between blade positions(segments)
	 * in prev and curr tick.
	 *
	 * Inputs must be in the same coord system.
	 *
	 * For N blade samples creates:
	 *
	 *     (N - 1) quads
	 *     2 * (N - 1) triangles
	 */
	void BuildSurface(
		const TArray<FVector3f>& PreviousBladePoints,
		const TArray<FVector3f>& CurrentBladePoints,
		TArray<FSweptBladeTriangle>& OutTriangles
	);
}