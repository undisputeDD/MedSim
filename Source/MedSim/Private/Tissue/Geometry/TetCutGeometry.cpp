#include "Tissue/Geometry/TetCutGeometry.h"
#include "Tissue/Geometry/TetGeometry.h"

void TetCutGeometry::Build(
    const TArray<FTetCutData>& TetCutData,
    const FTissueTopologySnapshot& TissueSnapshot,
    TArray<FTetCutGeometry>& TetCutGeometry,
    // For Debug
    const FTransform& TissueTransform,
    const bool bDebugSingleTet,
    const int32 DebugTetId,
    const UWorld* World)
{
    for (const FTetCutData& TetCut : TetCutData)
    {
        if (!TetCut.bNeedsCut)
        {
            continue;
        }

        FTetCutGeometry Geometry;
        Geometry.TetId = TetCut.TetId;

        // ----------------------------------------------------
        // 6.1 Build cut surface and validate it
        // ----------------------------------------------------

        constexpr float SurfaceVertexMergeTolerance = 0.01f;
        if (!TetCutSurface::Build(TetCut, SurfaceVertexMergeTolerance, Geometry.Surface))
        {
            continue;
        }

        // ----------------------------------------------------
        // 6.2 Compute barycentric coordinates
        // ----------------------------------------------------

        bool bBarycentricValid = TetGeometry::AssignSurfaceBarycentrics(TissueSnapshot, TetCut, Geometry);
        if (!bBarycentricValid)
        {
            continue;
        }

        // ----------------------------------------------------
        // 6.3 Build boundary from surface topology
        // ----------------------------------------------------

        constexpr float TetFaceClassificationTolerance = 0.01f;
        if (!TetCutBoundary::Build(Geometry.Surface, TetFaceClassificationTolerance, Geometry.Boundary))
        {
            continue;
        }

        // ----------------------------------------------------
        // 6.4 Debug
        // ----------------------------------------------------

        const FTissueTet& Tet = TissueSnapshot.Tetrahedra[TetCut.TetId];

        const FVector3f V0 = TissueSnapshot.Vertices[Tet.Vertices.X].CurrentPosition;
        const FVector3f V1 = TissueSnapshot.Vertices[Tet.Vertices.Y].CurrentPosition;
        const FVector3f V2 = TissueSnapshot.Vertices[Tet.Vertices.Z].CurrentPosition;
        const FVector3f V3 = TissueSnapshot.Vertices[Tet.Vertices.W].CurrentPosition;

        if (bDebugSingleTet && TetCut.TetId == DebugTetId)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "TetCutSurface topology valid: "
                    "Tet=%d"
                ),
                Geometry.Surface.TetId
            );

            const FVector3f TetA = V0;
            const FVector3f TetB = V1;
            const FVector3f TetC = V2;
            const FVector3f TetD = V3;

            // ------------------------------------------------
            // Tetrahedron
            // ------------------------------------------------

            const TArray<TPair<FVector3f, FVector3f>> TetEdges =
            {
                { TetA, TetB },
                { TetA, TetC },
                { TetA, TetD },
                { TetB, TetC },
                { TetB, TetD },
                { TetC, TetD }
            };

            for (const TPair<FVector3f, FVector3f>& Edge : TetEdges)
            {
                DrawDebugLine(
                    World,
                    TissueTransform.TransformPosition(FVector(Edge.Key)),
                    TissueTransform.TransformPosition(FVector(Edge.Value)),
                    FColor::Yellow,
                    true,
                    20.0f,
                    0,
                    0.08f
                );
            }

            // ------------------------------------------------
            // Cut surface
            // ------------------------------------------------

            for (const FTetCutSurfaceTriangle& Triangle : Geometry.Surface.Triangles)
            {
                const FVector3f A = Geometry.Surface.Vertices[Triangle.Vertices.X].Position;
                const FVector3f B = Geometry.Surface.Vertices[Triangle.Vertices.Y].Position;
                const FVector3f C = Geometry.Surface.Vertices[Triangle.Vertices.Z].Position;

                const FVector WorldA = TissueTransform.TransformPosition(FVector(A));
                const FVector WorldB = TissueTransform.TransformPosition(FVector(B));
                const FVector WorldC = TissueTransform.TransformPosition(FVector(C));

                DrawDebugLine(
                    World,
                    WorldA,
                    WorldB,
                    FColor::Green,
                    true,
                    20.0f,
                    0,
                    0.06f
                );

                DrawDebugLine(
                    World,
                    WorldB,
                    WorldC,
                    FColor::Green,
                    true,
                    20.0f,
                    0,
                    0.06f
                );

                DrawDebugLine(
                    World,
                    WorldC,
                    WorldA,
                    FColor::Green,
                    true,
                    20.0f,
                    0,
                    0.06f
                );
            }

            // ------------------------------------------------
            // Boundary
            // ------------------------------------------------

            for (const FTetCutBoundaryEdge& Edge : Geometry.Boundary.Edges)
            {
                const FVector3f& A = Geometry.Boundary.Vertices[Edge.VertexA].Position;
                const FVector3f& B = Geometry.Boundary.Vertices[Edge.VertexB].Position;

                DrawDebugLine(
                    World,
                    TissueTransform.TransformPosition(FVector(A)),
                    TissueTransform.TransformPosition(FVector(B)),
                    FColor::Red,
                    true,
                    20.0f,
                    0,
                    0.12f
                );
            }

            // ------------------------------------------------
            // Boundary vertices
            // ------------------------------------------------

            for (int32 BoundaryVertexIndex = 0; BoundaryVertexIndex < Geometry.Boundary.Vertices.Num(); ++BoundaryVertexIndex)
            {
                const FTetCutBoundaryVertex& Vertex = Geometry.Boundary.Vertices[BoundaryVertexIndex];
                const FVector WorldPosition = TissueTransform.TransformPosition(FVector(Vertex.Position));

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BoundaryVertex[%d] "
                        "Surface=%d "
                        "Position=(%.6f %.6f %.6f) "
                        "Bary=(%.6f %.6f %.6f %.6f) "
                        "Faces=%d"
                    ),
                    BoundaryVertexIndex,
                    Vertex.SurfaceVertexIndex,
                    Vertex.Position.X,
                    Vertex.Position.Y,
                    Vertex.Position.Z,
                    Vertex.Barycentric.X,
                    Vertex.Barycentric.Y,
                    Vertex.Barycentric.Z,
                    Vertex.Barycentric.W,
                    Vertex.TetFaceIndices.Num()
                );

                DrawDebugSphere(
                    World,
                    WorldPosition,
                    0.12f,
                    8,
                    FColor::Blue,
                    true,
                    20.0f,
                    0,
                    0.02f
                );
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "DEBUG Tet=%d "
                    "SurfaceVertices=%d "
                    "SurfaceTriangles=%d "
                    "BoundaryVertices=%d "
                    "BoundaryEdges=%d "
                    "Chains=%d"
                ),
                TetCut.TetId,
                Geometry.Surface.Vertices.Num(),
                Geometry.Surface.Triangles.Num(),
                Geometry.Boundary.Vertices.Num(),
                Geometry.Boundary.Edges.Num(),
                Geometry.Boundary.Chains.Num()
            );

            for (int32 EdgeIndex = 0; EdgeIndex < Geometry.Boundary.Edges.Num(); ++EdgeIndex)
            {
                const FTetCutBoundaryEdge& Edge = Geometry.Boundary.Edges[EdgeIndex];

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BoundaryEdge[%d] "
                        "V=(%d,%d) "
                        "Faces=%d"
                    ),
                    EdgeIndex,
                    Edge.VertexA,
                    Edge.VertexB,
                    Edge.TetFaceIndices.Num()
                );
            }

            for (int32 ChainIndex = 0; ChainIndex < Geometry.Boundary.Chains.Num(); ++ChainIndex)
            {
                FString ChainString;

                for (const int32 VertexIndex : Geometry.Boundary.Chains[ChainIndex])
                {
                    if (!ChainString.IsEmpty())
                    {
                        ChainString += TEXT(" -> ");
                    }

                    ChainString += FString::FromInt(VertexIndex);
                }

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BoundaryChain[%d]: %s"
                    ),
                    ChainIndex,
                    *ChainString
                );
            }
        }

        // ----------------------------------------------------
        // 6.5 Store
        // ----------------------------------------------------

        TetCutGeometry.Add(MoveTemp(Geometry));
    }
}
