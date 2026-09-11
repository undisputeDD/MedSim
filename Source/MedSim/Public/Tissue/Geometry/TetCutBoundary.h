#pragma once

#include "CoreMinimal.h"
#include "Tissue/Geometry/TetCutSurface.h"

struct FTetCutBoundaryVertex
{
    FVector3f Position = FVector3f::ZeroVector;

    // Exact index into FTetCutSurface::Vertices.
    int32 SurfaceVertexIndex = INDEX_NONE;

    // Tet faces containing this boundary vertex.
    // Empty = vertex is inside the tetrahedron.
    // 1 face = vertex lies on a tet face.
    // 2 faces = vertex lies on a tet edge.
    // 3 faces = vertex is a tet vertex.
    TArray<int32> TetFaceIndices;

    // Coordinates inside source tetrahedron.
    FVector4f Barycentric = FVector4f(0.0, 0.0, 0.0, 0.0);
};

struct FTetCutBoundaryEdge
{
    int32 VertexA = INDEX_NONE;
    int32 VertexB = INDEX_NONE;

    // Empty = edge is fully internal to the tetrahedron.
    // One face = edge lies on a tet face.
    // Two faces = edge lies on a tet edge.
    TArray<int32> TetFaceIndices;
};

struct FTetCutBoundary
{
    TArray<FTetCutBoundaryVertex> Vertices;
    TArray<FTetCutBoundaryEdge> Edges;

    TArray<TArray<int32>> Chains;
};

namespace TetCutBoundary
{
    bool Build(
        const FTetCutSurface& Surface,
        float TetFaceTolerance,
        FTetCutBoundary& OutBoundary);
}