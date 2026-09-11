#include "Tissue/Geometry/TetCutBoundary.h"

namespace
{
    struct FSurfaceEdgeInfo
    {
        int32 VertexA = INDEX_NONE;
        int32 VertexB = INDEX_NONE;

        int32 TriangleCount = 0;
    };

    static uint64 MakeEdgeKey(int32 VertexA, int32 VertexB)
    {
        const uint32 A = static_cast<uint32>(FMath::Min(VertexA, VertexB));

        const uint32 B = static_cast<uint32>(FMath::Max(VertexA, VertexB));

        return (static_cast<uint64>(A) << 32) | static_cast<uint64>(B);
    }

    static bool ValidateSurfaceTriangles(const FTetCutSurface& Surface)
    {
        for (int32 TriangleIndex = 0; TriangleIndex < Surface.Triangles.Num(); ++TriangleIndex)
        {
            const FIntVector& Triangle = Surface.Triangles[TriangleIndex].Vertices;

            if (!Surface.Vertices.IsValidIndex(Triangle.X) ||
                !Surface.Vertices.IsValidIndex(Triangle.Y) ||
                !Surface.Vertices.IsValidIndex(Triangle.Z))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "TetCutBoundary: "
                        "Surface triangle %d has invalid vertex indices: "
                        "(%d,%d,%d)"
                    ),
                    TriangleIndex,
                    Triangle.X,
                    Triangle.Y,
                    Triangle.Z
                );

