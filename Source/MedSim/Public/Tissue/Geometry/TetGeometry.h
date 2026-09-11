#pragma once

#include "CoreMinimal.h"

namespace TetGeometry
{
    bool ComputeTetBarycentric(
        const FVector3f& Point,
        const FVector3f& V0,
        const FVector3f& V1,
        const FVector3f& V2,
        const FVector3f& V3,
        FVector4f& OutBarycentric,
        float Tolerance);

    double ComputeTetSignedVolume6(
        const FVector3f& V0,
        const FVector3f& V1,
        const FVector3f& V2,
        const FVector3f& V3);
}