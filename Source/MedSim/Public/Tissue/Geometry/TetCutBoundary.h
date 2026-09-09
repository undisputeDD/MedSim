#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueTopology.h"
#include "Tissue/Geometry/TetCutSurface.h"

struct FTetCutBoundaryVertex
{
    FVector3f Position;

    // Need to add mapping to surface vertices
    int32 SurfaceVertexIndex = INDEX_NONE;
};

struct FTetCutBoundaryEdge
{
    int32 VertexA = INDEX_NONE;
    int32 VertexB = INDEX_NONE;

    // Sometimes can be on 2 faces meaning boundary edge is on tet edge
    int32 TetFaceIndex = INDEX_NONE;

    // Debug
    int32 SourcePatchIndex = INDEX_NONE;
};

struct FTetCutBoundary
{
    TArray<FTetCutBoundaryVertex> Vertices;
    TArray<FTetCutBoundaryEdge> Edges;

    TArray<TArray<int32>> Chains;
};

namespace TetCutBoundary
{
    bool BuildTetCutBoundary(
        const FTetCutData& TetCutData,
        const FTissueTopologySnapshot& TissueSnapshot,
        float VertexMergeTolerance,
        FTetCutBoundary& OutBoundary);

    bool MapToSurface(
        const FTetCutSurface& Surface,
        FTetCutBoundary& Boundary,
        float MatchTolerance);
}