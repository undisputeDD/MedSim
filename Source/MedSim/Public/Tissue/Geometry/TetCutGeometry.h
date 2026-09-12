#pragma once

#include "CoreMinimal.h"
#include "Tissue/Geometry/TetCutData.h"
#include "Tissue/Geometry/TetCutSurface.h"
#include "Tissue/Geometry/TetCutBoundary.h"

struct FTetCutGeometry
{
    int32 TetId = INDEX_NONE;

    FTetCutSurface Surface;

    FTetCutBoundary Boundary;

    bool IsValid() const
    {
        return TetId != INDEX_NONE && Surface.IsValid() && Boundary.Vertices.Num() > 0;
    }
};

namespace TetCutGeometry
{
    void Build(
        const TArray<FTetCutData>& TetCutData,
        const FTissueTopologySnapshot& TissueSnapshot,
        TArray<FTetCutGeometry>& TetCutGeometry,
        // For Debug
        const FTransform& TissueTransform,
        const bool bDebugSingleTet = false,
        const int32 DebugTetId = 0,
        const UWorld* World = nullptr);
}