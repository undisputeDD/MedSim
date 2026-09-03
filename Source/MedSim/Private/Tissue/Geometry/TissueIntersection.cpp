#include "Tissue/Geometry/TissueIntersection.h"

namespace TissueIntersection
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

    // Barycentric coords
    bool IsPointInTetrahedron(
        const FVector3f& P,
        const FVector3f& V0,
        const FVector3f& V1,
        const FVector3f& V2,
        const FVector3f& V3)
    {
        const FVector3f D0 = V1 - V0;
        const FVector3f D1 = V2 - V0;
        const FVector3f D2 = V3 - V0;
        const FVector3f DP = P - V0;

        const float Det = FVector3f::DotProduct(D0, FVector3f::CrossProduct(D1, D2));

        constexpr float Epsilon = 1e-6f;

        if (FMath::Abs(Det) < Epsilon)
        {
            return false;
        }

        const float InvDet = 1.0f / Det;

        const float U = FVector3f::DotProduct(DP, FVector3f::CrossProduct(D1, D2)) * InvDet;

        const float V = FVector3f::DotProduct(D0, FVector3f::CrossProduct(DP, D2)) * InvDet;

        const float W = FVector3f::DotProduct(D0, FVector3f::CrossProduct(D1, DP)) * InvDet;

        const float X = 1.0f - U - V - W;

        const float Tolerance = 1e-4f;

        return U >= -Tolerance && V >= -Tolerance && W >= -Tolerance && X >= -Tolerance;
    }

    bool SegmentIntersectsTetrahedron(
        const FVector3f& SegmentStart,
        const FVector3f& SegmentEnd,
        int32 BladeSampleIndex,
        const FVector3f& V0,
        const FVector3f& V1,
        const FVector3f& V2,
        const FVector3f& V3,
        TArray<FCutIntersection>& OutCutIntersections)
    {
        OutCutIntersections.Reset();

        // ------------------------------------------------------------
        // 1. Start / end inside
        // ------------------------------------------------------------

        const bool bStartInside = IsPointInTetrahedron(SegmentStart, V0, V1, V2, V3);
        const bool bEndInside = IsPointInTetrahedron(SegmentEnd, V0, V1, V2, V3);

        // ------------------------------------------------------------
        // 2. Segment vs 4 tetrahedron faces
        // ------------------------------------------------------------

        struct FTriangle
        {
            FVector3f A;
            FVector3f B;
            FVector3f C;
            FVector3f OppositeVertex;
            int32 FaceIndex;
        };

        const FTriangle Faces[4] =
        {
            { V0, V1, V2, V3, 0 },
            { V0, V1, V3, V2, 1 },
            { V0, V2, V3, V1, 2 },
            { V1, V2, V3, V0, 3 }
        };

        for (const FTriangle& Face : Faces)
        {
            FVector3f Intersection;
            FVector3f FaceNormal;
            float T = 0.0f;

            if (!SegmentIntersectsTriangle(
                SegmentStart,
                SegmentEnd,
                Face.A,
                Face.B,
                Face.C,
                Intersection,
                T,
                FaceNormal))
            {
                continue;
            }

            // --------------------------------------------------------
            // Make normal point OUT of tetrahedron
            // --------------------------------------------------------

            const FVector3f ToOppositeVertex = Face.OppositeVertex - Intersection;

            if (FVector3f::DotProduct(FaceNormal, ToOppositeVertex) > 0.0f)
            {
                FaceNormal *= -1.0f;
            }

            // --------------------------------------------------------
            // Avoid duplicate point when intersection lies
            // exactly on edge / vertex shared by two faces
            // --------------------------------------------------------

            bool bAlreadyExists = false;

            for (const FCutIntersection& ExistingIntersection : OutCutIntersections)
            {
                if (ExistingIntersection.Point.Equals(Intersection, 0.01f))
                {
                    bAlreadyExists = true;
                    break;
                }
            }

            if (bAlreadyExists)
            {
                continue;
            }

            // --------------------------------------------------------
            // Store complete intersection event
            // --------------------------------------------------------

            FCutIntersection& NewIntersection = OutCutIntersections.AddDefaulted_GetRef();

            NewIntersection.Point = Intersection;
            NewIntersection.BladeSampleIndex = BladeSampleIndex;
            NewIntersection.SegmentT = T;
            NewIntersection.TetFaceIndex = Face.FaceIndex;
            NewIntersection.Normal = FaceNormal;
        }

        return bStartInside || bEndInside || OutCutIntersections.Num() > 0;
    }
}