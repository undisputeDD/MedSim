#include "Tissue/Geometry/CutPath.h"

namespace CutPath
{
	// DEPRECATED
	void BuildOrderedCutPoints(const TArray<FCutTetHit>& CutHits, TArray<FCutPathPoint>& OutPoints)
	{
		OutPoints.Reset();

		// ------------------------------------------------------------
		// Collect
		// ------------------------------------------------------------

		for (const FCutTetHit& TetHit : CutHits)
		{
			for (const FCutIntersection& Intersection : TetHit.Intersections)
			{
				FCutPathPoint Point;

				Point.Point = Intersection.Point;
				Point.TetId = TetHit.TetId;
				Point.BladeSampleIndex = Intersection.BladeSampleIndex;
				Point.SegmentT = Intersection.SegmentT;
				Point.Normal = Intersection.Normal;

				OutPoints.Add(Point);
			}
		}

		// ------------------------------------------------------------
		// Remove duplicates
		// ------------------------------------------------------------

		for (int32 i = OutPoints.Num() - 1; i >= 0; --i)
		{
			bool bDuplicate = false;

			for (int32 j = 0; j < i; ++j)
			{
				if (OutPoints[i].BladeSampleIndex == OutPoints[j].BladeSampleIndex &&
					OutPoints[i].Point.Equals(OutPoints[j].Point, 0.01f))
				{
					bDuplicate = true;
					break;
				}
			}

			if (bDuplicate)
			{
				OutPoints.RemoveAt(i);
			}
		}

		// ------------------------------------------------------------
		// Sort by blade sample, then by T
		// ------------------------------------------------------------

		OutPoints.Sort(
			[](const FCutPathPoint& A, const FCutPathPoint& B)
			{
				if (A.BladeSampleIndex != B.BladeSampleIndex)
				{
					return A.BladeSampleIndex < B.BladeSampleIndex;
				}

				return A.SegmentT < B.SegmentT;
			});
	}

}