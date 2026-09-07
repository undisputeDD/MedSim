#include "Tissue/Geometry/TetCutBoundary.h"

static int32 FindOrAddBoundaryVertex(
	const FVector3f& Position,
	TArray<FTetCutBoundaryVertex>& Vertices,
	float MergeTolerance)
{
	const float ToleranceSquared = FMath::Square(MergeTolerance);

	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		if (FVector3f::DistSquared(Vertices[Index].Position, Position) <= ToleranceSquared)
		{
			return Index;
		}
	}

	FTetCutBoundaryVertex& Vertex = Vertices.AddDefaulted_GetRef();

	Vertex.Position = Position;

	return Vertices.Num() - 1;
}

static bool IsPointOnTriangle(
	const FVector3f& Point,
	const FVector3f& A,
	const FVector3f& B,
	const FVector3f& C,
	float Tolerance)
{
    const FVector3f AB = B - A;
    const FVector3f AC = C - A;

    const FVector3f Normal = FVector3f::CrossProduct(AB, AC);

    const float NormalSizeSquared = Normal.SizeSquared();

    if (NormalSizeSquared <= SMALL_NUMBER)
    {
        return false;
    }

    const float NormalSize = FMath::Sqrt(NormalSizeSquared);

    // Distance from point to triangle plane.
    const float PlaneDistance = FMath::Abs(FVector3f::DotProduct(Point - A, Normal)) / NormalSize;

    if (PlaneDistance > Tolerance)
    {
        return false;
    }

    const FVector3f AP = Point - A;

    const float Dot00 = FVector3f::DotProduct(AC, AC);

    const float Dot01 = FVector3f::DotProduct(AC, AB);

    const float Dot02 = FVector3f::DotProduct(AC, AP);

    const float Dot11 = FVector3f::DotProduct(AB, AB);

    const float Dot12 = FVector3f::DotProduct(AB, AP);

    const float Denominator = Dot00 * Dot11 - Dot01 * Dot01;

    if (FMath::Abs(Denominator) <= SMALL_NUMBER)
    {
        return false;
    }

    const float InvDenominator = 1.0f / Denominator;

    const float U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenominator;

    const float V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenominator;

    return U >= -Tolerance && V >= -Tolerance && (U + V) <= 1.0f + Tolerance;
}

static uint64 MakeEdgeKey(int32 VertexA, int32 VertexB)
{
    const uint32 A = static_cast<uint32>(FMath::Min(VertexA, VertexB));

    const uint32 B = static_cast<uint32>(FMath::Max(VertexA, VertexB));

    return (static_cast<uint64>(A) << 32) | static_cast<uint64>(B);
}

struct FTetFace
{
    int32 Index = INDEX_NONE;

    FIntVector Vertices;
};

static void BuildTetFaces(const FTissueTet & Tet, TArray<FTetFace>&OutFaces)
{
    OutFaces.Reset();
    OutFaces.Reserve(4);

    // Face opposite V0
    OutFaces.Add({
        0,
        FIntVector(
        Tet.Vertices.Y,
        Tet.Vertices.Z,
        Tet.Vertices.W)
    });

    // Face opposite V1
    OutFaces.Add({
        1,
        FIntVector(
        Tet.Vertices.X,
        Tet.Vertices.W,
        Tet.Vertices.Z)
    });

    // Face opposite V2
    OutFaces.Add({
        2,
        FIntVector(
        Tet.Vertices.X,
        Tet.Vertices.Y,
        Tet.Vertices.W)
    });

    // Face opposite V3
    OutFaces.Add({
        3,
        FIntVector(
        Tet.Vertices.X,
        Tet.Vertices.Z,
        Tet.Vertices.Y)
    });
}

