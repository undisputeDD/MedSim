#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueTopology.h"
#include "Tissue/Geometry/TetCutGeometry.h"

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

    bool AssignSurfaceBarycentrics(
        const FTissueTopologySnapshot& TissueSnapshot,
        const FTetCutData& TetCut,
        FTetCutGeometry& Geometry);

    uint64 MakeEdgeKey(int32 VertexA, int32 VertexB);
}