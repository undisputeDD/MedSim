#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueTopology.h"
#include "Tissue/Geometry/SweptBlade.h"

// Search for potential tets
namespace SweptBladeBroadPhase
{
	struct FTissueAABB
	{
		FVector3f Min;
		FVector3f Max;

		bool Intersects(const FTissueAABB& Other, float Epsilon = 0.0f) const
		{
			return
				Min.X <= Other.Max.X + Epsilon &&
				Max.X + Epsilon >= Other.Min.X &&

				Min.Y <= Other.Max.Y + Epsilon &&
				Max.Y + Epsilon >= Other.Min.Y &&

				Min.Z <= Other.Max.Z + Epsilon &&
				Max.Z + Epsilon >= Other.Min.Z;
		}
	};

	struct FTissueTetAABB
	{
		int32 TetId;
		FTissueAABB Bounds;
	};

	FTissueAABB ComputeTriangleAABB(const FSweptBladeTriangle& Triangle);
	FTissueAABB ComputeTetAABB(const FTissueTet& Tet, const TArray<FTissueVertex>& Vertices);

    void FindCandidateTetrahedra(
        const TArray<FSweptBladeTriangle>& SweptTriangles,
        const FTissueTopologySnapshot& TissueSnapshot,
        TArray<int32>& OutCandidateTetIds);
}