                return false;
            }

            if (Triangle.X == Triangle.Y ||
                Triangle.Y == Triangle.Z ||
                Triangle.Z == Triangle.X)
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "TetCutBoundary: "
                        "Surface triangle %d is degenerate"
                    ),
                    TriangleIndex
                );

                return false;
            }
        }

        return true;
    }

    static void BuildSurfaceEdgeMap(const FTetCutSurface& Surface, TMap<uint64, FSurfaceEdgeInfo>& OutEdges)
    {
        OutEdges.Reset();

        for (const FTetCutSurfaceTriangle& SurfaceTriangle : Surface.Triangles)
        {
            const int32 A = SurfaceTriangle.Vertices.X;
            const int32 B = SurfaceTriangle.Vertices.Y;
            const int32 C = SurfaceTriangle.Vertices.Z;

            const uint64 ABKey = MakeEdgeKey(A, B);
            const uint64 BCKey = MakeEdgeKey(B, C);
            const uint64 CAKey = MakeEdgeKey(C, A);

            FSurfaceEdgeInfo& AB = OutEdges.FindOrAdd(ABKey);
            AB.VertexA = FMath::Min(A, B);
            AB.VertexB = FMath::Max(A, B);
            ++AB.TriangleCount;

            FSurfaceEdgeInfo& BC = OutEdges.FindOrAdd(BCKey);
            BC.VertexA = FMath::Min(B, C);
            BC.VertexB = FMath::Max(B, C);
            ++BC.TriangleCount;

            FSurfaceEdgeInfo& CA = OutEdges.FindOrAdd(CAKey);
            CA.VertexA = FMath::Min(C, A);
            CA.VertexB = FMath::Max(C, A);
            ++CA.TriangleCount;
        }
    }

    static bool BuildBoundaryVerticesAndEdges(
        const FTetCutSurface& Surface,
        const TMap<uint64, FSurfaceEdgeInfo>& SurfaceEdges,
        FTetCutBoundary& OutBoundary)
    {
        OutBoundary.Vertices.Reset();
        OutBoundary.Edges.Reset();

        // Surface vertex index -> boundary vertex index.
        TArray<int32> SurfaceToBoundary;
        SurfaceToBoundary.Init(INDEX_NONE, Surface.Vertices.Num());

        // ----------------------------------------------------
        // Boundary vertices
        // ----------------------------------------------------

        for (const TPair<uint64, FSurfaceEdgeInfo>& Pair : SurfaceEdges)
        {
            const FSurfaceEdgeInfo& Edge = Pair.Value;

            if (Edge.TriangleCount != 1)
            {
                continue;
            }

            const int32 SurfaceVertexIndices[2] =
            {
                Edge.VertexA,
                Edge.VertexB
            };

            for (const int32 SurfaceVertexIndex : SurfaceVertexIndices)
            {
                if (SurfaceToBoundary[SurfaceVertexIndex] != INDEX_NONE)
                {
                    continue;
                }

                const FTetCutSurfaceVertex& SurfaceVertex = Surface.Vertices[SurfaceVertexIndex];

                FTetCutBoundaryVertex& BoundaryVertex = OutBoundary.Vertices.AddDefaulted_GetRef();

                BoundaryVertex.Position = SurfaceVertex.Position;
                BoundaryVertex.SurfaceVertexIndex = SurfaceVertexIndex;
                BoundaryVertex.Barycentric = SurfaceVertex.Barycentric;

                SurfaceToBoundary[SurfaceVertexIndex] = OutBoundary.Vertices.Num() - 1;
            }
        }

        // ----------------------------------------------------
        // Boundary edges
        // ----------------------------------------------------

        for (const TPair<uint64, FSurfaceEdgeInfo>& Pair : SurfaceEdges)
        {
            const FSurfaceEdgeInfo& Edge = Pair.Value;

            if (Edge.TriangleCount == 2)
            {
                // Internal surface edge.
                continue;
            }

            if (Edge.TriangleCount != 1)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "TetCutBoundary: "
                        "non-manifold surface edge "
                        "V=(%d,%d) TriangleCount=%d"
                    ),
                    Edge.VertexA,
                    Edge.VertexB,
                    Edge.TriangleCount
                );

                return false;
            }

            const int32 BoundaryA = SurfaceToBoundary[Edge.VertexA];
            const int32 BoundaryB = SurfaceToBoundary[Edge.VertexB];

            if (BoundaryA == INDEX_NONE || BoundaryB == INDEX_NONE)
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "TetCutBoundary: "
                        "boundary vertex mapping failed "
                        "SurfaceEdge=(%d,%d)"
                    ),
                    Edge.VertexA,
                    Edge.VertexB
                );

                return false;
            }

            if (BoundaryA == BoundaryB)
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "TetCutBoundary: "
                        "boundary edge collapsed "
                        "SurfaceVertex=(%d,%d)"
                    ),
                    Edge.VertexA,
                    Edge.VertexB
                );

                return false;
            }

            FTetCutBoundaryEdge& BoundaryEdge = OutBoundary.Edges.AddDefaulted_GetRef();

            BoundaryEdge.VertexA = BoundaryA;
            BoundaryEdge.VertexB = BoundaryB;
        }

        return true;
    }

    static void ClassifyBoundaryVerticesOnTetFaces(
        float Tolerance,
        FTetCutBoundary& OutBoundary)
    {
        for (FTetCutBoundaryVertex& Vertex : OutBoundary.Vertices)
        {
            Vertex.TetFaceIndices.Reset();

            const float Barycentric[4] =
            {
                Vertex.Barycentric.X,
                Vertex.Barycentric.Y,
                Vertex.Barycentric.Z,
                Vertex.Barycentric.W
            };

            for (int32 FaceIndex = 0; FaceIndex < 4; ++FaceIndex)
            {
                if (FMath::Abs(Barycentric[FaceIndex]) <= Tolerance)
                {
                    Vertex.TetFaceIndices.Add(FaceIndex);
                }
            }
        }
    }

    static void ClassifyBoundaryEdgesOnTetFaces(
        float Tolerance,
        FTetCutBoundary& OutBoundary)
    {
        for (FTetCutBoundaryEdge& Edge : OutBoundary.Edges)
        {
            Edge.TetFaceIndices.Reset();

            const FTetCutBoundaryVertex& VertexA = OutBoundary.Vertices[Edge.VertexA];
            const FTetCutBoundaryVertex& VertexB = OutBoundary.Vertices[Edge.VertexB];

            const float BarycentricA[4] =
            {
                VertexA.Barycentric.X,
                VertexA.Barycentric.Y,
                VertexA.Barycentric.Z,
                VertexA.Barycentric.W
            };

            const float BarycentricB[4] =
            {
                VertexB.Barycentric.X,
                VertexB.Barycentric.Y,
                VertexB.Barycentric.Z,
                VertexB.Barycentric.W
            };

            for (int32 FaceIndex = 0; FaceIndex < 4; ++FaceIndex)
            {
                const bool bAOnFace = FMath::Abs(BarycentricA[FaceIndex]) <= Tolerance;

                const bool bBOnFace = FMath::Abs(BarycentricB[FaceIndex]) <= Tolerance;

                if (bAOnFace && bBOnFace)
                {
                    Edge.TetFaceIndices.Add(FaceIndex);
                }
            }
        }
    }

    static void TraceChain(
        int32 StartVertex,
        const TArray<TArray<int32>>& VertexToEdges,
        TArray<bool>& VisitedEdges,
        const FTetCutBoundary& Boundary,
        TArray<int32>& Chain)
    {
        Chain.Reset();

        int32 CurrentVertex = StartVertex;

        TSet<int32> LocalVisitedVertices;

        while (true)
        {
            if (!Boundary.Vertices.IsValidIndex(CurrentVertex))
            {
                break;
            }

            if (LocalVisitedVertices.Contains(CurrentVertex))
            {
                break;
            }

            LocalVisitedVertices.Add(CurrentVertex);

            Chain.Add(CurrentVertex);

            const int32 Degree = VertexToEdges[CurrentVertex].Num();

            if (Degree == 0 || Degree > 2)
            {
                break;
            }

            // If we arrived at an endpoint,
            // the chain is complete.
            if (CurrentVertex != StartVertex && Degree == 1)
            {
                break;
            }

            int32 NextEdge = INDEX_NONE;

            for (const int32 EdgeIndex : VertexToEdges[CurrentVertex])
            {
                if (!VisitedEdges[EdgeIndex])
                {
                    NextEdge = EdgeIndex;
                    break;
                }
            }

            if (NextEdge == INDEX_NONE)
            {
                break;
            }

            VisitedEdges[NextEdge] = true;

            const FTetCutBoundaryEdge& Edge = Boundary.Edges[NextEdge];

            const int32 NextVertex = Edge.VertexA == CurrentVertex ? Edge.VertexB : Edge.VertexA;

            if (NextVertex == StartVertex)
            {
                break;
            }

            CurrentVertex = NextVertex;
        }
    }

    static bool BuildBoundaryChains(FTetCutBoundary& OutBoundary)
    {
        OutBoundary.Chains.Reset();

        // ----------------------------------------------------
        // Build VertexToEdges
        // ----------------------------------------------------

        TArray<TArray<int32>> VertexToEdges;
        VertexToEdges.SetNum(OutBoundary.Vertices.Num());

        for (int32 EdgeIndex = 0; EdgeIndex < OutBoundary.Edges.Num(); ++EdgeIndex)
        {
            const FTetCutBoundaryEdge& Edge = OutBoundary.Edges[EdgeIndex];

            if (!OutBoundary.Vertices.IsValidIndex(Edge.VertexA) || !OutBoundary.Vertices.IsValidIndex(Edge.VertexB))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "TetCutBoundary: "
                        "invalid boundary edge %d"
                    ),
                    EdgeIndex
                );

                return false;
            }

            VertexToEdges[Edge.VertexA].Add(EdgeIndex);
            VertexToEdges[Edge.VertexB].Add(EdgeIndex);
        }

        // ----------------------------------------------------
        // Validate graph
        // ----------------------------------------------------

        for (int32 VertexIndex = 0; VertexIndex < VertexToEdges.Num(); ++VertexIndex)
        {
            const int32 Degree = VertexToEdges[VertexIndex].Num();

            if (Degree > 2)
            {
                const FVector3f& P = OutBoundary.Vertices[VertexIndex].Position;

                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "TetCutBoundary: "
                        "boundary branch "
                        "Vertex=%d Degree=%d "
                        "Position=(%.6f %.6f %.6f)"
                    ),
                    VertexIndex,
                    Degree,
                    P.X,
                    P.Y,
                    P.Z
                );

                return false;
            }
        }

        TArray<bool> VisitedEdges;
        VisitedEdges.Init(false, OutBoundary.Edges.Num());

        // ----------------------------------------------------
       // Open chains
       // ----------------------------------------------------

        for (int32 VertexIndex = 0; VertexIndex < VertexToEdges.Num(); ++VertexIndex)
        {
            if (VertexToEdges[VertexIndex].Num() != 1)
            {
                continue;
            }

            TArray<int32> Chain;

            TraceChain(VertexIndex, VertexToEdges, VisitedEdges, OutBoundary, Chain);

            if (Chain.Num() >= 2)
            {
                OutBoundary.Chains.Add(MoveTemp(Chain));
            }
        }

        // ----------------------------------------------------
        // Closed loops
        // ----------------------------------------------------

        for (int32 EdgeIndex = 0; EdgeIndex < OutBoundary.Edges.Num(); ++EdgeIndex)
        {
            if (VisitedEdges[EdgeIndex])
            {
                continue;
            }

            TArray<int32> Chain;

            TraceChain(OutBoundary.Edges[EdgeIndex].VertexA, VertexToEdges, VisitedEdges, OutBoundary, Chain);

            if (Chain.Num() >= 2)
            {
                OutBoundary.Chains.Add(MoveTemp(Chain));
            }
        }

        // ----------------------------------------------------
        // Every edge must belong to a chain
        // ----------------------------------------------------

        for (int32 EdgeIndex = 0; EdgeIndex < VisitedEdges.Num(); ++EdgeIndex)
        {
            if (!VisitedEdges[EdgeIndex])
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "TetCutBoundary: "
                        "Edge %d was not consumed"
                    ),
                    EdgeIndex
                );

                return false;
            }
        }

        return true;
    }
}