static void BuildBoundaryChains(FTetCutBoundary& OutBoundary)
{
    // Build adjacency
    TArray<TArray<int32>> VertexToEdges;
    VertexToEdges.SetNum(OutBoundary.Vertices.Num());
    for (int32 EdgeIndex = 0; EdgeIndex < OutBoundary.Edges.Num(); ++EdgeIndex)
    {
        const FTetCutBoundaryEdge& Edge = OutBoundary.Edges[EdgeIndex];

        VertexToEdges[Edge.VertexA].Add(EdgeIndex);
        VertexToEdges[Edge.VertexB].Add(EdgeIndex);
    }

    // Main algo
    TArray<bool> VisitedEdges;
    VisitedEdges.Init(false, OutBoundary.Edges.Num());
    for (int32 StartEdgeIndex = 0; StartEdgeIndex < OutBoundary.Edges.Num(); ++StartEdgeIndex)
    {
        if (VisitedEdges[StartEdgeIndex])
        {
            continue;
        }

        const FTetCutBoundaryEdge& StartEdge = OutBoundary.Edges[StartEdgeIndex];

        TArray<int32> Chain;
        int32 StartVertex = StartEdge.VertexA;
        int32 PreviousEdge = INDEX_NONE;
        int32 CurrentVertex = StartVertex;

        Chain.Add(CurrentVertex);

        while (true)
        {
            int32 NextEdge = INDEX_NONE;

            for (const int32 EdgeIndex : VertexToEdges[CurrentVertex])
            {
                if (VisitedEdges[EdgeIndex])
                {
                    continue;
                }

                if (EdgeIndex == PreviousEdge)
                {
                    continue;
                }

                NextEdge = EdgeIndex;
                break;
            }

            if (NextEdge == INDEX_NONE)
            {
                break;
            }

            VisitedEdges[NextEdge] = true;

            const FTetCutBoundaryEdge& Edge = OutBoundary.Edges[NextEdge];

            const int32 NextVertex = Edge.VertexA == CurrentVertex ? Edge.VertexB : Edge.VertexA;

            PreviousEdge = NextEdge;
            CurrentVertex = NextVertex;

            if (CurrentVertex == StartVertex)
            {
                break;
            }

            Chain.Add(CurrentVertex);
        }

        if (Chain.Num() >= 2)
        {
            OutBoundary.Chains.Add(MoveTemp(Chain));
        }
    }
}

bool TetCutBoundary::BuildTetCutBoundary(
    const FTetCutData& TetCutData,
    const FTissueTopologySnapshot& TissueSnapshot,
    float VertexMergeTolerance,
    FTetCutBoundary& OutBoundary)
{
    OutBoundary = FTetCutBoundary{};

    if (!TetCutData.bNeedsCut || TetCutData.Patches.IsEmpty())
    {
        return false;
    }

    // Build FTetCutBoundary.Vertices and FTetCutBoundary.Edges
    const FTissueTet& Tet = TissueSnapshot.Tetrahedra[TetCutData.TetId];
    const TArray<FTissueVertex>& TissueVertices = TissueSnapshot.Vertices;

    TArray<FTetFace> Faces;
    BuildTetFaces(Tet, Faces);

    TMap<uint64, int32> EdgeToIndex;
    for (int32 PatchIndex = 0; PatchIndex < TetCutData.Patches.Num(); ++PatchIndex)
    {
        const FTetCutPatch& Patch = TetCutData.Patches[PatchIndex];

        const TArray<FVector3f>& Polygon = Patch.Polygon;

        if (Polygon.Num() < 3)
        {
            continue;
        }

        for (int32 PointIndex = 0; PointIndex < Polygon.Num(); ++PointIndex)
        {
            const FVector3f& A = Polygon[PointIndex];
            const FVector3f& B = Polygon[(PointIndex + 1) % Polygon.Num()];

            for (const FTetFace& Face : Faces)
            {
                const FVector3f& FaceA = TissueVertices[Face.Vertices.X].CurrentPosition;

                const FVector3f& FaceB = TissueVertices[Face.Vertices.Y].CurrentPosition;

                const FVector3f& FaceC = TissueVertices[Face.Vertices.Z].CurrentPosition;

                const bool bAOnFace = IsPointOnTriangle(A, FaceA, FaceB, FaceC, VertexMergeTolerance);

                if (!bAOnFace)
                {
                    continue;
                }

                const bool bBOnFace = IsPointOnTriangle(B, FaceA, FaceB, FaceC, VertexMergeTolerance);

                if (!bBOnFace)
                {
                    continue;
                }

                // A-B lies on this tet face.

                const int32 VertexA = FindOrAddBoundaryVertex(A, OutBoundary.Vertices, VertexMergeTolerance);
                const int32 VertexB = FindOrAddBoundaryVertex(B, OutBoundary.Vertices, VertexMergeTolerance);

                if (VertexA == VertexB)
                {
                    continue;
                }

                const uint64 EdgeKey = MakeEdgeKey(VertexA, VertexB);

                if (EdgeToIndex.Contains(EdgeKey))
                {
                    continue;
                }

                FTetCutBoundaryEdge& Edge = OutBoundary.Edges.AddDefaulted_GetRef();

                Edge.VertexA = VertexA;
                Edge.VertexB = VertexB;
                Edge.TetFaceIndex = Face.Index;

                EdgeToIndex.Add(EdgeKey, OutBoundary.Edges.Num() - 1);
            }
        }
    }

    // Build FTetCutBoundary.Chains
    BuildBoundaryChains(OutBoundary);

    return OutBoundary.Edges.Num() > 0 && OutBoundary.Chains.Num() > 0;
}