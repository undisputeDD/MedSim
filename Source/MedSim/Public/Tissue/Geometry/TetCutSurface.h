#pragma once

#include "CoreMinimal.h"
#include "Tissue/Geometry/TetCutData.h"

// ============================================================
// Cut surface
// ============================================================

struct FTetCutSurfaceVertex
{
    FVector3f Position = FVector3f::ZeroVector;

    FVector4f Barycentric = FVector4f(0.0, 0.0, 0.0, 0.0);
};

struct FTetCutSurfaceTriangle
{
    FIntVector Vertices = FIntVector(-1, -1, -1);

    int32 SourcePatchIndex = INDEX_NONE;

    FVector3f Normal = FVector3f::ZeroVector;
};

struct FTetCutSurface
{
    int32 TetId = INDEX_NONE;

    TArray<FTetCutSurfaceVertex> Vertices;

    TArray<FTetCutSurfaceTriangle> Triangles;

    float Area = 0.0f;
    float RelativeAreaError = 0.0f;

    bool IsValid() const
    {
        return Vertices.Num() >= 3 && Triangles.Num() > 0 && Area > KINDA_SMALL_NUMBER;
    }
};

// ============================================================
// API
// ============================================================

namespace TetCutSurface
{
    bool Build(
        const FTetCutData& TetCutData,
        float VertexMergeTolerance,
        FTetCutSurface& OutSurface);

    bool ValidateTopology(const FTetCutSurface& Surface);
}