#include "Tissue/Geometry/Utility.h"

namespace Utility
{
    // Möller–Trumbore Algorithm
    bool SegmentIntersectsTriangle(
        const FVector3f& SegmentStart,
        const FVector3f& SegmentEnd,
        const FVector3f& A,
        const FVector3f& B,
        const FVector3f& C,
        FVector3f& OutIntersection,
        float& OutT,
        FVector3f& OutNormal)
    {
        constexpr float Epsilon = 1e-6f;

        const FVector3f Direction = SegmentEnd - SegmentStart;
        const FVector3f Edge1 = B - A;
        const FVector3f Edge2 = C - A;

        const FVector3f PVec = FVector3f::CrossProduct(Direction, Edge2);

        const float Det = FVector3f::DotProduct(Edge1, PVec);

        if (FMath::Abs(Det) < Epsilon)
        {
            return false;
        }

        const float InvDet = 1.0f / Det;

        const FVector3f TVec = SegmentStart - A;

        const float U = FVector3f::DotProduct(TVec, PVec) * InvDet;

        if (U < -Epsilon || U > 1.0f + Epsilon)
        {
            return false;
        }

        const FVector3f QVec = FVector3f::CrossProduct(TVec, Edge1);

        const float V = FVector3f::DotProduct(Direction, QVec) * InvDet;

        if (V < -Epsilon ||
            U + V > 1.0f + Epsilon)
        {
            return false;
        }

        const float T = FVector3f::DotProduct(Edge2, QVec) * InvDet;

        if (T < -Epsilon || T > 1.0f + Epsilon)
        {
            return false;
        }

        // Point place along trajectory
        OutT = FMath::Clamp(T, 0.0f, 1.0f);

        OutIntersection = SegmentStart + Direction * OutT;

        OutNormal = FVector3f::CrossProduct(Edge1, Edge2).GetSafeNormal();

        return true;
    }
}