bool TetCutBoundary::Build(
    const FTetCutSurface& Surface,
    float TetFaceTolerance,
    FTetCutBoundary& OutBoundary)
{
    OutBoundary = FTetCutBoundary{};

    if (!Surface.IsValid())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "TetCutBoundary::Build: "
                "invalid cut surface"
            )
        );

        return false;
    }

    if (!ValidateSurfaceTriangles(Surface))
    {
        return false;
    }

    // --------------------------------------------------------
    // 1. Build Surface Edge Map
    // --------------------------------------------------------

    TMap<uint64, FSurfaceEdgeInfo> SurfaceEdges;
    BuildSurfaceEdgeMap(Surface, SurfaceEdges);

    // --------------------------------------------------------
    // 2. Find boundary vertices and edges from surface topology
    // --------------------------------------------------------

    if (!BuildBoundaryVerticesAndEdges(Surface, SurfaceEdges, OutBoundary))
    {
        return false;
    }

    // --------------------------------------------------------
    // 3. Classify boundary vertices against tet faces
    // --------------------------------------------------------

    ClassifyBoundaryVerticesOnTetFaces(TetFaceTolerance, OutBoundary);

    // --------------------------------------------------------
    // 4. Classify boundary edges against tet faces
    // --------------------------------------------------------

    ClassifyBoundaryEdgesOnTetFaces(TetFaceTolerance, OutBoundary);

    // --------------------------------------------------------
    // 5. Build boundary chains
    // --------------------------------------------------------

    if (!BuildBoundaryChains(OutBoundary))
    {
        return false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "TetCutBoundary::Build "
            "Tet=%d "
            "SurfaceVertices=%d "
            "SurfaceTriangles=%d "
            "BoundaryVertices=%d "
            "BoundaryEdges=%d "
            "Chains=%d"
        ),
        Surface.TetId,
        Surface.Vertices.Num(),
        Surface.Triangles.Num(),
        OutBoundary.Vertices.Num(),
        OutBoundary.Edges.Num(),
        OutBoundary.Chains.Num()
    );

    return true;
}