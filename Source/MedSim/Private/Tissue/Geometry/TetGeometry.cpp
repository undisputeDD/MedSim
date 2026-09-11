#include "Tissue/Geometry/TetGeometry.h"


namespace
{
    static double ScalarTripleProduct(const FVector3f& A, const FVector3f& B, const FVector3f& C)
    {
        return static_cast<double>(FVector3f::DotProduct(A, FVector3f::CrossProduct(B, C)));
    }
}

bool TetGeometry::ComputeTetBarycentric(
    const FVector3f& Point,
    const FVector3f& V0,
    const FVector3f& V1,
    const FVector3f& V2,
    const FVector3f& V3,
    FVector4f& OutBarycentric,
    float Tolerance)
{
    const FVector3f A = V1 - V0;
    const FVector3f B = V2 - V0;
    const FVector3f C = V3 - V0;
    const FVector3f P = Point - V0;

    const double Denominator = ScalarTripleProduct(A, B, C);

    if (FMath::Abs(Denominator) <= SMALL_NUMBER)
    {
        OutBarycentric = FVector4f(0.0, 0.0, 0.0, 0.0);
        return false;
    }

    const double W1 = ScalarTripleProduct(P, B, C) / Denominator;

    const double W2 = ScalarTripleProduct(A, P, C) / Denominator;

    const double W3 = ScalarTripleProduct(A, B, P) / Denominator;

    const double W0 = 1.0 - W1 - W2 - W3;

    OutBarycentric = FVector4f(
        static_cast<float>(W0),
        static_cast<float>(W1),
        static_cast<float>(W2),
        static_cast<float>(W3)
    );

    const bool bInside =
        W0 >= -Tolerance &&
        W1 >= -Tolerance &&
        W2 >= -Tolerance &&
        W3 >= -Tolerance &&
        W0 <= 1.0 + Tolerance &&
        W1 <= 1.0 + Tolerance &&
        W2 <= 1.0 + Tolerance &&
        W3 <= 1.0 + Tolerance;

    return bInside;
}

double TetGeometry::ComputeTetSignedVolume6(
    const FVector3f& V0,
    const FVector3f& V1,
    const FVector3f& V2,
    const FVector3f& V3)
{
    const FVector3f A = V1 - V0;
    const FVector3f B = V2 - V0;
    const FVector3f C = V3 - V0;

    return ScalarTripleProduct(A, B, C);
}
