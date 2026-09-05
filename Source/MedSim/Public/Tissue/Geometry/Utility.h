#pragma once

#include "CoreMinimal.h"

namespace Utility
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
}