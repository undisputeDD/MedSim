#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueTopology.h"
#include "Tissue/Geometry/TetCutSurface.h"
#include "Tissue/Geometry/TetCutBoundary.h"

struct FTetSplitVertex
{
    FVector3f CurrentPosition = FVector3f::ZeroVector;
    FVector3f RestPosition = FVector3f::ZeroVector;

    FVector4f Barycentric = FVector4f(0.0, 0.0, 0.0, 0.0);

    float Mass = 0.0f;

    int32 SourceOriginalVertex = INDEX_NONE;
    int32 SourceCutSurfaceVertex = INDEX_NONE;
};

struct FTetSplitTet
{
    FIntVector4 Vertices = FIntVector4(0, 0, 0, 0);

    uint8 Side = 0;
};

struct FTetSplitInput
{
    int32 TetId = INDEX_NONE;

    FTissueTet OriginalTet;

    FTetCutSurface CutSurface;

    FTetCutBoundary CutBoundary;
};

struct FTetSplitResult
{
    bool bSuccess = false;

    TArray<FTetSplitVertex> Vertices;

    TArray<FTetSplitTet> Tetrahedra;
};
