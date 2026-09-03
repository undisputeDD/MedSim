#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueCutTypes.h"

namespace TissueIntersection
{
    bool SegmentIntersectsTriangle(
        const FVector3f& SegmentStart,
        const FVector3f& SegmentEnd,
        const FVector3f& A,
        const FVector3f& B,
        const FVector3f& C,
        FVector3f& OutIntersection,
        float& OutT,
        FVector3f& OutNormal);

    bool IsPointInTetrahedron(
        const FVector3f& P,
        const FVector3f& V0,
        const FVector3f& V1,
        const FVector3f& V2,
        const FVector3f& V3);

    bool SegmentIntersectsTetrahedron(
        const FVector3f& SegmentStart,
        const FVector3f& SegmentEnd,
        int32 BladeSampleIndex,
        const FVector3f& V0,
        const FVector3f& V1,
        const FVector3f& V2,
        const FVector3f& V3,
        TArray<FCutIntersection>& OutCutIntersections);
}