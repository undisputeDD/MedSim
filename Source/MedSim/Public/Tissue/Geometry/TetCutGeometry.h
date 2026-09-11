#pragma once

#include "CoreMinimal.h"
#include "Tissue/Geometry/TetCutSurface.h"
#include "Tissue/Geometry/TetCutBoundary.h"

struct FTetCutGeometry
{
    int32 TetId = INDEX_NONE;

    FTetCutSurface Surface;

    FTetCutBoundary Boundary;

    bool IsValid() const
    {
        return TetId != INDEX_NONE && Surface.IsValid() && Boundary.Edges.Num() > 0;
    }
};