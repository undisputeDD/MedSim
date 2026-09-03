#include "Tissue/Geometry/SweptBlade.h"

namespace SweptBlade
{
	void BuildSurface(
		const TArray<FVector3f>& PreviousBladePoints,
		const TArray<FVector3f>& CurrentBladePoints,
		TArray<FSweptBladeTriangle>& OutTriangles
	)
	{
		OutTriangles.Reset();

		if (PreviousBladePoints.Num() != CurrentBladePoints.Num())
		{
			return;
		}

		const int32 NumPoints = PreviousBladePoints.Num();

		if (NumPoints < 2)
		{
			return;
		}

		const int32 NumSegments = NumPoints - 1;

		// Each quad -> 2 triangles.
		OutTriangles.Reserve(NumSegments * 2);

		for (int32 i = 0; i < NumSegments; ++i)
		{
			const FVector3f& PreviousA = PreviousBladePoints[i];
			const FVector3f& PreviousB = PreviousBladePoints[i + 1];

			const FVector3f& CurrentA = CurrentBladePoints[i];
			const FVector3f& CurrentB = CurrentBladePoints[i + 1];

			/**
			 * Quad:
			 *
			 * PreviousA -------- PreviousB
			 *       \            |
			 *        \           |
			 *         \          |
			 *          CurrentA - CurrentB
			 *
			 * Diagonal:
			 *
			 * PreviousA -> PreviousB -> CurrentA
			 * PreviousB -> CurrentB  -> CurrentA
			 */

			FSweptBladeTriangle TriangleA;

			TriangleA.A = PreviousA;
			TriangleA.B = PreviousB;
			TriangleA.C = CurrentA;

			TriangleA.BladeSampleA = i;
			TriangleA.BladeSampleB = i + 1;

			OutTriangles.Add(TriangleA);


			FSweptBladeTriangle TriangleB;

			TriangleB.A = PreviousB;
			TriangleB.B = CurrentB;
			TriangleB.C = CurrentA;

			TriangleB.BladeSampleA = i;
			TriangleB.BladeSampleB = i + 1;

			OutTriangles.Add(TriangleB);
		}
	}
}